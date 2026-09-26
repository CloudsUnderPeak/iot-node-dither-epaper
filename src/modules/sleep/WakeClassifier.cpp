#include "SleepFeatures.h"
#if IOT_FEATURE_SLEEP
#include "WakeClassifier.h"

WakeMode classifyWake(bool deepSleepReset, WakeCause cause,
                      const SleepRtcRecord &record, bool enabled,
                      uint64_t scheduleGeneration) {
  return deepSleepReset && cause == WakeCause::Timer && enabled &&
                 sleepRtcRecordValid(record) &&
                 record.scheduleGeneration == scheduleGeneration
             ? WakeMode::WakeCycle
             : WakeMode::Normal;
}
#endif
