#include "WifiManager.h"

#include <cstring>
#include <esp32-hal-log.h>

namespace {
constexpr uint32_t kStaConnectTimeoutMs = 15000;
constexpr uint32_t kStaTestCommitWindowMs = 60000;
constexpr uint32_t kApShutdownGraceMs = 5000;

bool deadlineReached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

const char *apPasswordFor(const DeviceConfig &config) {
  return config.apPasswordEnabled ? config.adminPassword : nullptr;
}

Result startSoftAp(WifiDriver &driver,
                   const DeviceConfig &config,
                   WifiStatus &status) {
  status.apState = WifiApState::Starting;
  IPAddress localIp;
  IPAddress netmask;
  localIp.fromString(config.apIpAddress);
  netmask.fromString(config.apIpNetmask);
  // Advertise the SoftAP itself as DNS so captive-portal probes reach the
  // wildcard DNS server even when the client also has another online adapter.
  if (!driver.configureAp(localIp, localIp, netmask, IPAddress())) {
    status.apEnabled = false;
    status.apState = WifiApState::Failed;
    return networkError("failed to configure AP IPv4");
  }
  if (!driver.startAp(config.apSsid, apPasswordFor(config))) {
    status.apEnabled = false;
    status.apState = WifiApState::Failed;
    return networkError("failed to start AP");
  }

  // DHCP option 114 improves portal discovery on clients that support it.
  // DNS interception and direct AP-IP access remain available as fallbacks.
  if (!driver.enableDhcpCaptivePortal()) {
    log_w("failed to advertise DHCP captive portal URI");
  }

  status.apIp = driver.apIp();
  status.apEnabled = status.apIp != IPAddress(0, 0, 0, 0);
  status.apState = status.apEnabled ? WifiApState::Active : WifiApState::Failed;
  return status.apEnabled ? okResult() : networkError("failed to obtain AP IPv4");
}

Result configureStationIp(WifiDriver &driver, const DeviceConfig &config) {
  if (config.staIpMode == StaIpMode::Dhcp) {
    return driver.configureStation(IPAddress(), IPAddress(), IPAddress())
               ? okResult()
               : networkError("failed to enable STA DHCP");
  }

  IPAddress localIp;
  IPAddress gateway;
  IPAddress netmask;
  IPAddress dns1;
  IPAddress dns2;
  localIp.fromString(config.staIpAddress);
  gateway.fromString(config.staIpGateway);
  netmask.fromString(config.staIpNetmask);
  if (config.staDns1[0] != '\0') dns1.fromString(config.staDns1);
  if (config.staDns2[0] != '\0') dns2.fromString(config.staDns2);
  return driver.configureStation(localIp, gateway, netmask, dns1, dns2)
             ? okResult()
             : networkError("failed to configure STA static IPv4");
}

bool runtimeSubnetsOverlap(const WifiDriver &driver,
                           const DeviceConfig &config) {
  const String staIp = driver.stationIp().toString();
  const String staNetmask = driver.stationNetmask().toString();
  return ipv4SubnetsOverlap(staIp.c_str(),
                            staNetmask.c_str(),
                            config.apIpAddress,
                            config.apIpNetmask);
}

bool stationHasIpv4(const WifiDriver &driver) {
  return driver.stationConnected() &&
         driver.stationIp() != IPAddress(0, 0, 0, 0);
}

bool setActiveMode(WifiDriver &driver,
                   WifiDriverMode mode,
                   const DeviceConfig &config) {
  return driver.setMode(mode) && driver.setTxPower(config.wifiTxDbm);
}

bool apRuntimeSettingsEqual(const DeviceConfig &left, const DeviceConfig &right) {
  return strcmp(left.apSsid, right.apSsid) == 0 &&
         left.apPasswordEnabled == right.apPasswordEnabled &&
         strcmp(left.adminPassword, right.adminPassword) == 0 &&
         left.apIpMode == right.apIpMode &&
         strcmp(left.apIpAddress, right.apIpAddress) == 0 &&
         strcmp(left.apIpNetmask, right.apIpNetmask) == 0;
}

bool transitionStateActive(WifiTestState state, bool persisted) {
  switch (state) {
    case WifiTestState::Queued:
    case WifiTestState::Testing:
    case WifiTestState::Committing:
    case WifiTestState::Finalizing:
    case WifiTestState::RollbackPending:
    case WifiTestState::RollingBack:
    case WifiTestState::Restoring:
      return true;
    case WifiTestState::Succeeded:
      return !persisted;
    default:
      return false;
  }
}
}  // namespace

