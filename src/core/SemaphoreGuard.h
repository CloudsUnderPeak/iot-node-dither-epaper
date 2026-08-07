#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class SemaphoreGuard {
 public:
  SemaphoreGuard(SemaphoreHandle_t semaphore, TickType_t timeoutTicks)
      : semaphore_(semaphore),
        locked_(semaphore != nullptr &&
                xSemaphoreTake(semaphore, timeoutTicks) == pdTRUE) {}

  ~SemaphoreGuard() {
    if (locked_) xSemaphoreGive(semaphore_);
  }

  SemaphoreGuard(const SemaphoreGuard &) = delete;
  SemaphoreGuard &operator=(const SemaphoreGuard &) = delete;

  bool locked() const { return locked_; }

 private:
  SemaphoreHandle_t semaphore_;
  bool locked_;
};
