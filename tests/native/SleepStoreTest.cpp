#include <array>
#include <cassert>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include "modules/sleep/SleepStore.h"

namespace {
uint32_t crc32(const uint8_t *bytes, size_t length) {
  uint32_t value = 0xffffffffUL;
  for (size_t i = 0; i < length; ++i) {
    value ^= bytes[i];
    for (unsigned bit = 0; bit < 8; ++bit) {
      value = (value >> 1U) ^ ((value & 1U) != 0 ? 0xedb88320UL : 0UL);
    }
  }
  return value ^ 0xffffffffUL;
}

void writeInt(std::vector<uint8_t> &bytes, size_t offset, uint64_t value,
              size_t size) {
  for (size_t i = 0; i < size; ++i) {
    bytes[offset + i] = static_cast<uint8_t>(value >> (8U * i));
  }
}

void resealRecord(std::vector<uint8_t> &bytes) {
  assert(bytes.size() == 128);
  writeInt(bytes, 124, crc32(bytes.data(), 124), 4);
}

void resealSelector(std::vector<uint8_t> &bytes) {
  assert(bytes.size() == 13);
  writeInt(bytes, 9, crc32(bytes.data(), 9), 4);
}

class MemoryBackend final : public PreferencesBackend {
 public:
  std::map<std::string, std::map<std::string, std::vector<uint8_t>>> spaces;
  std::string active;
  PreferencesNamespaceState inspectState = PreferencesNamespaceState::Exists;
  bool useInspectState = false;
  bool failOpen = false;
  bool failWrite = false;
  bool failSelector = false;
  bool selectorWriteThenFail = false;
  bool corruptRecordWrite = false;
  std::string failReadSpace;
  unsigned writeCalls = 0;

