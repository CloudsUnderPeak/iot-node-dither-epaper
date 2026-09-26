#pragma once

#include "SleepDriver.h"

class ArduinoSleepDriver final : public SleepDriver {
 public:
  bool usbHostConnected() const override;
  WakeCause wakeupCause() const override;
  bool armTimer(uint32_t seconds) override;
  bool disarmTimer() override;
  bool holdEpaperPins() override;
  bool releaseEpaperPins() override;
  SleepRtcRecord bootRecord() const override;
  void writeRecord(const SleepRtcRecord &record) override;
  void invalidateRecord() override;
  void drainSerial(uint32_t timeoutMs) override;
  void deepSleep() override;
};
