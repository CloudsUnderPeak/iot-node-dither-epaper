#pragma once

#include <cstdint>

class IdleTimer {
 public:
  explicit IdleTimer(uint32_t timeoutSeconds) : timeoutMs_(timeoutSeconds * 1000U) {}
  void arm(uint32_t nowMs) { armed_ = true; expired_ = false; lastActivityMs_ = nowMs; }
  void disarm() { armed_ = false; expired_ = false; }
  void noteActivity(uint32_t nowMs) { if (armed_) arm(nowMs); }
  bool armed() const { return armed_; }
  bool expired(uint32_t nowMs) {
    if (armed_ && !expired_ && static_cast<uint32_t>(nowMs - lastActivityMs_) >= timeoutMs_) {
      expired_ = true;
    }
    return armed_ && expired_;
  }
  uint32_t remainingSeconds(uint32_t nowMs) {
    if (!armed_ || expired(nowMs)) return 0;
    const uint32_t elapsed = static_cast<uint32_t>(nowMs - lastActivityMs_);
    return (timeoutMs_ - elapsed + 999U) / 1000U;
  }
 private:
  uint32_t timeoutMs_;
  uint32_t lastActivityMs_ = 0;
  bool armed_ = false;
  bool expired_ = false;
};
