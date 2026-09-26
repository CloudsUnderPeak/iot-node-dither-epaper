#include "SleepFeatures.h"
#if ENABLE_SLEEP_SCHEDULER
#include "SleepStore.h"

#include <array>
#include <cstring>
#include <limits>

#include "SleepSchedule.h"

namespace {
constexpr const char *kSlots[] = {"sleep_a", "sleep_b"};
constexpr const char *kMeta = "sleep_meta";
constexpr const char *kRecordKey = "record";
constexpr const char *kSelectorKey = "active";
constexpr size_t kRecordBytes = 128;
constexpr size_t kSelectorBytes = 13;

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

void writeInt(uint8_t *bytes, size_t offset, uint64_t value, size_t size) {
  for (size_t i = 0; i < size; ++i) {
    bytes[offset + i] = static_cast<uint8_t>(value >> (8U * i));
  }
}

uint64_t readInt(const uint8_t *bytes, size_t offset, size_t size) {
  uint64_t value = 0;
  for (size_t i = 0; i < size; ++i) {
    value |= static_cast<uint64_t>(bytes[offset + i]) << (8U * i);
  }
  return value;
}

int64_t readSigned64(const uint8_t *bytes, size_t offset) {
  uint64_t bits = readInt(bytes, offset, 8);
  int64_t value = 0;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

int32_t readSigned32(const uint8_t *bytes, size_t offset) {
  uint32_t bits = static_cast<uint32_t>(readInt(bytes, offset, 4));
  int32_t value = 0;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

bool valid(const SleepRecord &record) {
  if (!SleepSchedule::validPeriodHours(record.periodHours) ||
      record.lastHandledSlot < -1 || record.lastWake.taskCount > 4 ||
      std::memchr(record.lastWake.lastErrorCode, '\0',
                  sizeof(record.lastWake.lastErrorCode)) == nullptr ||
      record.lastWake.staAttempts > 3 ||
      record.lastWake.cause > 1 || record.lastWake.mode > 2 ||
      record.lastWake.basis > 2 || record.lastWake.result > 9) return false;
  for (uint8_t index = 0; index < record.lastWake.taskCount; ++index) {
    const SleepTaskRecord &task = record.lastWake.tasks[index];
    if (task.name < 1 || task.name > 2 || task.status < 1 ||
        task.status > 4 || task.code > 12) return false;
  }
  if (record.enabled &&
      (!SleepSchedule::validClientEpoch(record.anchorEpoch) ||
       record.scheduleGeneration == 0)) return false;
  return true;
}

bool encode(const SleepRecord &record, std::array<uint8_t, kRecordBytes> &bytes) {
  if (!valid(record)) return false;
  bytes.fill(0);
  writeInt(bytes.data(), 0, 0x534c5032UL, 4);
  writeInt(bytes.data(), 4, 1, 2);
  writeInt(bytes.data(), 6, kRecordBytes, 2);
  writeInt(bytes.data(), 8, record.revision, 8);
  bytes[16] = record.enabled ? 1 : 0;
  bytes[17] = record.periodHours;
  writeInt(bytes.data(), 18, static_cast<uint64_t>(record.anchorEpoch), 8);
  writeInt(bytes.data(), 26, record.scheduleGeneration, 8);
  writeInt(bytes.data(), 34, static_cast<uint64_t>(record.lastHandledSlot), 8);
  const SleepLastWake &wake = record.lastWake;
  bytes[42] = wake.cause;
  bytes[43] = wake.mode;
  bytes[44] = wake.basis;
  writeInt(bytes.data(), 45, static_cast<uint64_t>(wake.epoch), 8);
  writeInt(bytes.data(), 53, static_cast<uint64_t>(wake.plannedDue), 8);
  writeInt(bytes.data(), 61, static_cast<uint32_t>(wake.driftSeconds), 4);
  bytes[65] = wake.result;
  writeInt(bytes.data(), 66, wake.consecutiveFailures, 2);
  bytes[68] = wake.staAttempts;
  bytes[69] = wake.timeSynced ? 1 : 0;
  std::memcpy(bytes.data() + 70, wake.lastErrorCode, 32);
  bytes[102] = wake.taskCount;
  for (size_t i = 0; i < 4; ++i) {
    bytes[103 + 3 * i] = wake.tasks[i].name;
    bytes[104 + 3 * i] = wake.tasks[i].status;
    bytes[105 + 3 * i] = wake.tasks[i].code;
  }
  writeInt(bytes.data(), 124, crc32(bytes.data(), 124), 4);
  return true;
}

bool decode(const std::array<uint8_t, kRecordBytes> &bytes, SleepRecord &record) {
  if (readInt(bytes.data(), 0, 4) != 0x534c5032UL ||
      readInt(bytes.data(), 4, 2) != 1 ||
      readInt(bytes.data(), 6, 2) != kRecordBytes ||
      readInt(bytes.data(), 124, 4) != crc32(bytes.data(), 124)) return false;
  SleepRecord candidate;
  candidate.revision = readInt(bytes.data(), 8, 8);
  if (bytes[16] > 1 || bytes[69] > 1) return false;
  candidate.enabled = bytes[16] != 0;
  candidate.periodHours = bytes[17];
  candidate.anchorEpoch = readSigned64(bytes.data(), 18);
  candidate.scheduleGeneration = readInt(bytes.data(), 26, 8);
  candidate.lastHandledSlot = readSigned64(bytes.data(), 34);
  SleepLastWake &wake = candidate.lastWake;
  wake.cause = bytes[42];
  wake.mode = bytes[43];
  wake.basis = bytes[44];
  wake.epoch = readSigned64(bytes.data(), 45);
  wake.plannedDue = readSigned64(bytes.data(), 53);
  wake.driftSeconds = readSigned32(bytes.data(), 61);
  wake.result = bytes[65];
  wake.consecutiveFailures = static_cast<uint16_t>(readInt(bytes.data(), 66, 2));
  wake.staAttempts = bytes[68];
  wake.timeSynced = bytes[69] != 0;
  std::memcpy(wake.lastErrorCode, bytes.data() + 70, 32);
  wake.taskCount = bytes[102];
  for (size_t i = 0; i < 4; ++i) {
    wake.tasks[i] = {bytes[103 + 3 * i], bytes[104 + 3 * i], bytes[105 + 3 * i]};
  }
  if (!valid(candidate)) return false;
  record = candidate;
  return true;
}

std::array<uint8_t, kSelectorBytes> selector(uint8_t slot, uint64_t revision) {
  std::array<uint8_t, kSelectorBytes> bytes{};
  bytes[0] = slot;
  writeInt(bytes.data(), 1, revision, 8);
  writeInt(bytes.data(), 9, crc32(bytes.data(), 9), 4);
  return bytes;
}

bool decodeSelector(const std::array<uint8_t, kSelectorBytes> &bytes,
                    uint8_t &slot, uint64_t &revision) {
  if (bytes[0] > 1 || readInt(bytes.data(), 9, 4) != crc32(bytes.data(), 9)) {
    return false;
  }
  slot = bytes[0];
  revision = readInt(bytes.data(), 1, 8);
  return true;
}

bool readRecord(PreferencesBackend &backend, uint8_t slot, SleepRecord &record) {
  if (!backend.open(kSlots[slot], true)) return false;
  std::array<uint8_t, kRecordBytes> bytes{};
  const bool read = backend.getBytes(kRecordKey, bytes.data(), bytes.size());
  backend.close();
  return read && decode(bytes, record);
}

bool readSelector(PreferencesBackend &backend, uint8_t &slot, uint64_t &revision) {
  if (!backend.open(kMeta, true)) return false;
  std::array<uint8_t, kSelectorBytes> bytes{};
  const bool read = backend.getBytes(kSelectorKey, bytes.data(), bytes.size());
  backend.close();
  return read && decodeSelector(bytes, slot, revision);
}
}  // namespace

SleepStoreState SleepStore::load(SleepRecord &record) {
  const PreferencesNamespaceState state = backend_.inspectNamespace(kMeta);
  if (state == PreferencesNamespaceState::Missing) {
    activeSlot_ = 0xff;
    active_ = {};
    record = active_;
    return state_ = SleepStoreState::Empty;
  }
  if (state == PreferencesNamespaceState::StorageError) {
    return state_ = SleepStoreState::Error;
  }
  uint8_t slot = 0xff;
  uint64_t revision = 0;
  SleepRecord loaded;
  if (!readSelector(backend_, slot, revision) ||
      !readRecord(backend_, slot, loaded) || loaded.revision != revision) {
    activeSlot_ = 0xff;
    active_ = {};
    record = active_;
    return state_ = SleepStoreState::Recovery;
  }
  activeSlot_ = slot;
  active_ = loaded;
  record = loaded;
  return state_ = SleepStoreState::Ready;
}

SleepStoreState SleepStore::save(const SleepRecord &candidate,
                                SleepRecord &committed) {
  if (state_ == SleepStoreState::Error ||
      active_.revision == std::numeric_limits<uint64_t>::max()) {
    return state_ = SleepStoreState::Error;
  }
  if (state_ == SleepStoreState::Ready) {
    SleepRecord comparison = candidate;
    comparison.revision = active_.revision;
    std::array<uint8_t, kRecordBytes> proposed{};
    std::array<uint8_t, kRecordBytes> current{};
    if (encode(comparison, proposed) && encode(active_, current) &&
        proposed == current) {
      committed = active_;
      return SleepStoreState::Ready;
    }
  }
  SleepRecord next = candidate;
  next.revision = active_.revision + 1;
  std::array<uint8_t, kRecordBytes> bytes{};
  if (!encode(next, bytes)) return state_ = SleepStoreState::Error;
  const uint8_t target = activeSlot_ == 0 ? 1 : 0;
  if (!backend_.open(kSlots[target], false)) return state_ = SleepStoreState::Error;
  const bool wrote = backend_.putBytes(kRecordKey, bytes.data(), bytes.size());
  backend_.close();
  SleepRecord readBack;
  if (!wrote || !readRecord(backend_, target, readBack) ||
      readBack.revision != next.revision) return state_ = SleepStoreState::Error;
  std::array<uint8_t, kRecordBytes> verified{};
  if (!encode(readBack, verified) || verified != bytes)
    return state_ = SleepStoreState::Error;

  const auto selected = selector(target, next.revision);
  if (!backend_.open(kMeta, false)) return state_ = SleepStoreState::Error;
  const bool switched = backend_.putBytes(kSelectorKey, selected.data(), selected.size());
  backend_.close();

  uint8_t actual = 0xff;
  uint64_t actualRevision = 0;
  if (!readSelector(backend_, actual, actualRevision)) {
    return state_ = SleepStoreState::Error;
  }
  if (actual == target && actualRevision == next.revision) {
    activeSlot_ = target;
    active_ = next;
    committed = next;
    return state_ = SleepStoreState::Ready;
  }
  if (!switched && actual == activeSlot_ &&
      actualRevision == active_.revision) return state_ = SleepStoreState::Error;
  return state_ = SleepStoreState::Error;
}
#endif
