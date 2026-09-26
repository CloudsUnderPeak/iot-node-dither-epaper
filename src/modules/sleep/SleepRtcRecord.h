#pragma once

#include <cstdint>

enum class SleepClockBasis : uint8_t { Absolute = 1, Relative = 2 };

struct SleepRtcRecord {
  static constexpr uint32_t kMagic = 0x534c5031UL;
  static constexpr uint16_t kVersion = 1;
  uint32_t magic = kMagic;
  uint16_t version = kVersion;
  uint16_t length = sizeof(SleepRtcRecord);
  uint64_t scheduleGeneration = 0;
  SleepClockBasis basis = SleepClockBasis::Absolute;
  bool intent = false;
  bool synced = false;
  int64_t sleepEnteredClock = 0;
  int64_t timerTargetClock = 0;
  int64_t plannedDue = 0;
  int64_t slotIndex = -1;
  int64_t lastHandledSlot = -1;
  uint32_t crc32 = 0;
};

struct SleepWakeEvidence {
  bool valid = false;
  uint32_t sleptSeconds = 0;
};

uint32_t sleepRtcCrc32(const SleepRtcRecord &record);
void sealSleepRtcRecord(SleepRtcRecord &record);
bool sleepRtcRecordValid(const SleepRtcRecord &record);
SleepWakeEvidence sleepWakeEvidence(const SleepRtcRecord &record,
                                    bool deepSleepReset,
                                    int64_t bootClock,
                                    uint32_t maxPeriodSeconds);
