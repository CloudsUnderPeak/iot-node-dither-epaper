#include "EpaperPowerProbe.h"

bool EpaperPowerProbe::run(
    Epd7In3E *driver,
    EpdTransport *transport,
    EpaperSafetyStore *safetyStore,
    CpuFrequencyDriver *frequencyDriver,
    EpaperShutdownCoordinator *shutdownCoordinator) {
  result_ = EpaperPowerProbeResult::None;
  driverError_ = EpdDriverError::None;
  if (driver == nullptr || transport == nullptr || !transport->ready() ||
      safetyStore == nullptr || !safetyStore->ready() ||
      frequencyDriver == nullptr || shutdownCoordinator == nullptr ||
      !shutdownCoordinator->ready()) {
    result_ = EpaperPowerProbeResult::InvalidDependency;
    return false;
  }
  if (safetyStore->stage() != EpaperProtectionStage::None) {
    result_ = EpaperPowerProbeResult::UnsafeExistingMarker;
    return false;
  }
  if (!safetyStore->markActive()) {
    result_ = EpaperPowerProbeResult::MarkerWriteFailed;
    return false;
  }

  CpuFrequencyGuard frequencyGuard;
  if (!frequencyGuard.acquire(frequencyDriver)) {
    shutdownCoordinator->finishOperation();
    result_ = EpaperPowerProbeResult::CpuFrequencyFailed;
    return false;
  }

  transport->delayMs(kPrewakeDelayMs);
  const bool initialized = driver->initialize();
  driverError_ = driver->lastError();
  const bool shutdownSafe = shutdownCoordinator->finishOperation();
  const bool frequencyRestored = frequencyGuard.release();

  if (!shutdownSafe) {
    result_ = EpaperPowerProbeResult::ShutdownFailed;
    return false;
  }
  if (!frequencyRestored) {
    result_ = EpaperPowerProbeResult::CpuRestoreFailed;
    return false;
  }
  if (!initialized) {
    result_ = EpaperPowerProbeResult::InitializeFailed;
    return false;
  }
  result_ = EpaperPowerProbeResult::Success;
  return true;
}

const char *epaperPowerProbeResultToString(EpaperPowerProbeResult result) {
  switch (result) {
    case EpaperPowerProbeResult::None:
      return "none";
    case EpaperPowerProbeResult::Success:
      return "success";
    case EpaperPowerProbeResult::UnsafeExistingMarker:
      return "unsafe_existing_marker";
    case EpaperPowerProbeResult::MarkerWriteFailed:
      return "marker_write_failed";
    case EpaperPowerProbeResult::CpuFrequencyFailed:
      return "cpu_frequency_failed";
    case EpaperPowerProbeResult::InitializeFailed:
      return "initialize_failed";
    case EpaperPowerProbeResult::ShutdownFailed:
      return "shutdown_failed";
    case EpaperPowerProbeResult::CpuRestoreFailed:
      return "cpu_restore_failed";
    case EpaperPowerProbeResult::InvalidDependency:
      return "invalid_dependency";
  }
  return "unknown";
}
