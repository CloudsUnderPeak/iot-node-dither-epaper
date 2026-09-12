#pragma once
#include "queue.h"
constexpr int pdPASS = 1;
inline TickType_t pdMS_TO_TICKS(uint32_t ms) { return ms; }
struct NativeTask { void (*entry)(void *); void *context; };
using TaskHandle_t = NativeTask *;
inline TaskHandle_t nativeLastTask = nullptr;
inline bool nativeTaskCreateFails = false;
inline int xTaskCreate(void (*entry)(void *), const char *, size_t, void *context, int, TaskHandle_t *handle) {
  if (nativeTaskCreateFails) return 0;
  *handle = nativeLastTask = new NativeTask{entry, context};
  return pdPASS;
}
inline void nativeRunWorker() {
  try { nativeLastTask->entry(nativeLastTask->context); } catch (const NativeWorkerIdle &) {}
}
