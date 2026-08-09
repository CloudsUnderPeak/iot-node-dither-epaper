#include "EpaperRefreshProbe.h"

bool EpaperRefreshProbe::run(
    Epd7In3E *driver,
    EpdTransport *transport,
    const EpaperFrameSource *source,
    EpaperSafetyStore *safetyStore,
    CpuFrequencyDriver *frequencyDriver,
    EpaperShutdownCoordinator *shutdownCoordinator) {
  result_ = EpaperRefreshProbeResult::None;
  driverError_ = EpdDriverError::None;
  transferredBytes_ = 0;
  if (driver == nullptr || transport == nullptr || !transport->ready() ||
      source == nullptr || safetyStore == nullptr || !safetyStore->ready() ||
      frequencyDriver == nullptr || shutdownCoordinator == nullptr ||
      !shutdownCoordinator->ready()) {
    result_ = EpaperRefreshProbeResult::InvalidDependency;
    return false;
  }
  if (safetyStore->stage() != EpaperProtectionStage::None) {
    result_ = EpaperRefreshProbeResult::UnsafeExistingMarker;
    return false;
  }
  if (!safetyStore->markActive()) {
    result_ = EpaperRefreshProbeResult::MarkerWriteFailed;
    return false;
  }

  CpuFrequencyGuard frequencyGuard;
  if (!frequencyGuard.acquire(frequencyDriver)) {
    shutdownCoordinator->finishOperation();
    result_ = EpaperRefreshProbeResult::CpuFrequencyFailed;
    return false;
  }

  transport->delayMs(kPrewakeDelayMs);
  const bool initialized = driver->initialize();
  bool refreshed = false;
  if (initialized) refreshed = driver->transferAndRefresh(*source);
  driverError_ = driver->lastError();
  transferredBytes_ = driver->transferredBytes();

  // Cleanup is unconditional once the persistent active marker is written.
  const bool shutdownSafe = shutdownCoordinator->finishOperation();
  const bool frequencyRestored = frequencyGuard.release();

  if (!shutdownSafe) {
    result_ = EpaperRefreshProbeResult::ShutdownFailed;
    return false;
  }
  if (!frequencyRestored) {
    result_ = EpaperRefreshProbeResult::CpuRestoreFailed;
    return false;
  }
  if (!initialized) {
    result_ = EpaperRefreshProbeResult::InitializeFailed;
    return false;
  }
  if (!refreshed) {
    result_ = EpaperRefreshProbeResult::RefreshFailed;
    return false;
  }
  result_ = EpaperRefreshProbeResult::Success;
  return true;
}

const char *epaperRefreshProbeResultToString(EpaperRefreshProbeResult result) {
  switch (result) {
    case EpaperRefreshProbeResult::None:
      return "none";
    case EpaperRefreshProbeResult::Success:
      return "success";
    case EpaperRefreshProbeResult::UnsafeExistingMarker:
      return "unsafe_existing_marker";
    case EpaperRefreshProbeResult::MarkerWriteFailed:
      return "marker_write_failed";
    case EpaperRefreshProbeResult::CpuFrequencyFailed:
      return "cpu_frequency_failed";
    case EpaperRefreshProbeResult::InitializeFailed:
      return "initialize_failed";
    case EpaperRefreshProbeResult::RefreshFailed:
      return "refresh_failed";
    case EpaperRefreshProbeResult::ShutdownFailed:
      return "shutdown_failed";
    case EpaperRefreshProbeResult::CpuRestoreFailed:
      return "cpu_restore_failed";
    case EpaperRefreshProbeResult::InvalidDependency:
      return "invalid_dependency";
  }
  return "unknown";
}
