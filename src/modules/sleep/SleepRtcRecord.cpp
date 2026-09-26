#include "SleepFeatures.h"
#if ENABLE_SLEEP_SCHEDULER
#include "SleepRtcRecord.h"

#include <cstddef>
#include <limits>

namespace {
uint32_t appendByte(uint32_t crc, uint8_t byte) {
  crc ^= byte;
  for (unsigned bit = 0; bit < 8; ++bit) {
    crc = (crc >> 1U) ^ ((crc & 1U) != 0 ? 0xedb88320UL : 0UL);
  }
  return crc;
}

template <typename Value>
uint32_t appendLe(uint32_t crc, Value value) {
  const uint64_t encoded = static_cast<uint64_t>(value);
  for (std::size_t index = 0; index < sizeof(Value); ++index) {
    crc = appendByte(crc, static_cast<uint8_t>(encoded >> (index * 8U)));
  }
  return crc;
}
}  // namespace

uint32_t sleepRtcCrc32(const SleepRtcRecord &record) {
  uint32_t crc = 0xffffffffUL;
  crc = appendLe(crc, record.magic);
  crc = appendLe(crc, record.version);
  crc = appendLe(crc, record.length);
  crc = appendLe(crc, record.scheduleGeneration);
  crc = appendLe(crc, static_cast<uint8_t>(record.basis));
  crc = appendLe(crc, static_cast<uint8_t>(record.intent ? 1 : 0));
  crc = appendLe(crc, static_cast<uint8_t>(record.synced ? 1 : 0));
  crc = appendLe(crc, record.sleepEnteredClock);
  crc = appendLe(crc, record.timerTargetClock);
  crc = appendLe(crc, record.plannedDue);
  crc = appendLe(crc, record.slotIndex);
  crc = appendLe(crc, record.lastHandledSlot);
  return crc ^ 0xffffffffUL;
}

void sealSleepRtcRecord(SleepRtcRecord &record) {
  record.magic = SleepRtcRecord::kMagic;
  record.version = SleepRtcRecord::kVersion;
  record.length = sizeof(SleepRtcRecord);
  record.crc32 = sleepRtcCrc32(record);
}

bool sleepRtcRecordValid(const SleepRtcRecord &record) {
  return record.magic == SleepRtcRecord::kMagic &&
         record.version == SleepRtcRecord::kVersion &&
         record.length == sizeof(SleepRtcRecord) && record.intent &&
         (record.basis == SleepClockBasis::Absolute ||
          record.basis == SleepClockBasis::Relative) &&
         record.sleepEnteredClock >= 0 &&
         record.timerTargetClock > record.sleepEnteredClock &&
         record.plannedDue > 0 &&
         (record.basis == SleepClockBasis::Absolute ? record.slotIndex >= 0
                                                   : record.slotIndex == -1) &&
         record.lastHandledSlot >= -1 &&
         record.crc32 == sleepRtcCrc32(record);
}

SleepWakeEvidence sleepWakeEvidence(const SleepRtcRecord &record,
                                    bool deepSleepReset,
                                    int64_t bootClock,
                                    uint32_t maxPeriodSeconds) {
  if (!deepSleepReset || !sleepRtcRecordValid(record) ||
      bootClock < record.sleepEnteredClock) return {};
  const uint64_t elapsed = static_cast<uint64_t>(bootClock) -
                           static_cast<uint64_t>(record.sleepEnteredClock);
  if (elapsed > static_cast<uint64_t>(maxPeriodSeconds) + 300U ||
      elapsed > std::numeric_limits<uint32_t>::max()) return {};
  return {true, static_cast<uint32_t>(elapsed)};
}
#endif
