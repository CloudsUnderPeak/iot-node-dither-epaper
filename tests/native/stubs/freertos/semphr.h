#pragma once

#include <mutex>

#include "FreeRTOS.h"

struct NativeSemaphore {
  std::mutex mutex;
};

using SemaphoreHandle_t = NativeSemaphore *;

inline SemaphoreHandle_t xSemaphoreCreateMutex() {
  return new NativeSemaphore();
}

inline int xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t) {
  if (semaphore == nullptr) return 0;
  semaphore->mutex.lock();
  return pdTRUE;
}

inline int xSemaphoreGive(SemaphoreHandle_t semaphore) {
  if (semaphore == nullptr) return 0;
  semaphore->mutex.unlock();
  return pdTRUE;
}
