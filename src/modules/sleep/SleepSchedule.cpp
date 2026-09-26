#include "SleepFeatures.h"
#if IOT_FEATURE_SLEEP
#include "SleepSchedule.h"

#include <limits>

namespace SleepSchedule {

bool validPeriodHours(int value) {
  return value == 12 || value == 24 || value == 48;
}

bool validDelayMinutes(int periodHours, int delayMinutes) {
  return validPeriodHours(periodHours) && delayMinutes >= 1 &&
         delayMinutes <= periodHours * 60;
}

bool validClientEpoch(int64_t epoch) {
  return epoch >= 1704067200LL && epoch < 4102444800LL;
}

bool anchorFor(int64_t clientEpoch, int delayMinutes, int64_t &anchor) {
  if (!validClientEpoch(clientEpoch) || delayMinutes < 1 || delayMinutes > 2880) {
    return false;
  }
  anchor = clientEpoch + static_cast<int64_t>(delayMinutes) * 60;
  return true;
}

bool dueFor(int64_t anchor, int periodHours, int64_t index, int64_t &due) {
  if (!validPeriodHours(periodHours) || anchor < 0 || index < 0) return false;
  const int64_t period = static_cast<int64_t>(periodHours) * 3600;
  if (index > (std::numeric_limits<int64_t>::max() - anchor) / period) return false;
  due = anchor + index * period;
  return true;
}

bool nextSlot(int64_t anchor, int periodHours, int64_t now,
              int64_t lastHandledIndex, bool inclusive, Slot &slot) {
  if (!validPeriodHours(periodHours) || anchor < 0 || now < 0 ||
      lastHandledIndex < -1 || lastHandledIndex == std::numeric_limits<int64_t>::max()) {
    return false;
  }
  const int64_t period = static_cast<int64_t>(periodHours) * 3600;
  int64_t index = 0;
  if (now >= anchor) {
    index = (now - anchor) / period;
    const int64_t boundary = anchor + index * period;
    if (!inclusive || boundary < now) {
      if (index == std::numeric_limits<int64_t>::max()) return false;
      ++index;
    }
  }
  if (index <= lastHandledIndex) index = lastHandledIndex + 1;
  int64_t due = 0;
  if (!dueFor(anchor, periodHours, index, due)) return false;
  slot = {index, due};
  return true;
}

bool advancePast(int64_t anchor, int periodHours, int64_t now,
                 int64_t handledIndex, Slot &slot) {
  return nextSlot(anchor, periodHours, now, handledIndex, false, slot);
}

bool earlyFor(int64_t now, int64_t plannedDue) {
  return now >= 0 && plannedDue > now &&
         static_cast<uint64_t>(plannedDue) - static_cast<uint64_t>(now) >
             static_cast<uint64_t>(kEarlyToleranceSeconds);
}

uint32_t timerSecondsUntil(int64_t now, int64_t due, int periodHours) {
  if (!validPeriodHours(periodHours) || due <= now || now < 0) return 0;
  const int64_t remaining = due - now;
  const int64_t cap = static_cast<int64_t>(periodHours) * 3600;
  return static_cast<uint32_t>(remaining > cap ? cap : remaining);
}

}  // namespace SleepSchedule
#endif
