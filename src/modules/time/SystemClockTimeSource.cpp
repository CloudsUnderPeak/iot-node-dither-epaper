#include "SystemClockTimeSource.h"

#include <sys/time.h>

bool SystemClockTimeSource::begin() {
  if (setterMutex_ == nullptr) setterMutex_ = xSemaphoreCreateMutex();
  return setterMutex_ != nullptr;
}

TimeSnapshot SystemClockTimeSource::snapshot() const {
  TimeSnapshot result;
  if (setterMutex_ == nullptr ||
      xSemaphoreTake(setterMutex_, pdMS_TO_TICKS(1000)) != pdTRUE) return result;
  struct timeval time = {};
  const bool clockRead = gettimeofday(&time, nullptr) == 0;
  if (clockRead) result.epoch = time.tv_sec;
  portENTER_CRITICAL(&mux_);
  result.origin = clockRead ? origin_ : TimeOrigin::None;
  result.revision = revision_;
  portEXIT_CRITICAL(&mux_);
  xSemaphoreGive(setterMutex_);
  if (!result.synced()) result.epoch = 0;
  return result;
}

bool SystemClockTimeSource::set(int64_t epoch, TimeOrigin origin) {
  if (epoch < 1704067200LL || epoch >= 4102444800LL ||
      origin == TimeOrigin::None) return false;
  if (setterMutex_ == nullptr ||
      xSemaphoreTake(setterMutex_, pdMS_TO_TICKS(1000)) != pdTRUE) return false;
  struct timeval time = {static_cast<time_t>(epoch), 0};
  if (settimeofday(&time, nullptr) != 0) {
    xSemaphoreGive(setterMutex_);
    return false;
  }
  struct timeval readBack = {};
  if (gettimeofday(&readBack, nullptr) != 0 ||
      readBack.tv_sec < epoch || readBack.tv_sec > epoch + 1) {
    portENTER_CRITICAL(&mux_);
    origin_ = TimeOrigin::None;
    ++revision_;
    portEXIT_CRITICAL(&mux_);
    xSemaphoreGive(setterMutex_);
    return false;
  }
  portENTER_CRITICAL(&mux_);
  origin_ = origin;
  ++revision_;
  portEXIT_CRITICAL(&mux_);
  xSemaphoreGive(setterMutex_);
  return true;
}

void SystemClockTimeSource::clear() {
  if (setterMutex_ == nullptr ||
      xSemaphoreTake(setterMutex_, pdMS_TO_TICKS(1000)) != pdTRUE) return;
  portENTER_CRITICAL(&mux_);
  origin_ = TimeOrigin::None;
  ++revision_;
  portEXIT_CRITICAL(&mux_);
  xSemaphoreGive(setterMutex_);
}

void SystemClockTimeSource::restoreCarried(bool evidenceValid) {
  if (setterMutex_ == nullptr ||
      xSemaphoreTake(setterMutex_, pdMS_TO_TICKS(1000)) != pdTRUE) return;
  portENTER_CRITICAL(&mux_);
  origin_ = evidenceValid ? TimeOrigin::Carried : TimeOrigin::None;
  ++revision_;
  portEXIT_CRITICAL(&mux_);
  xSemaphoreGive(setterMutex_);
}
