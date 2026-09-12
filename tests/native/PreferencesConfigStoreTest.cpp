#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <string>

#include "modules/config/storage/ConfigSchema.h"
#include "modules/config/storage/PreferencesConfigStore.h"

namespace {
int failures = 0;

void expect(bool condition, const char *message) {
  if (condition) return;
  ++failures;
  std::cerr << "FAIL: " << message << '\n';
}

struct StoredValue {
  enum class Type { Number, String } type = Type::Number;
  uint32_t number = 0;
  std::string text;
};

class FakePreferencesBackend : public PreferencesBackend {
 public:
  std::map<std::string, std::map<std::string, StoredValue>> namespaces;
  std::set<std::string> inspectionErrors;
  int failMutationAt = -1;
  int mutationCount = 0;
  std::string corruptWriteKey;

  PreferencesNamespaceState inspectNamespace(const char *name) override {
    if (inspectionErrors.count(name) != 0) {
      return PreferencesNamespaceState::StorageError;
    }
    return namespaces.count(name) == 0
               ? PreferencesNamespaceState::Missing
               : PreferencesNamespaceState::Exists;
  }

  bool open(const char *name, bool readOnly) override {
    if (readOnly && namespaces.count(name) == 0) {
      return false;
    }
    currentNamespace_ = name;
    readOnly_ = readOnly;
    if (!readOnly) namespaces[currentNamespace_];
    return true;
  }

  void close() override {
    currentNamespace_.clear();
    readOnly_ = false;
  }

  bool clear() override {
    return mutate([this]() { namespaces[currentNamespace_].clear(); });
  }

  bool hasKey(const char *key) const override {
    const auto namespaceIt = namespaces.find(currentNamespace_);
    return namespaceIt != namespaces.end() &&
           namespaceIt->second.count(key) != 0;
  }

  uint8_t getUChar(const char *key, uint8_t fallback) const override {
    return static_cast<uint8_t>(getNumber(key, fallback));
  }

  bool getUCharChecked(const char *key, uint8_t &value) const override {
    const StoredValue *stored = findValue(key);
    if (stored == nullptr || stored->type != StoredValue::Type::Number ||
        stored->number > 0xffU) return false;
    value = static_cast<uint8_t>(stored->number);
    return true;
  }

  uint16_t getUShort(const char *key, uint16_t fallback) const override {
    return static_cast<uint16_t>(getNumber(key, fallback));
  }

  bool getBool(const char *key, bool fallback) const override {
    return getNumber(key, fallback ? 1U : 0U) != 0;
  }

  bool getString(const char *key,
                 char *target,
                 size_t targetSize) const override {
    const StoredValue *value = findValue(key);
    if (value == nullptr || value->type != StoredValue::Type::String ||
        value->text.length() >= targetSize) {
      return false;
    }
    strlcpy(target, value->text.c_str(), targetSize);
    return true;
  }

  bool putUChar(const char *key, uint8_t value) override {
    return putNumber(key, value);
  }

  bool putUShort(const char *key, uint16_t value) override {
    return putNumber(key, value);
  }

  bool putBool(const char *key, bool value) override {
    return putNumber(key, value ? 1U : 0U);
  }

  bool putString(const char *key, const char *value) override {
    return mutate([this, key, value]() {
      StoredValue stored;
      stored.type = StoredValue::Type::String;
      stored.text = corruptWriteKey == key ? "corrupted" : value;
      namespaces[currentNamespace_][key] = stored;
    });
  }

  void eraseKey(const char *name, const char *key) {
    namespaces[name].erase(key);
  }

  void setUShort(const char *name, const char *key, uint16_t value) {
    StoredValue stored;
    stored.type = StoredValue::Type::Number;
    stored.number = value;
    namespaces[name][key] = stored;
  }

  uint8_t activeSlot() const {
    const auto namespaceIt = namespaces.find("devcfg_meta");
    if (namespaceIt == namespaces.end()) return 0xff;
    const auto keyIt = namespaceIt->second.find("active");
    return keyIt == namespaceIt->second.end()
               ? 0xff
               : static_cast<uint8_t>(keyIt->second.number);
  }

 private:
  std::string currentNamespace_;
  bool readOnly_ = false;

  template <typename Mutation>
  bool mutate(Mutation mutation) {
    if (currentNamespace_.empty() || readOnly_) return false;
    const int current = mutationCount++;
    if (current == failMutationAt) return false;
    mutation();
    return true;
  }

  bool putNumber(const char *key, uint32_t value) {
    return mutate([this, key, value]() {
      StoredValue stored;
      stored.type = StoredValue::Type::Number;
      stored.number = corruptWriteKey == key ? value + 1U : value;
      namespaces[currentNamespace_][key] = stored;
    });
  }

