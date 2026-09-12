#pragma once
#include <cstdint>

enum class RestartRequest : uint8_t { Accepted, AlreadyPending, Rejected };
enum class RestartProgress : uint8_t { Idle, Draining, Ready, Failed };

class SystemRestartCoordinator {
 public:
  virtual ~SystemRestartCoordinator() = default;
  virtual RestartRequest requestRestart(uint32_t nowMs) = 0;
  virtual void pollRestart(uint32_t nowMs, bool allowRestart = true) = 0;
  virtual RestartProgress restartProgress() const = 0;
};
