#pragma once

#include <Arduino.h>

#include "modules/config/model/DeviceConfig.h"

enum class WifiLinkState : uint8_t { Disabled, Connecting, Connected, Failed };
enum class WifiApState : uint8_t { Disabled, Starting, Active, Failed };
enum class WifiTestState : uint8_t {
  Idle, Queued, Testing, Succeeded, Committing, Finalizing,
  RollbackPending, RollingBack, Restoring, Failed, Expired
};
enum class WifiTestFailure : uint8_t {
  None, ConnectTimeout, StationDisconnected, SubnetOverlap,
  IpConfigurationFailed, RadioUnavailable, ManagementApUnavailable,
  ApConfigurationFailed, RuntimeApplyFailed, StorageError
};

class IPAddress {
 public:
  String toString() const { return "0.0.0.0"; }
};

struct WifiStatus {
  WifiMode mode = WifiMode::Off;
  bool staEnabled = false;
  bool apEnabled = false;
  WifiLinkState staState = WifiLinkState::Disabled;
  WifiApState apState = WifiApState::Disabled;
  IPAddress staIp;
  IPAddress apIp;
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

inline const char *wifiLinkStateToString(WifiLinkState state) {
  return state == WifiLinkState::Connected ? "connected" :
         state == WifiLinkState::Connecting ? "connecting" :
         state == WifiLinkState::Failed ? "failed" : "disabled";
}

inline const char *wifiApStateToString(WifiApState state) {
  return state == WifiApState::Active ? "active" :
         state == WifiApState::Starting ? "starting" :
         state == WifiApState::Failed ? "failed" : "disabled";
}

inline const char *wifiTestStateToString(WifiTestState state) {
  switch (state) {
    case WifiTestState::Idle: return "idle";
    case WifiTestState::Queued: return "queued";
    case WifiTestState::Testing: return "testing";
    case WifiTestState::Succeeded: return "succeeded";
    case WifiTestState::Committing: return "committing";
    case WifiTestState::Finalizing: return "finalizing";
    case WifiTestState::RollbackPending: return "rollback_pending";
    case WifiTestState::RollingBack: return "rolling_back";
    case WifiTestState::Restoring: return "restoring";
    case WifiTestState::Failed: return "failed";
    case WifiTestState::Expired: return "expired";
  }
  return "unknown";
}

inline const char *wifiTestFailureToString(WifiTestFailure failure) {
  return failure == WifiTestFailure::ConnectTimeout ? "connect_timeout" :
         failure == WifiTestFailure::StationDisconnected ? "station_disconnected" :
         failure == WifiTestFailure::SubnetOverlap ? "subnet_overlap" :
         failure == WifiTestFailure::IpConfigurationFailed ? "ip_configuration_failed" :
         failure == WifiTestFailure::RadioUnavailable ? "radio_unavailable" :
         failure == WifiTestFailure::ManagementApUnavailable ? "management_ap_unavailable" :
         failure == WifiTestFailure::ApConfigurationFailed ? "ap_configuration_failed" :
         failure == WifiTestFailure::RuntimeApplyFailed ? "runtime_apply_failed" :
         failure == WifiTestFailure::StorageError ? "storage_error" : "none";
}

class WifiManager {
 public:
  WifiStatus value;
  WifiTestStatus testValue;
  Result queueResult = okResult();
  Result prepareResult = okResult();
  DeviceConfig candidate = defaultDeviceConfig();
  DeviceConfig previous = defaultDeviceConfig();
  unsigned queueCount = 0;
  unsigned finishCount = 0;
  bool committed = false;
  bool blocksScan = false;
  bool staBlocksScan = false;
  WifiStatus status() const { return value; }
  Result queueStaTest(const DeviceConfig &updated,
                      const DeviceConfig &prior,
                      uint32_t &testId) {
    ++queueCount;
    if (!queueResult.ok()) return queueResult;
    candidate = updated;
    previous = prior;
    testValue.testId = 7;
    testValue.state = WifiTestState::Queued;
    testId = testValue.testId;
    return okResult();
  }
  WifiTestStatus testStatus() const { return testValue; }
  bool testBlocksScan() const { return blocksScan; }
  bool staConnectionBlocksScan() const { return staBlocksScan; }
  Result prepareTestCommit(uint32_t testId, DeviceConfig &updated) {
    if (!prepareResult.ok()) return prepareResult;
    if (testId != testValue.testId) return notFound("wifi test id is stale");
    updated = candidate;
    testValue.state = WifiTestState::Committing;
    return okResult();
  }
  void finishTestCommit(uint32_t, bool success) {
    ++finishCount;
    committed = success;
  }
};
