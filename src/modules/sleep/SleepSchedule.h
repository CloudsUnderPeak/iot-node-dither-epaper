#pragma once

#include <cstdint>

namespace SleepSchedule {

constexpr int64_t kEarlyToleranceSeconds = 60;

struct Slot {
  int64_t index = -1;
  int64_t dueEpoch = 0;
};

bool validPeriodMinutes(int value);
bool validDelayMinutes(int periodMinutes, int delayMinutes);
bool validClientEpoch(int64_t epoch);
bool anchorFor(int64_t clientEpoch, int delayMinutes, int64_t &anchor);
bool dueFor(int64_t anchor, int periodMinutes, int64_t index, int64_t &due);
// inclusive=false is used after an ordinary reboot: missed slots are skipped.
bool nextSlot(int64_t anchor, int periodMinutes, int64_t now,
              int64_t lastHandledIndex, bool inclusive, Slot &slot);
bool advancePast(int64_t anchor, int periodMinutes, int64_t now,
                 int64_t handledIndex, Slot &slot);
bool earlyFor(int64_t now, int64_t plannedDue);
uint32_t timerSecondsUntil(int64_t now, int64_t due, int periodMinutes);

}  // namespace SleepSchedule
