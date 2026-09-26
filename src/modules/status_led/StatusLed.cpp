#include "StatusLed.h"
#if IOT_FEATURE_STATUS_LED
bool StatusLed::setNormal(bool normal) {
  if (!driver_ || (held_ && normal)) return false;
  if (on_ == normal) return true;
  if (!driver_->write(normal)) return false;
  on_ = normal;
  return true;
}

bool StatusLed::prepareSleep() {
  if (!driver_ || !setNormal(false)) return false;
  // Track a possibly partial hold so cancellation always attempts release.
  held_ = true;
  return driver_->holdOff();
}

bool StatusLed::cancelSleep() {
  if (!driver_) return false;
  if (held_ && !driver_->releaseHold()) return false;
  held_ = false;
  return true;
}
#endif
