#pragma once

#include <cstdint>

#include "EpaperSafetyStore.h"
#include "Epd7In3E.h"
#include "modules/runtime/SystemRestartCoordinator.h"

class RestartDriver {
 public:
  virtual ~RestartDriver() = default;
  virtual void restart() = 0;
};

enum class EpaperShutdownOutcome : uint8_t {
  None,
  SafeWithoutWake,
  ShutdownConfirmed,
  ProtocolFailed,
  MarkerFailed,
  UnsafeMarker,
  NotReady,
};

class EpaperShutdownCoordinator final : public SystemRestartCoordinator {
 public:
  bool begin(Epd7In3E *driver,
             EpaperSafetyStore *safetyStore,
             RestartDriver *restartDriver);

  // Completes an operation after its active marker was persisted. A failure
  // before panel Power ON clears the marker; once the panel may be active,
  // protocol Power OFF and Deep Sleep plus marker read-back are mandatory.
  bool finishOperation();
  bool restartNow() override;

  bool ready() const { return ready_; }
  bool unavailable() const { return unavailable_; }
  EpaperShutdownOutcome lastOutcome() const { return lastOutcome_; }

 private:
  Epd7In3E *driver_ = nullptr;
  EpaperSafetyStore *safetyStore_ = nullptr;
  RestartDriver *restartDriver_ = nullptr;
  bool ready_ = false;
  bool unavailable_ = false;
  EpaperShutdownOutcome lastOutcome_ = EpaperShutdownOutcome::None;

  bool fail(EpaperShutdownOutcome outcome);
};

const char *epaperShutdownOutcomeToString(EpaperShutdownOutcome outcome);
