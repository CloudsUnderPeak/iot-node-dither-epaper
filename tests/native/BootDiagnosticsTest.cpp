#include <cstdlib>
#include <iostream>
#include <string>

#include "modules/runtime/BootDiagnostics.h"

namespace {
int failures = 0;

void expect(bool condition, const char *message) {
  if (condition) return;
  ++failures;
  std::cerr << "FAIL: " << message << '\n';
}

void expectMapping(esp_reset_reason_t sdkReason,
                   DeviceResetReason projectReason,
                   const char *apiValue) {
  const DeviceResetReason normalized = deviceResetReasonFromEsp(sdkReason);
  expect(normalized == projectReason,
         "SDK reset reason should normalize to the expected project enum");
  expect(std::string(deviceResetReasonToString(normalized)) == apiValue,
         "project reset reason should map to the stable API string");
}

void testMappings() {
  expectMapping(ESP_RST_POWERON, DeviceResetReason::PowerOn, "power_on");
  expectMapping(ESP_RST_EXT, DeviceResetReason::External, "external");
  expectMapping(ESP_RST_SW, DeviceResetReason::Software, "software");
  expectMapping(ESP_RST_PANIC, DeviceResetReason::Panic, "panic");
  expectMapping(ESP_RST_INT_WDT, DeviceResetReason::Watchdog, "watchdog");
  expectMapping(ESP_RST_TASK_WDT, DeviceResetReason::Watchdog, "watchdog");
  expectMapping(ESP_RST_WDT, DeviceResetReason::Watchdog, "watchdog");
  expectMapping(ESP_RST_DEEPSLEEP, DeviceResetReason::DeepSleep, "deep_sleep");
  expectMapping(ESP_RST_BROWNOUT, DeviceResetReason::Brownout, "brownout");
  expectMapping(ESP_RST_UNKNOWN, DeviceResetReason::Unknown, "unknown");
  expect(std::string(deviceResetReasonToString(
             static_cast<DeviceResetReason>(0xff))) == "unknown",
         "unrecognized project reset reason should use the stable fallback");
}

void testImmutableCapture() {
  nativeEspResetReason = ESP_RST_BROWNOUT;
  BootDiagnostics diagnostics;
  diagnostics.capture();
  expect(diagnostics.snapshot().resetReason == DeviceResetReason::Brownout,
         "capture should snapshot the SDK reset reason");

  nativeEspResetReason = ESP_RST_SW;
  diagnostics.capture();
  expect(diagnostics.snapshot().resetReason == DeviceResetReason::Brownout,
         "later capture calls must not mutate the boot snapshot");
}
}  // namespace

int main() {
  testMappings();
  testImmutableCapture();
  if (failures != 0) return EXIT_FAILURE;
  std::cout << "Boot diagnostics tests passed\n";
  return EXIT_SUCCESS;
}