  const StoredValue *findValue(const char *key) const {
    const auto namespaceIt = namespaces.find(currentNamespace_);
    if (namespaceIt == namespaces.end()) return nullptr;
    const auto keyIt = namespaceIt->second.find(key);
    return keyIt == namespaceIt->second.end() ? nullptr : &keyIt->second;
  }

  uint32_t getNumber(const char *key, uint32_t fallback) const {
    const StoredValue *value = findValue(key);
    return value == nullptr || value->type != StoredValue::Type::Number
               ? fallback
               : value->number;
  }
};

bool configsEqual(const DeviceConfig &left, const DeviceConfig &right) {
  return left.schemaVersion == right.schemaVersion &&
         left.wifiMode == right.wifiMode &&
         left.wifiTxDbm == right.wifiTxDbm &&
         strcmp(left.hostname, right.hostname) == 0 &&
         strcmp(left.staSsid, right.staSsid) == 0 &&
         strcmp(left.staPassword, right.staPassword) == 0 &&
         left.staSecurity == right.staSecurity &&
         left.staIpMode == right.staIpMode &&
         strcmp(left.staIpAddress, right.staIpAddress) == 0 &&
         strcmp(left.staIpGateway, right.staIpGateway) == 0 &&
         strcmp(left.staIpNetmask, right.staIpNetmask) == 0 &&
         strcmp(left.staDns1, right.staDns1) == 0 &&
         strcmp(left.staDns2, right.staDns2) == 0 &&
         strcmp(left.apSsid, right.apSsid) == 0 &&
         left.apPasswordEnabled == right.apPasswordEnabled &&
         left.apIpMode == right.apIpMode &&
         strcmp(left.apIpAddress, right.apIpAddress) == 0 &&
         strcmp(left.apIpNetmask, right.apIpNetmask) == 0 &&
         left.fallbackToAp == right.fallbackToAp &&
         strcmp(left.adminUsername, right.adminUsername) == 0 &&
         strcmp(left.adminPassword, right.adminPassword) == 0;
}

DeviceConfig namedConfig(const char *hostname, const char *password) {
  DeviceConfig config = defaultDeviceConfig();
  strlcpy(config.hostname, hostname, sizeof(config.hostname));
  strlcpy(config.adminPassword, password, sizeof(config.adminPassword));
  return config;
}

void testEmptyActiveAndFallbackSlots() {
  FakePreferencesBackend backend;
  PreferencesConfigStore store(backend);
  DeviceConfig loaded;

  expect(store.load(loaded).code == ResultCode::NotFound,
         "empty partition should report missing config");
  expect(backend.namespaces.empty() && backend.mutationCount == 0,
         "empty load must not create or overwrite namespaces");

  const DeviceConfig first = namedConfig("first-host", "FirstPass1");
  expect(store.save(first).ok() && backend.activeSlot() == 0,
         "first save should commit slot A");
  expect(store.load(loaded).ok() && configsEqual(loaded, first),
         "active slot A should load exactly");

  const DeviceConfig second = namedConfig("second-host", "SecondPass1");
  expect(store.save(second).ok() && backend.activeSlot() == 1,
         "second save should commit slot B");
  expect(store.load(loaded).ok() && configsEqual(loaded, second),
         "active slot B should load exactly");

  backend.eraseKey("devcfg_b", "wifi_mode");
  expect(store.load(loaded).ok() && configsEqual(loaded, first),
         "corrupted active slot should fall back to the previous valid slot");
  expect(backend.activeSlot() == 0,
         "fallback load should repair the active marker");
}

void testEveryMutationFailurePreservesLastValidConfig() {
  constexpr int kSaveMutationPoints = 23;
  const DeviceConfig initial = namedConfig("stable-host", "StablePass1");
  const DeviceConfig candidate = namedConfig("candidate-host", "Candidate1");

  for (int failAt = 0; failAt < kSaveMutationPoints; ++failAt) {
    FakePreferencesBackend backend;
    PreferencesConfigStore store(backend);
    expect(store.save(initial).ok(),
           "fault-injection baseline should save");

    backend.mutationCount = 0;
    backend.failMutationAt = failAt;
    expect(store.save(candidate).code == ResultCode::StorageError,
           "every slot or active-marker mutation failure should fail save");

    backend.failMutationAt = -1;
    DeviceConfig loaded;
    expect(store.load(loaded).ok() && configsEqual(loaded, initial),
           "failed save must preserve the last active config");
    expect(backend.activeSlot() == 0,
           "failed save must not switch away from the last active slot");
  }
}

void testReadbackMismatchDoesNotCommitSlot() {
  FakePreferencesBackend backend;
  PreferencesConfigStore store(backend);
  const DeviceConfig initial = namedConfig("verified-host", "VerifiedPass1");
  const DeviceConfig candidate = namedConfig("mismatch-host", "MismatchPass1");
  expect(store.save(initial).ok(), "readback baseline should save");

  backend.mutationCount = 0;
  backend.corruptWriteKey = "hostname";
  expect(store.save(candidate).code == ResultCode::StorageError,
         "readback mismatch should fail the save");
  backend.corruptWriteKey.clear();

  DeviceConfig loaded;
  expect(store.load(loaded).ok() && configsEqual(loaded, initial) &&
             backend.activeSlot() == 0,
         "readback mismatch must leave the previous slot active");
}

void testUnsupportedAndCorruptedSlotsArePreserved() {
  {
    FakePreferencesBackend backend;
    PreferencesConfigStore store(backend);
    const DeviceConfig initial = namedConfig("unsupported", "Unsupported1");
    expect(store.save(initial).ok(), "unsupported-schema baseline should save");
    backend.setUShort("devcfg_a", "schema", kDeviceConfigSchemaVersion + 1);
    backend.mutationCount = 0;

    DeviceConfig loaded;
    expect(store.load(loaded).code == ResultCode::Unsupported,
           "unsupported schema should be reported");
    expect(backend.mutationCount == 0 &&
               backend.namespaces["devcfg_a"]["schema"].number ==
                   kDeviceConfigSchemaVersion + 1,
           "unsupported schema must not be overwritten during load");
  }

  {
    FakePreferencesBackend backend;
    PreferencesConfigStore store(backend);
    const DeviceConfig initial = namedConfig("corrupted", "CorruptedPass1");
    expect(store.save(initial).ok(), "corruption baseline should save");
    backend.eraseKey("devcfg_a", "wifi_mode");
    backend.mutationCount = 0;

    DeviceConfig loaded;
    expect(store.load(loaded).code == ResultCode::StorageError,
           "incomplete slot should report corruption");
    expect(backend.mutationCount == 0 &&
               backend.namespaces["devcfg_a"].count("hostname") == 1,
           "corrupted slot must not be cleared during load");
  }

  {
    FakePreferencesBackend backend;
    PreferencesConfigStore store(backend);
    backend.inspectionErrors.insert("devcfg_a");
    DeviceConfig loaded;
    expect(store.load(loaded).code == ResultCode::StorageError,
           "namespace inspection failure should report storage error");
  }
}

void testWifiTxPowerCompatibilityAndValidation() {
  {
    FakePreferencesBackend backend;
    PreferencesConfigStore store(backend);
    DeviceConfig config = defaultDeviceConfig();
    config.wifiTxDbm = 20;
    expect(store.save(config).ok(), "20 dBm policy should persist");
    DeviceConfig loaded;
    expect(store.load(loaded).ok() && loaded.wifiTxDbm == 20,
           "Wi-Fi TX power should round-trip");

    backend.eraseKey("devcfg_a", "wifi_tx_dbm");
    expect(store.load(loaded).ok() && loaded.wifiTxDbm == kDefaultWifiTxDbm,
           "legacy slot without TX power should load the 15 dBm default");
  }

  {
    FakePreferencesBackend backend;
    PreferencesConfigStore store(backend);
    expect(store.save(defaultDeviceConfig()).ok(),
           "invalid TX power baseline should save");
    StoredValue wrongType;
    wrongType.type = StoredValue::Type::String;
    wrongType.text = "15";
    backend.namespaces["devcfg_a"]["wifi_tx_dbm"] = wrongType;
    DeviceConfig loaded;
    expect(store.load(loaded).code == ResultCode::StorageError,
           "wrong NVS type for TX power should be treated as corruption");
  }

  {
    FakePreferencesBackend backend;
    PreferencesConfigStore store(backend);
    expect(store.save(defaultDeviceConfig()).ok(),
           "out-of-range TX power baseline should save");
    backend.namespaces["devcfg_a"]["wifi_tx_dbm"].number = 21;
    DeviceConfig loaded;
    expect(store.load(loaded).code == ResultCode::StorageError,
           "out-of-range persisted TX power should be treated as corruption");
  }
}
}  // namespace

int main() {
  testEmptyActiveAndFallbackSlots();
  testEveryMutationFailurePreservesLastValidConfig();
  testReadbackMismatchDoesNotCommitSlot();
  testUnsupportedAndCorruptedSlotsArePreserved();
  testWifiTxPowerCompatibilityAndValidation();
  if (failures != 0) {
    std::cerr << failures << " Preferences config store test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "Preferences config store fault-injection tests passed\n";
  return EXIT_SUCCESS;
}