  PreferencesNamespaceState inspectNamespace(const char *name) override {
    if (useInspectState) return inspectState;
    return spaces.count(name) ? PreferencesNamespaceState::Exists
                              : PreferencesNamespaceState::Missing;
  }
  bool open(const char *name, bool) override {
    if (failOpen) return false;
    active = name;
    spaces[name];
    return true;
  }
  void close() override { active.clear(); }
  bool clear() override {
    spaces[active].clear();
    return true;
  }
  bool hasKey(const char *key) const override {
    return spaces.at(active).count(key);
  }
  uint8_t getUChar(const char *, uint8_t value) const override { return value; }
  bool getUCharChecked(const char *, uint8_t &) const override { return false; }
  uint16_t getUShort(const char *, uint16_t value) const override {
    return value;
  }
  bool getBool(const char *, bool value) const override { return value; }
  bool getString(const char *, char *, size_t) const override { return false; }
  bool putUChar(const char *, uint8_t) override { return false; }
  bool putUShort(const char *, uint16_t) override { return false; }
  bool putBool(const char *, bool) override { return false; }
  bool putString(const char *, const char *) override { return false; }
  bool getBytes(const char *key, void *target, size_t size) const override {
    if (!failReadSpace.empty() && active == failReadSpace) return false;
    const auto ns = spaces.find(active);
    if (ns == spaces.end() || !ns->second.count(key) ||
        ns->second.at(key).size() != size) {
      return false;
    }
    std::memcpy(target, ns->second.at(key).data(), size);
    return true;
  }
  bool putBytes(const char *key, const void *source, size_t size) override {
    ++writeCalls;
    if (failWrite || (failSelector && active == "sleep_meta")) return false;
    const auto *first = static_cast<const uint8_t *>(source);
    spaces[active][key] = {first, first + size};
    if (corruptRecordWrite && active != "sleep_meta") {
      spaces[active][key][17] = 13;
    }
    return !(selectorWriteThenFail && active == "sleep_meta");
  }
};

SleepRecord sampleRecord() {
  SleepRecord value;
  value.enabled = true;
  value.periodHours = 24;
  value.anchorEpoch = 1704067200LL;
  value.scheduleGeneration = 1;
  value.lastHandledSlot = 42;
  value.lastWake.cause = 1;
  value.lastWake.mode = 2;
  value.lastWake.basis = 1;
  value.lastWake.epoch = 1704153605LL;
  value.lastWake.plannedDue = 1704153600LL;
  value.lastWake.driftSeconds = 5;
  value.lastWake.result = 7;
  value.lastWake.consecutiveFailures = 3;
  value.lastWake.staAttempts = 3;
  value.lastWake.timeSynced = true;
  std::strcpy(value.lastWake.lastErrorCode, "draw_failed");
  value.lastWake.taskCount = 4;
  value.lastWake.tasks[0] = {1, 2, 1};
  value.lastWake.tasks[1] = {2, 4, 7};
  value.lastWake.tasks[2] = {1, 3, 9};
  value.lastWake.tasks[3] = {2, 3, 12};
  return value;
}

SleepRecord commitBaseline(SleepStore &store) {
  SleepRecord loaded;
  assert(store.load(loaded) == SleepStoreState::Empty);
  SleepRecord value = sampleRecord();
  assert(store.save(value, loaded) == SleepStoreState::Ready);
  assert(loaded.revision == 1);
  return loaded;
}

void assertBaselineStillSelected(MemoryBackend &backend, int64_t anchor) {
  SleepStore rebooted(backend);
  SleepRecord loaded;
  assert(rebooted.load(loaded) == SleepStoreState::Ready);
  assert(loaded.anchorEpoch == anchor && loaded.revision == 1);
}

void testRoundTripAndNoop() {
  MemoryBackend backend;
  SleepStore store(backend);
  SleepRecord committed = commitBaseline(store);
  const unsigned writes = backend.writeCalls;
  SleepRecord noop;
  assert(store.save(committed, noop) == SleepStoreState::Ready);
  assert(noop.revision == 1 && backend.writeCalls == writes);

  SleepStore afterBoot(backend);
  SleepRecord loaded;
  assert(afterBoot.load(loaded) == SleepStoreState::Ready);
  assert(loaded.anchorEpoch == 1704067200LL);
  assert(loaded.lastHandledSlot == 42);
  assert(loaded.lastWake.taskCount == 4);
  assert(loaded.lastWake.tasks[3].code == 12);
  assert(std::strcmp(loaded.lastWake.lastErrorCode, "draw_failed") == 0);
}

void testInactiveSlotFailuresKeepOldSelector() {
  {
    MemoryBackend backend;
    SleepStore store(backend);
    SleepRecord baseline = commitBaseline(store);
    SleepRecord candidate = baseline;
    ++candidate.anchorEpoch;
    backend.failWrite = true;
    SleepRecord ignored;
    assert(store.save(candidate, ignored) == SleepStoreState::Error);
    backend.failWrite = false;
    assertBaselineStillSelected(backend, baseline.anchorEpoch);
  }
  {
    MemoryBackend backend;
    SleepStore store(backend);
    SleepRecord baseline = commitBaseline(store);
    SleepRecord candidate = baseline;
    candidate.anchorEpoch += 2;
    backend.failReadSpace = "sleep_b";
    SleepRecord ignored;
    assert(store.save(candidate, ignored) == SleepStoreState::Error);
    backend.failReadSpace.clear();
    assertBaselineStillSelected(backend, baseline.anchorEpoch);
  }
  {
    MemoryBackend backend;
    SleepStore store(backend);
    SleepRecord baseline = commitBaseline(store);
    SleepRecord candidate = baseline;
    candidate.anchorEpoch += 3;
    backend.corruptRecordWrite = true;
    SleepRecord ignored;
    assert(store.save(candidate, ignored) == SleepStoreState::Error);
    backend.corruptRecordWrite = false;
    assertBaselineStillSelected(backend, baseline.anchorEpoch);
  }
}

void testSelectorOutcomesAreResolvedOrFailClosed() {
  {
    MemoryBackend backend;
    SleepStore store(backend);
    SleepRecord baseline = commitBaseline(store);
    SleepRecord candidate = baseline;
    candidate.anchorEpoch += 10;
    backend.failSelector = true;
    SleepRecord ignored;
    assert(store.save(candidate, ignored) == SleepStoreState::Error);
    backend.failSelector = false;
    assertBaselineStillSelected(backend, baseline.anchorEpoch);
  }
  {
    MemoryBackend backend;
    SleepStore store(backend);
    SleepRecord baseline = commitBaseline(store);
    SleepRecord candidate = baseline;
    candidate.anchorEpoch += 20;
    backend.selectorWriteThenFail = true;
    SleepRecord committed;
    assert(store.save(candidate, committed) == SleepStoreState::Ready);
    assert(committed.anchorEpoch == candidate.anchorEpoch &&
           committed.revision == 2);
    backend.selectorWriteThenFail = false;
    SleepStore rebooted(backend);
    assert(rebooted.load(committed) == SleepStoreState::Ready);
    assert(committed.anchorEpoch == candidate.anchorEpoch);
  }
  {
    MemoryBackend backend;
    SleepStore store(backend);
    SleepRecord baseline = commitBaseline(store);
    SleepRecord candidate = baseline;
    candidate.anchorEpoch += 30;
    backend.selectorWriteThenFail = true;
    backend.failReadSpace = "sleep_meta";
    SleepRecord ignored;
    assert(store.save(candidate, ignored) == SleepStoreState::Error);
    backend.selectorWriteThenFail = false;
    backend.failReadSpace.clear();
    SleepStore rebooted(backend);
    SleepRecord loaded;
    assert(rebooted.load(loaded) == SleepStoreState::Ready);
    assert(loaded.anchorEpoch == candidate.anchorEpoch && loaded.revision == 2);
  }
}

void testRecoveryAndSchemaValidation() {
  {
    MemoryBackend backend;
    backend.spaces["sleep_meta"];
    SleepStore store(backend);
    SleepRecord loaded;
    assert(store.load(loaded) == SleepStoreState::Recovery && !loaded.enabled);
  }
  {
    MemoryBackend backend;
    SleepStore store(backend);
    commitBaseline(store);
    auto &record = backend.spaces["sleep_a"]["record"];
    record[4] = 2;
    resealRecord(record);
    SleepStore rebooted(backend);
    SleepRecord loaded;
    assert(rebooted.load(loaded) == SleepStoreState::Recovery);
  }
  {
    MemoryBackend backend;
    SleepStore store(backend);
    commitBaseline(store);
    backend.spaces["sleep_meta"]["active"][0] ^= 1;
    SleepStore rebooted(backend);
    SleepRecord loaded;
    assert(rebooted.load(loaded) == SleepStoreState::Recovery);
  }
  {
    MemoryBackend backend;
    backend.useInspectState = true;
    backend.inspectState = PreferencesNamespaceState::StorageError;
    SleepStore store(backend);
    SleepRecord loaded;
    assert(store.load(loaded) == SleepStoreState::Error);
  }
}

void testInvalidValuesAndRevisionExhaustionFailClosed() {
  {
    MemoryBackend backend;
    SleepStore store(backend);
    SleepRecord loaded;
    assert(store.load(loaded) == SleepStoreState::Empty);
    SleepRecord invalid = sampleRecord();
    invalid.scheduleGeneration = 0;
    const unsigned writes = backend.writeCalls;
    assert(store.save(invalid, loaded) == SleepStoreState::Error);
    assert(backend.writeCalls == writes);
  }
  {
    MemoryBackend backend;
    SleepStore store(backend);
    commitBaseline(store);
    auto &record = backend.spaces["sleep_a"]["record"];
    auto &selector = backend.spaces["sleep_meta"]["active"];
    writeInt(record, 8, std::numeric_limits<uint64_t>::max(), 8);
    resealRecord(record);
    writeInt(selector, 1, std::numeric_limits<uint64_t>::max(), 8);
    resealSelector(selector);

    SleepStore rebooted(backend);
    SleepRecord loaded;
    assert(rebooted.load(loaded) == SleepStoreState::Ready);
    const unsigned writes = backend.writeCalls;
    SleepRecord candidate = loaded;
    ++candidate.anchorEpoch;
    assert(rebooted.save(candidate, loaded) == SleepStoreState::Error);
    assert(backend.writeCalls == writes);
  }
}
}  // namespace

int main() {
  testRoundTripAndNoop();
  testInactiveSlotFailuresKeepOldSelector();
  testSelectorOutcomesAreResolvedOrFailClosed();
  testRecoveryAndSchemaValidation();
  testInvalidValuesAndRevisionExhaustionFailClosed();
}
