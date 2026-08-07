#include "WifiRadio.h"

Result WifiRadio::begin() {
  if (mutex_ == nullptr) mutex_ = xSemaphoreCreateMutex();
  return mutex_ == nullptr ? outOfSpace("failed to create Wi-Fi radio mutex") : okResult();
}

bool WifiRadio::ready() const {
  return mutex_ != nullptr;
}

bool WifiRadio::lock(TickType_t timeoutTicks) {
  return mutex_ != nullptr && xSemaphoreTake(mutex_, timeoutTicks) == pdTRUE;
}

void WifiRadio::unlock() {
  xSemaphoreGive(mutex_);
}

WifiRadioGuard::WifiRadioGuard(WifiRadio *radio, TickType_t timeoutTicks)
    : radio_(radio), locked_(radio != nullptr && radio->lock(timeoutTicks)) {}

WifiRadioGuard::~WifiRadioGuard() {
  if (locked_) radio_->unlock();
}
