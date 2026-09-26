#include <cassert>
#include <cstdint>
#include <limits>
#include <initializer_list>

#include "modules/sleep/IdleTimer.h"
#include "modules/sleep/SleepSchedule.h"

int main() {
  using namespace SleepSchedule;
  assert(validPeriodMinutes(1) && validPeriodMinutes(1440) && validPeriodMinutes(2880));
  assert(!validPeriodMinutes(0) && !validPeriodMinutes(2881));
  assert(validDelayMinutes(1440, 1) && validDelayMinutes(1440, 1440));
  assert(!validDelayMinutes(1440, 0) && !validDelayMinutes(1440, 1441));
  assert(validClientEpoch(1704067200LL) && !validClientEpoch(4102444800LL));
  int64_t anchor = 0;
  assert(anchorFor(1704067200LL, 90, anchor) && anchor == 1704072600LL);
  assert(!anchorFor(1, 90, anchor));

  Slot slot;
  const int64_t morning = 1704103200LL; // 10:00 UTC.
  assert(nextSlot(morning, 1440, morning - 1, -1, true, slot));
  assert(slot.index == 0 && slot.dueEpoch == morning);
  assert(nextSlot(morning, 1440, morning, -1, true, slot));
  assert(slot.index == 0); // An awake normal mode processes the due slot.
  assert(nextSlot(morning, 1440, morning, -1, false, slot));
  assert(slot.index == 1); // Ordinary reboot skips a missed slot.
  assert(advancePast(morning, 1440, morning + 60, 0, slot));
  assert(slot.index == 1 && slot.dueEpoch == morning + 86400);
  assert(advancePast(morning, 1440, morning + 3 * 86400 + 60, 0, slot));
  assert(slot.index == 4);
  assert(!earlyFor(morning - 60, morning));
  assert(earlyFor(morning - 61, morning));
  assert(timerSecondsUntil(morning, morning + 200000, 1440) == 86400);
  assert(timerSecondsUntil(morning, morning + 4, 1440) == 4);
  int64_t due = 0;
  assert(!dueFor(morning, 2880, std::numeric_limits<int64_t>::max(), due));


  // Model seven scheduled days with the timer and system clock driven by the
  // same imperfect oscillator. NTP uses true UTC and must converge from an
  // early fast-clock wake without replaying a slot; the relative path stays
  // periodic in its own RTC domain.
  for (int periodMinutes : {720, 1440, 2880}) {
    const int64_t periodSeconds = static_cast<int64_t>(periodMinutes) * 60;
    const int targetSlots = static_cast<int>((7 * 1440) / periodMinutes);
    for (double oscillator : {0.95, 1.0, 1.05}) {
      for (bool ntpAvailable : {false, true}) {
        double trueUtc = static_cast<double>(morning);
        double rtcClock = static_cast<double>(morning);
        int64_t planned = morning + periodSeconds;
        int handled = 0;
        int wakes = 0;
        while (handled < targetSlots && wakes < targetSlots * 4) {
          const int64_t rtcNow = static_cast<int64_t>(rtcClock);
          const uint32_t armed = timerSecondsUntil(rtcNow, planned, periodMinutes);
          assert(armed > 0);
          trueUtc += static_cast<double>(armed) / oscillator;
          rtcClock += armed;
          ++wakes;
          const int64_t decisionClock = ntpAvailable
              ? static_cast<int64_t>(trueUtc)
              : static_cast<int64_t>(rtcClock);
          if (ntpAvailable) rtcClock = trueUtc;
          if (ntpAvailable && earlyFor(decisionClock, planned)) continue;
          assert(decisionClock >= planned - kEarlyToleranceSeconds);
          ++handled;
          planned += periodSeconds;
        }
        assert(handled == targetSlots);
        assert(wakes <= targetSlots * 3);
      }
    }
  }

  // Minute schedules retain their anchor and skip slots overrun by work.
  assert(validDelayMinutes(1, 1) && !validDelayMinutes(1, 2));
  assert(timerSecondsUntil(morning, morning + 60, 1) == 60);
  assert(advancePast(morning, 1, morning + 185, 0, slot));
  assert(slot.index == 4 && slot.dueEpoch == morning + 240);
  assert(nextSlot(morning, 2880, morning - 1, -1, true, slot));
  assert(slot.index == 0);

  IdleTimer timer(1800);
  timer.arm(UINT32_MAX - 1000U);
  assert(timer.remainingSeconds(UINT32_MAX - 1000U) == 1800);
  assert(!timer.expired(1000U));
  timer.noteActivity(1000U);
  assert(!timer.expired(1000U + 1799000U));
  assert(timer.expired(1000U + 1800000U));
  assert(timer.expired(1000U + 1800001U));
  timer.noteActivity(1000U + 1800001U);
  assert(timer.remainingSeconds(1000U + 1800001U) == 1800);
  timer.disarm();
  assert(!timer.armed());
}
