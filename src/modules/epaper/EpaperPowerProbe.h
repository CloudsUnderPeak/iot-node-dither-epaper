#pragma once

#include <cstdint>

#include "CpuFrequencyGuard.h"
#include "EpaperSafetyStore.h"
#include "EpaperShutdownCoordinator.h"
#include "Epd7In3E.h"
#include "EpdTransport.h"

enum class EpaperPowerProbeResult : uint8_t {
  None,
  Success,
  UnsafeExistingMarker,
  MarkerWriteFailed,
  CpuFrequencyFailed,
  InitializeFailed,
  ShutdownFailed,
  CpuRestoreFailed,
  InvalidDependency,
};

class EpaperPowerProbe {
 public:
  static constexpr uint32_t kPrewakeDelayMs = 2000;

  bool run(Epd7In3E *driver,
           EpdTransport *transport,
           EpaperSafetyStore *safetyStore,
           CpuFrequencyDriver *frequencyDriver,
           EpaperShutdownCoordinator *shutdownCoordinator);

  EpaperPowerProbeResult result() const { return result_; }
  EpdDriverError driverError() const { return driverError_; }

 private:
  EpaperPowerProbeResult result_ = EpaperPowerProbeResult::None;
  EpdDriverError driverError_ = EpdDriverError::None;
};

const char *epaperPowerProbeResultToString(EpaperPowerProbeResult result);
