#pragma once

#include <cstdint>

#include "CpuFrequencyGuard.h"
#include "EpaperFrameSource.h"
#include "EpaperSafetyStore.h"
#include "EpaperShutdownCoordinator.h"
#include "Epd7In3E.h"
#include "EpdTransport.h"

enum class EpaperRefreshProbeResult : uint8_t {
  None,
  Success,
  UnsafeExistingMarker,
  MarkerWriteFailed,
  CpuFrequencyFailed,
  InitializeFailed,
  RefreshFailed,
  ShutdownFailed,
  CpuRestoreFailed,
  InvalidDependency,
};

class EpaperRefreshProbe {
 public:
  static constexpr uint32_t kPrewakeDelayMs = 2000;

  bool run(Epd7In3E *driver,
           EpdTransport *transport,
           const EpaperFrameSource *source,
           EpaperSafetyStore *safetyStore,
           CpuFrequencyDriver *frequencyDriver,
           EpaperShutdownCoordinator *shutdownCoordinator);

  EpaperRefreshProbeResult result() const { return result_; }
  EpdDriverError driverError() const { return driverError_; }
  size_t transferredBytes() const { return transferredBytes_; }

 private:
  EpaperRefreshProbeResult result_ = EpaperRefreshProbeResult::None;
  EpdDriverError driverError_ = EpdDriverError::None;
  size_t transferredBytes_ = 0;
};

const char *epaperRefreshProbeResultToString(EpaperRefreshProbeResult result);
