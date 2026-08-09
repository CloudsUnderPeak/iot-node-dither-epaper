#pragma once

#include <cstdint>

class EpaperCooldown {
 public:
  static constexpr uint32_t kDurationMs = 180000;

  enum class State {
    Idle,
    CoolingDown,
    Unavailable,
  };

  State state() const { return state_; }
  bool canDraw() const { return state_ == State::Idle; }

  void begin(uint32_t nowMs);
  void markUnavailable();

  bool elapsed(uint32_t nowMs) const;
  uint32_t remainingMs(uint32_t nowMs) const;
  uint32_t retryAfterSeconds(uint32_t nowMs) const;

  // The caller may release the gate only after the persistent protection
  // marker was successfully cleared and read back.
  bool releaseIfElapsed(uint32_t nowMs, bool markerCleared);

 private:
  State state_ = State::Idle;
  uint32_t startedAtMs_ = 0;
};
