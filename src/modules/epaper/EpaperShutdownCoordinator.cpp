#include "EpaperShutdownCoordinator.h"

bool EpaperShutdownCoordinator::begin(Epd7In3E *driver,
                                      EpaperSafetyStore *safetyStore,
                                      RestartDriver *restartDriver) {
  ready_ = driver != nullptr && safetyStore != nullptr &&
           safetyStore->ready() && restartDriver != nullptr;
  if (!ready_) {
    lastOutcome_ = EpaperShutdownOutcome::NotReady;
    return false;
  }
  driver_ = driver;
  safetyStore_ = safetyStore;
  restartDriver_ = restartDriver;
  unavailable_ = safetyStore_->stage() == EpaperProtectionStage::Active;
  lastOutcome_ = unavailable_ ? EpaperShutdownOutcome::UnsafeMarker
                              : EpaperShutdownOutcome::None;
  return true;
}

bool EpaperShutdownCoordinator::fail(EpaperShutdownOutcome outcome) {
  if (driver_ != nullptr) driver_->logicalQuiesce();
  lastOutcome_ = outcome;
  unavailable_ = true;
  return false;
}

bool EpaperShutdownCoordinator::finishOperation() {
  if (!ready_ || safetyStore_->stage() != EpaperProtectionStage::Active) {
    return fail(EpaperShutdownOutcome::NotReady);
  }

  if (!driver_->panelMayBeActive()) {
    driver_->logicalQuiesce();
    if (!safetyStore_->clear()) {
      return fail(EpaperShutdownOutcome::MarkerFailed);
    }
    lastOutcome_ = EpaperShutdownOutcome::SafeWithoutWake;
    unavailable_ = false;
    return true;
  }

  if (!driver_->shutdown()) {
    return fail(EpaperShutdownOutcome::ProtocolFailed);
  }
  if (!safetyStore_->markShutdownConfirmed()) {
    return fail(EpaperShutdownOutcome::MarkerFailed);
  }
  lastOutcome_ = EpaperShutdownOutcome::ShutdownConfirmed;
  unavailable_ = false;
  return true;
}

bool EpaperShutdownCoordinator::restartNow() {
  if (!ready_ || unavailable_) return fail(EpaperShutdownOutcome::UnsafeMarker);

  if (driver_->panelMayBeActive()) {
    if (safetyStore_->stage() != EpaperProtectionStage::Active ||
        !finishOperation()) {
      return false;
    }
  } else if (safetyStore_->stage() == EpaperProtectionStage::Active) {
    // An active marker loaded at boot represents an interrupted operation;
    // software restart is not a full power-cycle recovery.
    return fail(EpaperShutdownOutcome::UnsafeMarker);
  } else {
    driver_->logicalQuiesce();
    lastOutcome_ = EpaperShutdownOutcome::SafeWithoutWake;
  }

  restartDriver_->restart();
  return true;
}

const char *epaperShutdownOutcomeToString(EpaperShutdownOutcome outcome) {
  switch (outcome) {
    case EpaperShutdownOutcome::None:
      return "none";
    case EpaperShutdownOutcome::SafeWithoutWake:
      return "safe_without_wake";
    case EpaperShutdownOutcome::ShutdownConfirmed:
      return "shutdown_confirmed";
    case EpaperShutdownOutcome::ProtocolFailed:
      return "protocol_failed";
    case EpaperShutdownOutcome::MarkerFailed:
      return "marker_failed";
    case EpaperShutdownOutcome::UnsafeMarker:
      return "unsafe_marker";
    case EpaperShutdownOutcome::NotReady:
      return "not_ready";
  }
  return "unknown";
}
