#pragma once

#include "SleepRtcRecord.h"

enum class WakeMode : uint8_t { Normal, WakeCycle };
enum class WakeCause : uint8_t { None, Timer, Gpio, Poll, Unknown };

WakeMode classifyWake(bool deepSleepReset, WakeCause cause,
                      const SleepRtcRecord &record, bool enabled,
                      uint64_t scheduleGeneration);
