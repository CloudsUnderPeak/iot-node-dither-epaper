#pragma once
#include <condition_variable>
#include <chrono>
#include <mutex>
#include <functional>
#include "FreeRTOS.h"

// Binary gates may be released by a different task. std::mutex alone cannot
// model that ownership, nor an initially empty FreeRTOS binary semaphore.
struct NativeSemaphore {
  std::mutex mutex;
  std::condition_variable changed;
  bool available = true;
};
inline thread_local std::function<void()> nativeSemaphoreBlocked;
inline thread_local std::function<void()> nativeAfterSemaphoreGive;
using SemaphoreHandle_t = NativeSemaphore *;
inline SemaphoreHandle_t xSemaphoreCreateMutex() { return new NativeSemaphore(); }
inline SemaphoreHandle_t xSemaphoreCreateBinary() {
  auto *value = new NativeSemaphore();
  value->available = false;
  return value;
}
inline int xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t ticks) {
  if (!semaphore) return 0;
  std::unique_lock<std::mutex> lock(semaphore->mutex);
  if (!semaphore->available && nativeSemaphoreBlocked) nativeSemaphoreBlocked();
  if (ticks == 0 && !semaphore->available) return 0;
  if (ticks == portMAX_DELAY) {
    semaphore->changed.wait(lock, [&] { return semaphore->available; });
  } else if (ticks != 0 && !semaphore->changed.wait_for(
                 lock, std::chrono::milliseconds(ticks),
                 [&] { return semaphore->available; })) {
    return 0;
  }
  semaphore->available = false;
  return pdTRUE;
}
inline int xSemaphoreGive(SemaphoreHandle_t semaphore) {
  if (!semaphore) return 0;
  std::unique_lock<std::mutex> lock(semaphore->mutex);
  if (semaphore->available) return 0;
  semaphore->available = true;
  semaphore->changed.notify_one();
  lock.unlock();
  if (nativeAfterSemaphoreGive) nativeAfterSemaphoreGive();
  return pdTRUE;
}
inline void vSemaphoreDelete(SemaphoreHandle_t semaphore) { delete semaphore; }
