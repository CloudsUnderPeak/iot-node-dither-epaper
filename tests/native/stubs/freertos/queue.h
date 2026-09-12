#pragma once
#include <cstring>
#include <deque>
#include <vector>
#include "FreeRTOS.h"
struct NativeQueue { size_t depth; size_t itemSize; std::deque<std::vector<uint8_t>> items; };
using QueueHandle_t = NativeQueue *;
inline QueueHandle_t xQueueCreate(size_t depth, size_t itemSize) { return new NativeQueue{depth, itemSize, {}}; }
inline int xQueueSend(QueueHandle_t queue, const void *item, TickType_t) {
  if (queue->items.size() >= queue->depth) return 0;
  auto *bytes = static_cast<const uint8_t *>(item);
  queue->items.emplace_back(bytes, bytes + queue->itemSize);
  return pdTRUE;
}
struct NativeWorkerIdle {};
inline int xQueueReceive(QueueHandle_t queue, void *item, TickType_t) {
  // A test drives the registered task until it waits; no production private
  // method is exposed and the real worker executes each accepted operation.
  if (queue->items.empty()) throw NativeWorkerIdle{};
  std::memcpy(item, queue->items.front().data(), queue->itemSize);
  queue->items.pop_front();
  return pdTRUE;
}
