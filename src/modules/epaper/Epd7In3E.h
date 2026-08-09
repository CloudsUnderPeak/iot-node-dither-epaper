#pragma once

#include <cstddef>
#include <cstdint>

#include "EpaperFrameSource.h"
#include "EpdTransport.h"

enum class EpdDriverError : uint8_t {
  None,
  InvalidState,
  TransportFailure,
  BusyTimeout,
  BusyNeverAsserted,
  OperationWatchdogTimeout,
  SourceReadFailed,
  FrameSizeMismatch,
  PowerOffFailed,
  SleepFailed,
};

class Epd7In3E {
 public:
  static constexpr size_t kTransferChunkBytes = 4096;
  static constexpr uint32_t kOperationWatchdogMs = 90000;

  enum class State : uint8_t {
    Uninitialized,
    Quiesced,
    Initializing,
    Powered,
    FrameTransferred,
    Refreshed,
    Sleeping,
    Fault,
  };

  bool begin(EpdTransport *transport);
  bool initialize();
  bool transferFrame(const EpaperFrameSource &source);
  bool refresh();
  bool transferAndRefresh(const EpaperFrameSource &source);
  bool shutdown();
  void logicalQuiesce();

  State state() const { return state_; }
  EpdDriverError lastError() const { return lastError_; }
  bool panelMayBeActive() const { return panelMayBeActive_; }
  size_t transferredBytes() const { return transferredBytes_; }

 private:
  static constexpr uint32_t kResetTimeoutMs = 10000;
  static constexpr uint32_t kPowerTimeoutMs = 15000;
  static constexpr uint32_t kBusyAssertTimeoutMs = 2000;
  static constexpr uint32_t kRefreshTimeoutMs = 60000;
  static constexpr uint32_t kPowerOffTimeoutMs = 15000;

  EpdTransport *transport_ = nullptr;
  State state_ = State::Uninitialized;
  EpdDriverError lastError_ = EpdDriverError::None;
  uint32_t operationStartedAtMs_ = 0;
  bool operationStarted_ = false;
  bool panelMayBeActive_ = false;
  size_t transferredBytes_ = 0;
  uint8_t transferBuffer_[kTransferChunkBytes]{};

  bool fail(EpdDriverError error);
  bool withinOperationBudget();
  bool sendCommandData(uint8_t command, const uint8_t *data, size_t length);
  bool waitUntilIdle(uint32_t timeoutMs,
                     EpdDriverError timeoutError,
                     bool enforceOperationBudget = true);
  bool waitForBusyCycle();
};
