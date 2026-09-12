#include "BootDiagnostics.h"

DeviceResetReason deviceResetReasonFromEsp(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON: return DeviceResetReason::PowerOn;
    case ESP_RST_EXT: return DeviceResetReason::External;
    case ESP_RST_SW: return DeviceResetReason::Software;
    case ESP_RST_PANIC: return DeviceResetReason::Panic;
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return DeviceResetReason::Watchdog;
    case ESP_RST_DEEPSLEEP: return DeviceResetReason::DeepSleep;
    case ESP_RST_BROWNOUT: return DeviceResetReason::Brownout;
    default: return DeviceResetReason::Unknown;
  }
}

const char *deviceResetReasonToString(DeviceResetReason reason) {
  switch (reason) {
    case DeviceResetReason::PowerOn: return "power_on";
    case DeviceResetReason::External: return "external";
    case DeviceResetReason::Software: return "software";
    case DeviceResetReason::Panic: return "panic";
    case DeviceResetReason::Watchdog: return "watchdog";
    case DeviceResetReason::DeepSleep: return "deep_sleep";
    case DeviceResetReason::Brownout: return "brownout";
    case DeviceResetReason::Unknown: return "unknown";
  }
  return "unknown";
}

void BootDiagnostics::capture() {
  if (captured_) return;
  snapshot_.resetReason = deviceResetReasonFromEsp(esp_reset_reason());
  captured_ = true;
}
