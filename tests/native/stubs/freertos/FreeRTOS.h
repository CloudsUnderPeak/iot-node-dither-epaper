#pragma once

#include <cstdint>
#include <mutex>

using TickType_t = uint32_t;

constexpr int pdTRUE = 1;
constexpr TickType_t portMAX_DELAY = UINT32_MAX;

struct portMUX_TYPE {
  std::mutex mutex;
};

#define portMUX_INITIALIZER_UNLOCKED {}

inline void portENTER_CRITICAL(portMUX_TYPE *mux) {
  mux->mutex.lock();
}

inline void portEXIT_CRITICAL(portMUX_TYPE *mux) {
  mux->mutex.unlock();
}
