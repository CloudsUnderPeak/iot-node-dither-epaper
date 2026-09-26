#pragma once

#include "TimeSource.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class SystemClockTimeSource final : public TimeSource {
 public:
  bool begin();
  TimeSnapshot snapshot() const override;
  bool set(int64_t epoch, TimeOrigin origin) override;
  void clear() override;
  void restoreCarried(bool evidenceValid) override;

 private:
  mutable SemaphoreHandle_t setterMutex_ = nullptr;
  mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  TimeOrigin origin_ = TimeOrigin::None;
  uint32_t revision_ = 0;
};
