#include "EpaperCooldown.h"

void EpaperCooldown::begin(uint32_t nowMs) {
  startedAtMs_ = nowMs;
  state_ = State::CoolingDown;
}

void EpaperCooldown::markUnavailable() {
  state_ = State::Unavailable;
}

bool EpaperCooldown::elapsed(uint32_t nowMs) const {
  return state_ == State::CoolingDown &&
         static_cast<uint32_t>(nowMs - startedAtMs_) >= kDurationMs;
}

uint32_t EpaperCooldown::remainingMs(uint32_t nowMs) const {
  if (state_ != State::CoolingDown) return 0;
  const uint32_t elapsedMs = static_cast<uint32_t>(nowMs - startedAtMs_);
  return elapsedMs >= kDurationMs ? 0 : kDurationMs - elapsedMs;
}

uint32_t EpaperCooldown::retryAfterSeconds(uint32_t nowMs) const {
  const uint32_t remaining = remainingMs(nowMs);
  return remaining == 0 ? 0 : (remaining + 999U) / 1000U;
}

bool EpaperCooldown::releaseIfElapsed(uint32_t nowMs, bool markerCleared) {
  if (!elapsed(nowMs) || !markerCleared) return false;
  state_ = State::Idle;
  return true;
}
