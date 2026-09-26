#pragma once
#include "core/ProjectFeatures.h"
#if IOT_FEATURE_STATUS_LED

class StatusLedDriver {
 public:
  virtual ~StatusLedDriver() = default;
  virtual bool write(bool on) = 0;
  virtual bool holdOff() = 0;
  virtual bool releaseHold() = 0;
};

// Loop-owned mode indicator. No clock, blinking, task or persisted setting.
class StatusLed {
 public:
  void begin(StatusLedDriver &driver) { driver_ = &driver; }
  bool setNormal(bool normal);
  bool prepareSleep();
  bool cancelSleep();
  bool on() const { return on_; }
 private:
  StatusLedDriver *driver_ = nullptr;
  bool on_ = false;
  bool held_ = false;
};
#endif
