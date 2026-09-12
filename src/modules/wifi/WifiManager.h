#pragma once

#include <Arduino.h>
#include <IPAddress.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "../../core/Result.h"
#include "../config/model/DeviceConfig.h"
#include "MonotonicClock.h"
#include "WifiDriver.h"
#include "WifiRadio.h"

enum class WifiLinkState : uint8_t {
  Disabled,
  Connecting,
  Connected,
  Failed,
};

enum class WifiApState : uint8_t {
  Disabled,
  Starting,
  Active,
  Failed,
};

struct WifiStatus {
  WifiMode mode;
  bool staEnabled;
  bool apEnabled;
  WifiLinkState staState;
  WifiApState apState;
  IPAddress staIp;
  IPAddress apIp;
};

enum class WifiTestState : uint8_t {
  Idle,
  Queued,
  Testing,
  Succeeded,
  Committing,
  Finalizing,
  RollbackPending,
  RollingBack,
  Restoring,
  Failed,
  Expired,
};

enum class WifiTestFailure : uint8_t {
  None,
  ConnectTimeout,
  StationDisconnected,
  SubnetOverlap,
  IpConfigurationFailed,
  RadioUnavailable,
  ManagementApUnavailable,
  ApConfigurationFailed,
  RuntimeApplyFailed,
  StorageError,
};

struct WifiTestStatus {
  uint32_t testId = 0;
  WifiTestState state = WifiTestState::Idle;
  WifiTestFailure failure = WifiTestFailure::None;
  IPAddress staIp;
  uint32_t commitExpiresInSeconds = 0;
  uint32_t apShutdownInSeconds = 0;
  bool persisted = false;
};

// Applies the persisted Wi-Fi intent to the Arduino WiFi runtime and keeps the
// latest observed status for serial logs and REST responses.
class WifiManager {
 public:
  Result begin(WifiRadio *radio,
               WifiDriver *driver,
               MonotonicClock *clock);
  Result apply(const DeviceConfig &config, WifiStatus &status);
  Result applyTxPower(const DeviceConfig &config);
  bool poll(const DeviceConfig &config);
  WifiStatus status() const;
  Result queueStaTest(const DeviceConfig &candidate,
                      const DeviceConfig &previous,
                      uint32_t &testId);
  WifiTestStatus testStatus() const;
  bool testBlocksScan() const;
  bool scanBlocksRadio() const { return radio_ != nullptr && radio_->scanReserved(); }
  bool staConnectionBlocksScan() const;
  Result prepareTestCommit(uint32_t testId, DeviceConfig &candidate);
  void finishTestCommit(uint32_t testId, bool committed);
  Result prepareTestRollback(uint32_t testId, DeviceConfig &previous);
  void finishTestRollback(uint32_t testId, bool restored);

 private:
  WifiRadio *radio_ = nullptr;
  WifiDriver *driver_ = nullptr;
  MonotonicClock *clock_ = nullptr;
  WifiStatus status_{WifiMode::Off, false, false, WifiLinkState::Disabled, WifiApState::Disabled, IPAddress(), IPAddress()};
  uint32_t connectStartedMs_ = 0;
  mutable portMUX_TYPE statusMux_ = portMUX_INITIALIZER_UNLOCKED;
  mutable SemaphoreHandle_t testMutex_ = nullptr;
  WifiTestState testState_ = WifiTestState::Idle;
  WifiTestFailure testFailure_ = WifiTestFailure::None;
  DeviceConfig testCandidate_{};
  DeviceConfig testPrevious_{};
  uint32_t testId_ = 0;
  uint32_t nextTestId_ = 1;
  uint32_t testConnectStartedMs_ = 0;
  uint32_t testCommitDeadlineMs_ = 0;
  uint32_t testFinalizeDeadlineMs_ = 0;
  bool testCommitCompleted_ = false;
 IPAddress testStaIp_;

  struct TestPollSnapshot {
    WifiTestState state = WifiTestState::Idle;
    DeviceConfig candidate{};
    DeviceConfig previous{};
    uint32_t testId = 0;
    uint32_t startedMs = 0;
    uint32_t commitDeadlineMs = 0;
    uint32_t finalizeDeadlineMs = 0;
    bool committed = false;
  };

  void setStatus(const WifiStatus &status);
  bool pollStaTest(WifiStatus &current, bool &changed);
  bool readTestPollSnapshot(TestPollSnapshot &snapshot) const;
  bool pollQueuedStaTest(const TestPollSnapshot &snapshot,
                         WifiStatus &current,
                         bool &changed);
  bool pollTestingStaTest(const TestPollSnapshot &snapshot,
                          WifiStatus &current,
                          bool &changed);
  bool pollSucceededStaTest(const TestPollSnapshot &snapshot,
                            WifiStatus &current,
                            bool &changed);
  bool pollFinalizingStaTest(const TestPollSnapshot &snapshot,
                             WifiStatus &current,
                             bool &changed);
  bool pollRestoringStaTest(const TestPollSnapshot &snapshot,
                            WifiStatus &current,
                            bool &changed);
  Result restorePersistedAfterTest(const DeviceConfig &persisted, WifiStatus &current);
  bool failTest(uint32_t testId,
                WifiTestState expectedState,
                WifiTestFailure failure,
                WifiStatus &current);
  bool beginRollback(uint32_t testId,
                     WifiTestState expectedState,
                     WifiTestFailure failure);
  void finishTestSuccess(uint32_t testId);
  void resetTestLocked(WifiTestState state);
  bool lockTest() const;
  void unlockTest() const;
  void clearTestConfigsLocked();
};

const char *wifiLinkStateToString(WifiLinkState state);
const char *wifiApStateToString(WifiApState state);
const char *wifiTestStateToString(WifiTestState state);
const char *wifiTestFailureToString(WifiTestFailure failure);
