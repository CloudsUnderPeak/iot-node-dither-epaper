#pragma once

#include <cstdint>
#include <esp_system.h>

enum class DeviceResetReason : uint8_t {
  PowerOn,
  External,
  Software,
  Panic,
  Watchdog,
  DeepSleep,
  Brownout,
  Unknown,
};

struct BootDiagnosticsSnapshot {
  DeviceResetReason resetReason = DeviceResetReason::Unknown;
};

DeviceResetReason deviceResetReasonFromEsp(esp_reset_reason_t reason);
const char *deviceResetReasonToString(DeviceResetReason reason);

class BootDiagnostics {
 public:
  BootDiagnostics() = default;
  explicit BootDiagnostics(DeviceResetReason resetReason)
      : snapshot_{resetReason}, captured_(true) {}

  void capture();
  const BootDiagnosticsSnapshot &snapshot() const { return snapshot_; }

 private:
  BootDiagnosticsSnapshot snapshot_;
  bool captured_ = false;
};
