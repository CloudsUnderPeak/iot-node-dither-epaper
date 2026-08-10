#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <string>

#include "modules/epaper/calibration/EpaperCalibrationService.h"
#include "modules/epaper/calibration/storage/PreferencesEpaperCalibrationStore.h"

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

class FakePreferencesBackend final : public PreferencesBackend {
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
    if (readOnly && namespaces.count(name) == 0) return false;
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
      stored.text = corruptWriteKey == key ? "corrupt" : value;
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
    const auto namespaceIt = namespaces.find("epcal_meta");
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

EpaperCalibration::Profile adjustedProfile(uint8_t red) {
  EpaperCalibration::Profile profile = EpaperCalibration::defaultProfile();
  profile.display[3].r = red;
  return profile;
}

void testDefinitionsAndValidation() {
  const char *ids[] = {"black", "white", "yellow", "red", "blue", "green"};
  const uint8_t codes[] = {0, 1, 2, 3, 5, 6};
  for (size_t index = 0; index < EpaperCalibration::kColorCount; ++index) {
    expect(strcmp(EpaperCalibration::definition(index).id, ids[index]) == 0 &&
               EpaperCalibration::definition(index).code == codes[index] &&
               EpaperCalibration::slotForId(ids[index]) ==
                   static_cast<int>(index),
           "calibration definitions should follow EPD code order");
  }
  expect(EpaperCalibration::definition(2).protocol.r == 255 &&
             EpaperCalibration::definition(2).protocol.g == 255 &&
             EpaperCalibration::definition(2).protocol.b == 0 &&
             EpaperCalibration::definition(3).protocol.r == 255 &&
             EpaperCalibration::definition(3).protocol.g == 0,
         "yellow and red protocol RGB should map to code 2 and 3");

  EpaperCalibration::Profile profile = EpaperCalibration::defaultProfile();
  expect(EpaperCalibration::validate(profile).ok(),
         "default calibration should be valid");
  profile.display[0] = {0, 0, 0};
  profile.display[1] = {255, 255, 255};
  expect(EpaperCalibration::validate(profile).ok(),
         "channel boundaries should be accepted");
  profile.display[1] = profile.display[0];
  expect(EpaperCalibration::validate(profile).code == ResultCode::InvalidInput,
         "duplicate display RGB should be rejected");
}

void testDualSlotAndFallback() {
  FakePreferencesBackend backend;
  PreferencesEpaperCalibrationStore store(backend);
  EpaperCalibration::Profile loaded;
  expect(store.load(loaded).code == ResultCode::NotFound,
         "empty calibration store should report missing");
  expect(backend.namespaces.empty(),
         "empty load should not create namespaces");

  const EpaperCalibration::Profile first = adjustedProfile(121);
  const EpaperCalibration::Profile second = adjustedProfile(122);
  expect(store.save(first).ok() && backend.activeSlot() == 0,
         "first calibration save should commit slot A");
  expect(store.save(second).ok() && backend.activeSlot() == 1,
         "second calibration save should commit slot B");
  expect(store.load(loaded).ok() && EpaperCalibration::equal(loaded, second),
         "active calibration should load exactly");

  backend.eraseKey("epcal_b", "colors");
  expect(store.load(loaded).ok() && EpaperCalibration::equal(loaded, first) &&
             backend.activeSlot() == 0,
         "corrupt active calibration should fall back and repair marker");
}

void testMutationFailuresPreserveActiveCalibration() {
  const EpaperCalibration::Profile initial = adjustedProfile(123);
  const EpaperCalibration::Profile candidate = adjustedProfile(124);
  constexpr int kMutationPoints = 4;
  for (int failAt = 0; failAt < kMutationPoints; ++failAt) {
    FakePreferencesBackend backend;
    PreferencesEpaperCalibrationStore store(backend);
    expect(store.save(initial).ok(), "fault baseline should save");
    backend.mutationCount = 0;
    backend.failMutationAt = failAt;
    expect(store.save(candidate).code == ResultCode::StorageError,
           "slot or marker mutation failure should fail calibration save");
    backend.failMutationAt = -1;
    EpaperCalibration::Profile loaded;
    expect(store.load(loaded).ok() && EpaperCalibration::equal(loaded, initial) &&
               backend.activeSlot() == 0,
           "failed calibration save should preserve active profile");
  }
}

void testReadbackUnsupportedAndReset() {
  {
    FakePreferencesBackend backend;
    PreferencesEpaperCalibrationStore store(backend);
    const EpaperCalibration::Profile initial = adjustedProfile(125);
    const EpaperCalibration::Profile candidate = adjustedProfile(126);
    expect(store.save(initial).ok(), "readback baseline should save");
    backend.mutationCount = 0;
    backend.corruptWriteKey = "colors";
    expect(store.save(candidate).code == ResultCode::StorageError,
           "calibration readback mismatch should fail save");
    backend.corruptWriteKey.clear();
    EpaperCalibration::Profile loaded;
    expect(store.load(loaded).ok() && EpaperCalibration::equal(loaded, initial),
           "readback mismatch should preserve previous calibration");
  }

  {
    FakePreferencesBackend backend;
    PreferencesEpaperCalibrationStore store(backend);
    expect(store.save(adjustedProfile(125)).ok() &&
               store.save(adjustedProfile(126)).ok(),
           "unsupported dual-slot baseline should save");
    backend.setUShort("epcal_b", "schema",
                      EpaperCalibration::kSchemaVersion + 1);
    EpaperCalibrationService service(store);
    expect(service.begin().ok(), "service should remain available in recovery");
    EpaperCalibrationSnapshot snapshot = service.snapshot();
    expect(snapshot.source == EpaperCalibrationSource::RecoveryDefault &&
               snapshot.recoveryReason == ResultCode::Unsupported,
           "unsupported schema should expose recovery defaults");
    expect(service.update(adjustedProfile(127)).code == ResultCode::Unsupported,
           "unsupported schema should block update until explicit reset");
    expect(service.reset(&snapshot).ok() &&
               snapshot.source == EpaperCalibrationSource::Default &&
               store.load(snapshot.profile).code == ResultCode::NotFound,
           "explicit reset should clear slots and restore defaults");
  }
}

void testServicePersistenceAndFailureIsolation() {
  FakePreferencesBackend backend;
  PreferencesEpaperCalibrationStore store(backend);
  EpaperCalibrationService service(store);
  expect(service.begin().ok(), "empty calibration service should start");
  EpaperCalibrationSnapshot snapshot = service.snapshot();
  expect(snapshot.ready && snapshot.source == EpaperCalibrationSource::Default,
         "empty calibration service should expose non-persisted defaults");

  const EpaperCalibration::Profile persisted = adjustedProfile(128);
  expect(service.update(persisted, &snapshot).ok() &&
             snapshot.source == EpaperCalibrationSource::Persisted,
         "successful service update should publish persisted calibration");
  backend.mutationCount = 0;
  backend.failMutationAt = 0;
  expect(service.update(adjustedProfile(129)).code == ResultCode::StorageError &&
             EpaperCalibration::equal(service.snapshot().profile, persisted),
         "failed service update should leave RAM canonical unchanged");
}

}  // namespace

int main() {
  testDefinitionsAndValidation();
  testDualSlotAndFallback();
  testMutationFailuresPreserveActiveCalibration();
  testReadbackUnsupportedAndReset();
  testServicePersistenceAndFailureIsolation();
  if (failures != 0) {
    std::cerr << failures << " e-paper calibration test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "E-paper calibration model, store, and service tests passed\n";
  return EXIT_SUCCESS;
}
