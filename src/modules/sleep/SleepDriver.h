#pragma once

#include <cstdint>

#include "SleepRtcRecord.h"
#include "WakeClassifier.h"

class SleepDriver {
 public:
  virtual ~SleepDriver() = default;
  virtual bool usbHostConnected() const = 0;
  virtual WakeCause wakeupCause() const = 0;
  virtual bool armTimer(uint32_t seconds) = 0;
  virtual bool disarmTimer() = 0;
  virtual bool holdEpaperPins() = 0;
  virtual bool releaseEpaperPins() = 0;
  virtual SleepRtcRecord bootRecord() const = 0;
  virtual void writeRecord(const SleepRtcRecord &record) = 0;
  virtual void invalidateRecord() = 0;
  virtual void drainSerial(uint32_t timeoutMs) = 0;
  virtual void deepSleep() = 0;
};