const char *wifiLinkStateToString(WifiLinkState state) {
  switch (state) {
    case WifiLinkState::Disabled: return "disabled";
    case WifiLinkState::Connecting: return "connecting";
    case WifiLinkState::Connected: return "connected";
    case WifiLinkState::Failed: return "failed";
  }
  return "unknown";
}

const char *wifiApStateToString(WifiApState state) {
  switch (state) {
    case WifiApState::Disabled: return "disabled";
    case WifiApState::Starting: return "starting";
    case WifiApState::Active: return "active";
    case WifiApState::Failed: return "failed";
  }
  return "unknown";
}

const char *wifiTestStateToString(WifiTestState state) {
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

const char *wifiTestFailureToString(WifiTestFailure failure) {
  switch (failure) {
    case WifiTestFailure::None: return "none";
    case WifiTestFailure::ConnectTimeout: return "connect_timeout";
    case WifiTestFailure::StationDisconnected: return "station_disconnected";
    case WifiTestFailure::SubnetOverlap: return "subnet_overlap";
    case WifiTestFailure::IpConfigurationFailed: return "ip_configuration_failed";
    case WifiTestFailure::RadioUnavailable: return "radio_unavailable";
    case WifiTestFailure::ManagementApUnavailable: return "management_ap_unavailable";
    case WifiTestFailure::ApConfigurationFailed: return "ap_configuration_failed";
    case WifiTestFailure::RuntimeApplyFailed: return "runtime_apply_failed";
    case WifiTestFailure::StorageError: return "storage_error";
  }
  return "unknown";
}

Result WifiManager::begin(WifiRadio *radio,
                          WifiDriver *driver,
                          MonotonicClock *clock) {
  if (radio == nullptr || driver == nullptr || clock == nullptr) {
    return invalidInput("missing Wi-Fi runtime dependency");
  }
  radio_ = radio;
  driver_ = driver;
  clock_ = clock;
  if (testMutex_ == nullptr) testMutex_ = xSemaphoreCreateMutex();
  if (testMutex_ == nullptr) return outOfSpace("failed to create Wi-Fi connection mutex");
  return okResult();
}

Result WifiManager::apply(const DeviceConfig &config, WifiStatus &status) {
  WifiRadioGuard radioGuard(radio_, portMAX_DELAY);
  if (!radioGuard.locked()) return networkError("Wi-Fi radio unavailable");

  if (lockTest()) {
    if (testState_ == WifiTestState::Succeeded && testCommitCompleted_) {
      resetTestLocked(WifiTestState::Idle);
    } else if (transitionStateActive(testState_, testCommitCompleted_)) {
      resetTestLocked(WifiTestState::Expired);
    }
    unlockTest();
  }

  WifiStatus next{config.wifiMode, false, false, WifiLinkState::Disabled,
                  WifiApState::Disabled, IPAddress(), IPAddress()};

  driver_->setPersistent(false);
  driver_->setMode(WifiDriverMode::Off);

  if (config.wifiMode == WifiMode::Off) {
    setStatus(next);
    status = next;
    return okResult();
  }

  if (config.hostname[0] != '\0') {
    driver_->setHostname(config.hostname);
  }

  if (config.wifiMode == WifiMode::Sta || config.wifiMode == WifiMode::ApSta) {
    next.staEnabled = true;
    const WifiDriverMode mode =
        config.wifiMode == WifiMode::ApSta
            ? WifiDriverMode::ApSta
            : WifiDriverMode::Sta;
    if (!setActiveMode(*driver_, mode, config)) {
      next.staEnabled = false;
      next.staState = WifiLinkState::Failed;
      setStatus(next);
      status = next;
      return networkError("failed to set Wi-Fi mode");
    }

    const Result staIpResult = configureStationIp(*driver_, config);
    if (!staIpResult.ok()) {
      next.staState = WifiLinkState::Failed;
      setStatus(next);
      status = next;
      return staIpResult;
    }

    if (config.wifiMode == WifiMode::ApSta) {
      const Result apResult = startSoftAp(*driver_, config, next);
      if (!apResult.ok()) {
        next.staState = WifiLinkState::Failed;
        setStatus(next);
        status = next;
        return apResult;
      }
    }

    next.staState = WifiLinkState::Connecting;
    driver_->beginStation(
        config.staSsid,
        config.staSecurity == StaSecurity::Open ? nullptr : config.staPassword);
    connectStartedMs_ = clock_->nowMs();
    setStatus(next);
    status = next;
    return okResult();
  }

  if (config.wifiMode == WifiMode::Ap) {
    if (!setActiveMode(*driver_, WifiDriverMode::Ap, config)) {
      next.apState = WifiApState::Failed;
      setStatus(next);
      status = next;
      return networkError("failed to set AP mode");
    }
    const Result apResult = startSoftAp(*driver_, config, next);
    setStatus(next);
    status = next;
    return apResult;
  }

  setStatus(next);
  status = next;
  return invalidInput("unsupported Wi-Fi mode");
}

Result WifiManager::applyTxPower(const DeviceConfig &config) {
  WifiRadioGuard radioGuard(radio_, portMAX_DELAY);
  if (!radioGuard.locked()) return networkError("Wi-Fi radio unavailable");
  if (status().mode == WifiMode::Off) return okResult();
  return driver_->setTxPower(config.wifiTxDbm)
             ? okResult()
             : networkError("failed to apply Wi-Fi TX power");
}

bool WifiManager::poll(const DeviceConfig &config) {
  WifiRadioGuard radioGuard(radio_, 0);
  if (!radioGuard.locked()) return false;

  WifiStatus current = status();
  bool testChanged = false;
  if (pollStaTest(current, testChanged)) {
    return testChanged;
  }

  const bool shouldUseSta = config.wifiMode == WifiMode::Sta || config.wifiMode == WifiMode::ApSta;
  if (!shouldUseSta || !current.staEnabled) {
    return false;
  }

  if (stationHasIpv4(*driver_)) {
    bool changed = current.staState != WifiLinkState::Connected ||
                   current.staIp != driver_->stationIp();
    current.staState = WifiLinkState::Connected;
    current.staIp = driver_->stationIp();

    if (current.apEnabled && runtimeSubnetsOverlap(*driver_, config)) {
      driver_->disconnectStation(false, false);
      setActiveMode(*driver_, WifiDriverMode::Ap, config);
      current.mode = WifiMode::Ap;
      current.staEnabled = false;
      current.staState = WifiLinkState::Failed;
      current.staIp = IPAddress();
      current.apIp = driver_->apIp();
      setStatus(current);
      return true;
    }

    if (config.wifiMode == WifiMode::Sta && current.apEnabled) {
      driver_->stopAp(false);
      setActiveMode(*driver_, WifiDriverMode::Sta, config);
      current.apEnabled = false;
      current.apState = WifiApState::Disabled;
      current.apIp = IPAddress();
      current.mode = WifiMode::Sta;
      changed = true;
    } else if (config.wifiMode == WifiMode::ApSta) {
      current.mode = WifiMode::ApSta;
    }
    setStatus(current);
    return changed;
  }

  if (current.staState == WifiLinkState::Connecting) {
    if (clock_->nowMs() - connectStartedMs_ < kStaConnectTimeoutMs) {
      return false;
    }
    // The application timeout must also stop the driver's connection attempt.
    // ESP-IDF rejects esp_wifi_scan_start() with ESP_ERR_WIFI_STATE while STA
    // is still connecting, even if our public status has already moved on.
    driver_->disconnectStationAsync(false, false);
    current.staState = WifiLinkState::Failed;
    current.staIp = IPAddress();
    if (!current.apEnabled && config.fallbackToAp && config.apSsid[0] != '\0') {
      if (setActiveMode(*driver_, WifiDriverMode::ApSta, config) &&
          startSoftAp(*driver_, config, current).ok()) {
        current.mode = WifiMode::ApSta;
      } else {
        current.apState = WifiApState::Failed;
      }
    }
    setStatus(current);
    return true;
  }

  const bool changed = current.staState == WifiLinkState::Connected || current.staIp != IPAddress();
  current.staState = WifiLinkState::Failed;
  current.staIp = IPAddress();
  if (config.fallbackToAp && !current.apEnabled && config.apSsid[0] != '\0') {
    if (setActiveMode(*driver_, WifiDriverMode::ApSta, config) &&
        startSoftAp(*driver_, config, current).ok()) {
      current.mode = WifiMode::ApSta;
    } else {
      current.apState = WifiApState::Failed;
    }
    setStatus(current);
    return true;
  }
  setStatus(current);
  return changed;
}

Result WifiManager::queueStaTest(const DeviceConfig &candidate,
                                 const DeviceConfig &previous,
                                 uint32_t &testId) {
  if (candidate.wifiMode != WifiMode::Sta && candidate.wifiMode != WifiMode::ApSta) {
    return invalidInput("wifi connection requires STA or AP + STA mode");
  }
  const WifiStatus current = status();
  if (!current.apEnabled || current.apState != WifiApState::Active ||
      current.apIp == IPAddress(0, 0, 0, 0)) {
    return networkError("wifi connection requires an active management AP");
  }
  if (!lockTest()) return networkError("wifi connection unavailable");
  if (transitionStateActive(testState_, testCommitCompleted_)) {
    unlockTest();
    return unsupported("wifi connection already in progress");
  }

  clearTestConfigsLocked();
  testCandidate_ = candidate;
  testPrevious_ = previous;
  testId_ = nextTestId_++;
  if (testId_ == 0) testId_ = nextTestId_++;
  testState_ = WifiTestState::Queued;
  testFailure_ = WifiTestFailure::None;
  testConnectStartedMs_ = 0;
  testCommitDeadlineMs_ = 0;
  testFinalizeDeadlineMs_ = 0;
  testCommitCompleted_ = false;
  testStaIp_ = IPAddress();
  testId = testId_;
  unlockTest();
  return okResult();
}

WifiTestStatus WifiManager::testStatus() const {
  WifiTestStatus snapshot;
  if (!lockTest()) return snapshot;
  snapshot.testId = testId_;
  snapshot.state = testState_;
  snapshot.failure = testFailure_;
  snapshot.staIp = testStaIp_;
  snapshot.persisted = testCommitCompleted_;
  const uint32_t now = clock_ == nullptr ? 0 : clock_->nowMs();
  if (testState_ == WifiTestState::Succeeded &&
      !testCommitCompleted_ &&
      !deadlineReached(now, testCommitDeadlineMs_)) {
    const uint32_t remainingMs = testCommitDeadlineMs_ - now;
    snapshot.commitExpiresInSeconds = (remainingMs + 999U) / 1000U;
  }
  if (testState_ == WifiTestState::Finalizing && testCommitCompleted_ &&
      testCandidate_.wifiMode == WifiMode::Sta &&
      !deadlineReached(now, testFinalizeDeadlineMs_)) {
    const uint32_t remainingMs = testFinalizeDeadlineMs_ - now;
    snapshot.apShutdownInSeconds = (remainingMs + 999U) / 1000U;
  }
  unlockTest();
  return snapshot;
}

bool WifiManager::testBlocksScan() const {
  if (!lockTest()) return true;
  const bool blocked = transitionStateActive(testState_, testCommitCompleted_);
  unlockTest();
  return blocked;
}

bool WifiManager::staConnectionBlocksScan() const {
  return status().staState == WifiLinkState::Connecting;
}

Result WifiManager::prepareTestCommit(uint32_t testId, DeviceConfig &candidate) {
  if (!lockTest()) return networkError("wifi test unavailable");
  if (testId == 0 || testId != testId_) {
    unlockTest();
    return notFound("wifi test id is stale");
  }
  if (testState_ != WifiTestState::Succeeded ||
      deadlineReached(clock_->nowMs(), testCommitDeadlineMs_)) {
    unlockTest();
    return unsupported("wifi test is not ready to commit");
  }
  candidate = testCandidate_;
  testState_ = WifiTestState::Committing;
  unlockTest();
  return okResult();
}

void WifiManager::finishTestCommit(uint32_t testId, bool committed) {
  if (!lockTest()) return;
  if (testId_ == testId && testState_ == WifiTestState::Committing) {
    if (committed) {
      testState_ = WifiTestState::Finalizing;
      testCommitCompleted_ = true;
      testCommitDeadlineMs_ = 0;
      testFinalizeDeadlineMs_ = testCandidate_.wifiMode == WifiMode::Sta
                                    ? clock_->nowMs() + kApShutdownGraceMs
                                    : clock_->nowMs();
    } else {
      testState_ = WifiTestState::Restoring;
      testFailure_ = WifiTestFailure::StorageError;
      testCommitCompleted_ = false;
      testCommitDeadlineMs_ = 0;
      testFinalizeDeadlineMs_ = 0;
      testStaIp_ = IPAddress();
    }
  }
  unlockTest();
}

Result WifiManager::prepareTestRollback(uint32_t testId, DeviceConfig &previous) {
  if (!lockTest()) return networkError("wifi rollback unavailable");
  if (testId == 0 || testId != testId_) {
    unlockTest();
    return notFound("wifi test id is stale");
  }
  if (testState_ != WifiTestState::RollbackPending || !testCommitCompleted_) {
    unlockTest();
    return unsupported("wifi rollback is not ready");
  }
  previous = testPrevious_;
  testState_ = WifiTestState::RollingBack;
  unlockTest();
  return okResult();
}

void WifiManager::finishTestRollback(uint32_t testId, bool restored) {
  if (!lockTest()) return;
  if (testId_ == testId && testState_ == WifiTestState::RollingBack) {
    testState_ = WifiTestState::Restoring;
    testCommitCompleted_ = false;
    if (!restored) testFailure_ = WifiTestFailure::StorageError;
  }
  unlockTest();
}

bool WifiManager::pollStaTest(WifiStatus &current, bool &changed) {
  TestPollSnapshot snapshot;
  if (!readTestPollSnapshot(snapshot)) return false;

  switch (snapshot.state) {
    case WifiTestState::Idle:
    case WifiTestState::Failed:
    case WifiTestState::Expired:
      return false;
    case WifiTestState::Committing:
    case WifiTestState::RollbackPending:
    case WifiTestState::RollingBack:
      return true;
    case WifiTestState::Queued:
      return pollQueuedStaTest(snapshot, current, changed);
    case WifiTestState::Testing:
      return pollTestingStaTest(snapshot, current, changed);
    case WifiTestState::Succeeded:
      return pollSucceededStaTest(snapshot, current, changed);
    case WifiTestState::Finalizing:
      return pollFinalizingStaTest(snapshot, current, changed);
    case WifiTestState::Restoring:
      return pollRestoringStaTest(snapshot, current, changed);
  }
  return true;
}

bool WifiManager::readTestPollSnapshot(TestPollSnapshot &snapshot) const {
  if (!lockTest()) return false;
  snapshot.state = testState_;
  snapshot.testId = testId_;
  snapshot.startedMs = testConnectStartedMs_;
  snapshot.commitDeadlineMs = testCommitDeadlineMs_;
  snapshot.finalizeDeadlineMs = testFinalizeDeadlineMs_;
  snapshot.committed = testCommitCompleted_;
  if (transitionStateActive(snapshot.state, testCommitCompleted_) ||
      snapshot.state == WifiTestState::Succeeded) {
    snapshot.candidate = testCandidate_;
    snapshot.previous = testPrevious_;
  }
  unlockTest();
  return true;
}

bool WifiManager::pollQueuedStaTest(const TestPollSnapshot &snapshot,
                                    WifiStatus &current,
                                    bool &changed) {
  if (!current.apEnabled || current.apState != WifiApState::Active ||
      current.apIp == IPAddress(0, 0, 0, 0)) {
    changed = failTest(
        snapshot.testId,
        snapshot.state,
        WifiTestFailure::ManagementApUnavailable,
        current);
    return true;
  }

  // Clear the driver's previous STA credential/PMK state so testing a new
  // password for the currently connected SSID cannot produce a false pass.
  // WiFi persistence is disabled, so this does not touch product config NVS.
  driver_->disconnectStation(false, true);
  if (!setActiveMode(*driver_, WifiDriverMode::ApSta, snapshot.candidate)) {
    changed = failTest(
        snapshot.testId,
        snapshot.state,
        WifiTestFailure::RadioUnavailable,
        current);
    return true;
  }
  const Result ipResult = configureStationIp(*driver_, snapshot.candidate);
  if (!ipResult.ok()) {
    changed = failTest(
        snapshot.testId,
        snapshot.state,
        WifiTestFailure::IpConfigurationFailed,
        current);
    return true;
  }
  driver_->beginStation(
      snapshot.candidate.staSsid,
      snapshot.candidate.staSecurity == StaSecurity::Open
          ? nullptr
          : snapshot.candidate.staPassword);

  const uint32_t now = clock_->nowMs();
  if (lockTest()) {
    if (testId_ == snapshot.testId &&
        testState_ == WifiTestState::Queued) {
      testState_ = WifiTestState::Testing;
      testConnectStartedMs_ = now;
    }
    unlockTest();
  }
  current.mode = WifiMode::ApSta;
  current.staEnabled = true;
  current.staState = WifiLinkState::Connecting;
  current.staIp = IPAddress();
  setStatus(current);
  changed = true;
  return true;
}

bool WifiManager::pollTestingStaTest(const TestPollSnapshot &snapshot,
                                     WifiStatus &current,
                                     bool &changed) {
  if (stationHasIpv4(*driver_)) {
    const bool finalApMayRun =
        snapshot.candidate.wifiMode == WifiMode::ApSta ||
        (snapshot.candidate.wifiMode == WifiMode::Sta &&
         snapshot.candidate.fallbackToAp);
    if (runtimeSubnetsOverlap(*driver_, snapshot.previous) ||
        (finalApMayRun &&
         runtimeSubnetsOverlap(*driver_, snapshot.candidate))) {
      changed = failTest(
          snapshot.testId,
          snapshot.state,
          WifiTestFailure::SubnetOverlap,
          current);
      return true;
    }
    const IPAddress staIp = driver_->stationIp();
    if (lockTest()) {
      if (testId_ == snapshot.testId &&
          testState_ == WifiTestState::Testing) {
        testState_ = WifiTestState::Succeeded;
        testFailure_ = WifiTestFailure::None;
        testStaIp_ = staIp;
        testCommitDeadlineMs_ = clock_->nowMs() + kStaTestCommitWindowMs;
      }
      unlockTest();
    }
    current.mode = WifiMode::ApSta;
    current.staEnabled = true;
    current.staState = WifiLinkState::Connected;
    current.staIp = staIp;
    setStatus(current);
    changed = true;
    return true;
  }
  if (clock_->nowMs() - snapshot.startedMs >= kStaConnectTimeoutMs) {
    // Stop the driver's attempt as well as the application state. Otherwise
    // ESP-IDF may keep rejecting scan requests after the public timeout.
    driver_->disconnectStationAsync(false, false);
    changed = failTest(
        snapshot.testId,
        snapshot.state,
        WifiTestFailure::ConnectTimeout,
        current);
  }
  return true;
}

bool WifiManager::pollSucceededStaTest(const TestPollSnapshot &snapshot,
                                       WifiStatus &current,
                                       bool &changed) {
  // Persisted success is a terminal public result. Let normal Wi-Fi polling
  // continue without re-entering the pre-commit expiry path.
  if (snapshot.committed) return false;

  WifiTestFailure failure = WifiTestFailure::None;
  WifiTestState terminalState = WifiTestState::Failed;
  if (deadlineReached(clock_->nowMs(), snapshot.commitDeadlineMs)) {
    terminalState = WifiTestState::Expired;
  } else if (!stationHasIpv4(*driver_)) {
    failure = WifiTestFailure::StationDisconnected;
  } else {
    return true;
  }

  bool shouldRestore = false;
  if (lockTest()) {
    if (testId_ == snapshot.testId &&
        testState_ == WifiTestState::Succeeded) {
      testState_ = terminalState;
      testFailure_ = failure;
      testStaIp_ = IPAddress();
      shouldRestore = true;
    }
    unlockTest();
  }
  if (shouldRestore) {
    const Result restoreResult =
        restorePersistedAfterTest(snapshot.previous, current);
    if (lockTest()) {
      if (!restoreResult.ok()) {
        testFailure_ = WifiTestFailure::RuntimeApplyFailed;
      }
      clearTestConfigsLocked();
      unlockTest();
    }
    setStatus(current);
    changed = true;
  }
  return true;
}

bool WifiManager::pollFinalizingStaTest(const TestPollSnapshot &snapshot,
                                        WifiStatus &current,
                                        bool &changed) {
  if (!stationHasIpv4(*driver_)) {
    beginRollback(
        snapshot.testId,
        snapshot.state,
        WifiTestFailure::StationDisconnected);
    return true;
  }

  if (snapshot.candidate.wifiMode == WifiMode::Sta) {
    if (!deadlineReached(clock_->nowMs(), snapshot.finalizeDeadlineMs)) {
      return true;
    }

    // Keep the radio and verified STA link alive while removing only the AP
    // interface. Passing true here would end the whole Wi-Fi driver.
    const bool apStopped = driver_->stopAp(false);
    const bool staModeSet = setActiveMode(
        *driver_, WifiDriverMode::Sta, snapshot.candidate);
    if (apStopped) {
      current.apEnabled = false;
      current.apState = WifiApState::Disabled;
      current.apIp = IPAddress();
    }
    if (!apStopped || !staModeSet || !stationHasIpv4(*driver_)) {
      setStatus(current);
      beginRollback(
          snapshot.testId,
          snapshot.state,
          WifiTestFailure::RuntimeApplyFailed);
      changed = true;
      return true;
    }
    current.mode = WifiMode::Sta;
    current.staEnabled = true;
    current.staState = WifiLinkState::Connected;
    current.staIp = driver_->stationIp();
    setStatus(current);
    finishTestSuccess(snapshot.testId);
    changed = true;
    return true;
  }

  if (snapshot.candidate.wifiMode == WifiMode::ApSta) {
    if (!apRuntimeSettingsEqual(
            snapshot.previous, snapshot.candidate)) {
      const bool apStopped = driver_->stopAp(false);
      current.apEnabled = false;
      current.apState = WifiApState::Disabled;
      current.apIp = IPAddress();
      if (!apStopped ||
          !setActiveMode(*driver_, WifiDriverMode::ApSta, snapshot.candidate) ||
          !startSoftAp(*driver_, snapshot.candidate, current).ok() ||
          !stationHasIpv4(*driver_)) {
        setStatus(current);
        beginRollback(
            snapshot.testId,
            snapshot.state,
            WifiTestFailure::ApConfigurationFailed);
        changed = true;
        return true;
      }
    }
    current.mode = WifiMode::ApSta;
    current.staEnabled = true;
    current.staState = WifiLinkState::Connected;
    current.staIp = driver_->stationIp();
    setStatus(current);
    finishTestSuccess(snapshot.testId);
    changed = true;
    return true;
  }

  beginRollback(
      snapshot.testId,
      snapshot.state,
      WifiTestFailure::RuntimeApplyFailed);
  return true;
}

bool WifiManager::pollRestoringStaTest(const TestPollSnapshot &snapshot,
                                       WifiStatus &current,
                                       bool &changed) {
  const Result restoreResult =
      restorePersistedAfterTest(snapshot.previous, current);
  setStatus(current);
  if (lockTest()) {
    if (testId_ == snapshot.testId &&
        testState_ == WifiTestState::Restoring) {
      if (!restoreResult.ok()) {
        testFailure_ = WifiTestFailure::RuntimeApplyFailed;
      }
      testState_ = WifiTestState::Failed;
      testCommitCompleted_ = false;
      testStaIp_ = IPAddress();
      clearTestConfigsLocked();
    }
    unlockTest();
  }
  changed = true;
  return true;
}

Result WifiManager::restorePersistedAfterTest(const DeviceConfig &persisted,
                                              WifiStatus &current) {
  driver_->disconnectStation(false, true);
  current.staIp = IPAddress();

  if (persisted.wifiMode == WifiMode::Ap) {
    if (!setActiveMode(*driver_, WifiDriverMode::Ap, persisted)) {
      return networkError("failed to restore AP mode");
    }
    current.mode = WifiMode::Ap;
    current.staEnabled = false;
    current.staState = WifiLinkState::Disabled;
    current.apIp = driver_->apIp();
    current.apEnabled = current.apIp != IPAddress(0, 0, 0, 0);
    current.apState = current.apEnabled ? WifiApState::Active : WifiApState::Failed;
    if (!current.apEnabled) {
      const Result apResult = startSoftAp(*driver_, persisted, current);
      if (!apResult.ok()) return apResult;
    }
    return current.apEnabled ? okResult() : networkError("management AP was lost");
  }

  if (persisted.wifiMode == WifiMode::Sta || persisted.wifiMode == WifiMode::ApSta) {
    const bool retainAp = current.apEnabled || persisted.wifiMode == WifiMode::ApSta;
    if (!setActiveMode(*driver_,
                       retainAp ? WifiDriverMode::ApSta : WifiDriverMode::Sta,
                       persisted)) {
      current.staEnabled = false;
      current.staState = WifiLinkState::Failed;
      return networkError("failed to restore STA mode");
    }
    current.apIp = driver_->apIp();
    current.apEnabled = current.apIp != IPAddress(0, 0, 0, 0);
    current.apState = current.apEnabled ? WifiApState::Active : WifiApState::Disabled;
    if (persisted.wifiMode == WifiMode::ApSta && !current.apEnabled) {
      const Result apResult = startSoftAp(*driver_, persisted, current);
      if (!apResult.ok()) return apResult;
    }
    const Result ipResult = configureStationIp(*driver_, persisted);
    if (!ipResult.ok()) {
      current.staState = WifiLinkState::Failed;
      return ipResult;
    }
    driver_->beginStation(
        persisted.staSsid,
        persisted.staSecurity == StaSecurity::Open
            ? nullptr
            : persisted.staPassword);
    connectStartedMs_ = clock_->nowMs();
    current.mode = current.apEnabled ? WifiMode::ApSta : WifiMode::Sta;
    current.staEnabled = true;
    current.staState = WifiLinkState::Connecting;
    return okResult();
  }

  driver_->setMode(WifiDriverMode::Off);
  current = {WifiMode::Off, false, false, WifiLinkState::Disabled,
             WifiApState::Disabled, IPAddress(), IPAddress()};
  return okResult();
}

bool WifiManager::failTest(uint32_t testId,
                           WifiTestState expectedState,
                           WifiTestFailure failure,
                           WifiStatus &current) {
  DeviceConfig previous{};
  bool shouldRestore = false;
  if (lockTest()) {
    if (testId_ == testId && testState_ == expectedState) {
      previous = testPrevious_;
      testState_ = WifiTestState::Failed;
      testFailure_ = failure;
      testStaIp_ = IPAddress();
      shouldRestore = true;
    }
    unlockTest();
  }
  if (!shouldRestore) return false;
  const Result restoreResult = restorePersistedAfterTest(previous, current);
  if (lockTest()) {
    if (!restoreResult.ok()) testFailure_ = WifiTestFailure::RuntimeApplyFailed;
    clearTestConfigsLocked();
    unlockTest();
  }
  setStatus(current);
  return true;
}

bool WifiManager::beginRollback(uint32_t testId,
                                WifiTestState expectedState,
                                WifiTestFailure failure) {
  if (!lockTest()) return false;
  const bool accepted = testId_ == testId && testState_ == expectedState &&
                        testCommitCompleted_;
  if (accepted) {
    testState_ = WifiTestState::RollbackPending;
    testFailure_ = failure;
    testFinalizeDeadlineMs_ = 0;
  }
  unlockTest();
  return accepted;
}

void WifiManager::finishTestSuccess(uint32_t testId) {
  if (!lockTest()) return;
  if (testId_ == testId && testState_ == WifiTestState::Finalizing) {
    testState_ = WifiTestState::Succeeded;
    testFailure_ = WifiTestFailure::None;
    testCommitCompleted_ = true;
    testCommitDeadlineMs_ = 0;
    testFinalizeDeadlineMs_ = 0;
    clearTestConfigsLocked();
  }
  unlockTest();
}

void WifiManager::resetTestLocked(WifiTestState state) {
  clearTestConfigsLocked();
  testState_ = state;
  testFailure_ = WifiTestFailure::None;
  testConnectStartedMs_ = 0;
  testCommitDeadlineMs_ = 0;
  testFinalizeDeadlineMs_ = 0;
  testCommitCompleted_ = false;
  testStaIp_ = IPAddress();
  if (state == WifiTestState::Idle) testId_ = 0;
}

bool WifiManager::lockTest() const {
  return testMutex_ != nullptr && xSemaphoreTake(testMutex_, portMAX_DELAY) == pdTRUE;
}

void WifiManager::unlockTest() const {
  xSemaphoreGive(testMutex_);
}

void WifiManager::clearTestConfigsLocked() {
  volatile uint8_t *candidate = reinterpret_cast<volatile uint8_t *>(&testCandidate_);
  for (size_t index = 0; index < sizeof(testCandidate_); ++index) {
    candidate[index] = 0;
  }
  volatile uint8_t *previous = reinterpret_cast<volatile uint8_t *>(&testPrevious_);
  for (size_t index = 0; index < sizeof(testPrevious_); ++index) {
    previous[index] = 0;
  }
}

WifiStatus WifiManager::status() const {
  portENTER_CRITICAL(&statusMux_);
  const WifiStatus copy = status_;
  portEXIT_CRITICAL(&statusMux_);
  return copy;
}

void WifiManager::setStatus(const WifiStatus &status) {
  portENTER_CRITICAL(&statusMux_);
  status_ = status;
  portEXIT_CRITICAL(&statusMux_);
}
