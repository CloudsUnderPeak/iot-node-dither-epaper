#pragma once

#include <atomic>
#include <cstdint>

inline uint32_t esp_random() {
  static std::atomic<uint32_t> sequence{0x13579bdfU};
  return sequence.fetch_add(0x9e3779b9U, std::memory_order_relaxed);
}
