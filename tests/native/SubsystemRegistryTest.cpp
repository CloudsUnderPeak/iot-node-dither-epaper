#include <cstdlib>
#include <iostream>

#include "core/SubsystemRegistry.h"

namespace {
int failures = 0;
int startOrder = 0;
bool dynamicHealth = true;

void expect(bool condition, const char *message) {
  if (condition) return;
  ++failures;
  std::cerr << "FAIL: " << message << '\n';
}

Result startFirst() {
  expect(startOrder == 0, "first subsystem must start first");
  ++startOrder;
  return okResult();
}

Result startSecond() {
  expect(startOrder == 1, "second subsystem must start second");
  ++startOrder;
  return okResult();
}

Result failStart() {
  return networkError("start failed");
}

bool healthCheck() {
  return dynamicHealth;
}
}  // namespace

int main() {
  Subsystem subsystems[] = {
      {"first", startFirst},
      {"second", startSecond, healthCheck},
      {"failed", failStart},
      {"missing", nullptr},
  };

  for (Subsystem &subsystem : subsystems) {
    startSubsystem(subsystem);
  }

  expect(startOrder == 2, "registry iteration must preserve declaration order");
  expect(subsystems[0].ready && subsystemHealthy(subsystems[0]),
         "successful subsystem without health callback should be ready");
  expect(subsystems[1].ready && subsystemHealthy(subsystems[1]),
         "successful subsystem with healthy callback should be ready");
  expect(!subsystems[2].ready && !subsystemHealthy(subsystems[2]),
         "failed start must not publish ready");
  expect(!subsystems[3].ready && !subsystemHealthy(subsystems[3]),
         "missing start callback must not publish ready");

  dynamicHealth = false;
  expect(subsystems[1].ready && !subsystemHealthy(subsystems[1]),
         "runtime health failure must not erase successful startup state");

  if (failures != 0) {
    std::cerr << failures << " subsystem registry test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "Subsystem registry lifecycle tests passed\n";
  return EXIT_SUCCESS;
}
