#include "RuntimeActionScheduler.h"

Result RuntimeActionScheduler::begin(ConfigService *configService,
                                     WifiManager *wifiManager,
                                     MdnsService *mdnsService,
                                     CaptivePortalDnsService *captivePortalDnsService,
                                     SystemRestartCoordinator *restartCoordinator) {
  ready_ = false;
  if (configService == nullptr || wifiManager == nullptr || mdnsService == nullptr ||
      captivePortalDnsService == nullptr || restartCoordinator == nullptr) {
    return invalidInput("missing runtime action scheduler dependencies");
  }

  configService_ = configService;
  wifiManager_ = wifiManager;
  mdnsService_ = mdnsService;
  captivePortalDnsService_ = captivePortalDnsService;
  restartCoordinator_ = restartCoordinator;
  ready_ = true;
  return okResult();
}

void RuntimeActionScheduler::poll() {
  // A persisted reset intent has priority over every network-side action.
  // This keeps a pending factory/settings/data reset from being delayed by a
  // Wi-Fi transition or another coalesced apply.
  applyPendingSystemReset();
  if (snapshot().restartPending) return;
  rollbackFailedWifiTransition();
  commitVerifiedWifiConnection();
  applyPendingWifi();
  applyPendingWifiTxPower();
}

bool RuntimeActionScheduler::ready() const {
  return ready_;
}

void RuntimeActionScheduler::scheduleWifiApply(uint32_t delayMs) {
  const uint32_t dueMs = millis() + delayMs;
  portENTER_CRITICAL(&pendingMux_);
  if (!wifiApplyPending_ || static_cast<int32_t>(dueMs - wifiApplyDueMs_) < 0) {
    wifiApplyDueMs_ = dueMs;
  }
  wifiApplyPending_ = true;
  portEXIT_CRITICAL(&pendingMux_);
}

void RuntimeActionScheduler::scheduleWifiTxPowerApply(uint32_t delayMs) {
  const uint32_t dueMs = millis() + delayMs;
  portENTER_CRITICAL(&pendingMux_);
  if (!wifiTxPowerApplyPending_ ||
      static_cast<int32_t>(dueMs - wifiTxPowerApplyDueMs_) < 0) {
    wifiTxPowerApplyDueMs_ = dueMs;
  }
  wifiTxPowerApplyPending_ = true;
  wifiTxPowerRetryCount_ = 0;
  portEXIT_CRITICAL(&pendingMux_);
}

void RuntimeActionScheduler::scheduleSystemReset(uint32_t delayMs) {
  const uint32_t dueMs = millis() + delayMs;
  portENTER_CRITICAL(&pendingMux_);
  if (!systemResetPending_ || static_cast<int32_t>(dueMs - systemResetDueMs_) < 0) {
    systemResetDueMs_ = dueMs;
  }
  systemResetPending_ = true;
  systemResetFailed_ = false;
  portEXIT_CRITICAL(&pendingMux_);
}

RuntimeActionSnapshot RuntimeActionScheduler::snapshot() {
  RuntimeActionSnapshot result;
  portENTER_CRITICAL(&pendingMux_);
  result.restartPending = systemResetPending_;
  result.restartFailed = systemResetFailed_;
  portEXIT_CRITICAL(&pendingMux_);
  return result;
}

void RuntimeActionScheduler::applyPendingWifi() {
  bool apply = false;
  const uint32_t now = millis();
  portENTER_CRITICAL(&pendingMux_);
  if (wifiApplyPending_ && static_cast<int32_t>(now - wifiApplyDueMs_) >= 0) {
    wifiApplyPending_ = false;
    apply = true;
  }
  portEXIT_CRITICAL(&pendingMux_);
  if (!apply) {
    return;
  }

  // A normal Wi-Fi apply starts by turning the radio off. Defer it while the
  // management-AP-preserving connection transaction owns the radio.
  if (wifiManager_->testBlocksScan() || wifiManager_->scanBlocksRadio()) {
    scheduleWifiApply(100);
    return;
  }

  const DeviceConfig config = configService_->snapshot();
  WifiStatus status{};
  const Result applyResult = wifiManager_->apply(config, status);
  if (applyResult.code == ResultCode::Unsupported) {
    scheduleWifiApply(100);
    return;
  }
  const Result mdnsResult = mdnsService_->restart(config, status);
  const Result captiveDnsResult = captivePortalDnsService_->restart(status);
  Serial.printf("api wifi: apply %s: %s (%u)\n",
                wifiModeToString(config.wifiMode),
                applyResult.message,
                static_cast<unsigned>(applyResult.code));
  Serial.printf("api mdns: restart: %s (%u), host=%s.local\n",
                mdnsResult.message,
                static_cast<unsigned>(mdnsResult.code),
                mdnsService_->running() ? mdnsService_->hostName() : "disabled");
  Serial.printf("api captive-dns: restart: %s (%u), ip=%s\n",
                captiveDnsResult.message,
                static_cast<unsigned>(captiveDnsResult.code),
                captivePortalDnsService_->running() ? captivePortalDnsService_->captiveIp().toString().c_str() : "disabled");
}

void RuntimeActionScheduler::applyPendingWifiTxPower() {
  bool apply = false;
  const uint32_t now = millis();
  portENTER_CRITICAL(&pendingMux_);
  if (wifiTxPowerApplyPending_ &&
      static_cast<int32_t>(now - wifiTxPowerApplyDueMs_) >= 0) {
    wifiTxPowerApplyPending_ = false;
    apply = true;
  }
  portEXIT_CRITICAL(&pendingMux_);
  if (!apply) return;

  if (wifiManager_->testBlocksScan() || wifiManager_->scanBlocksRadio()) {
    portENTER_CRITICAL(&pendingMux_);
    wifiTxPowerApplyPending_ = true;
    wifiTxPowerApplyDueMs_ = millis() + 100U;
    portEXIT_CRITICAL(&pendingMux_);
    return;
  }

  const DeviceConfig config = configService_->snapshot();
  const Result result = wifiManager_->applyTxPower(config);
  Serial.printf("api wifi tx power: configured_dbm=%u, apply=%s (%u)\n",
                static_cast<unsigned>(config.wifiTxDbm),
                result.message,
                static_cast<unsigned>(result.code));
  if (result.ok()) {
    portENTER_CRITICAL(&pendingMux_);
    wifiTxPowerRetryCount_ = 0;
    portEXIT_CRITICAL(&pendingMux_);
    return;
  }

  portENTER_CRITICAL(&pendingMux_);
  if (wifiTxPowerRetryCount_ < 2U) {
    ++wifiTxPowerRetryCount_;
    wifiTxPowerApplyPending_ = true;
    wifiTxPowerApplyDueMs_ = millis() + 1000U;
  } else {
    wifiTxPowerRetryCount_ = 0;
  }
  portEXIT_CRITICAL(&pendingMux_);
}

void RuntimeActionScheduler::commitVerifiedWifiConnection() {
  const WifiTestStatus status = wifiManager_->testStatus();
  if (status.state != WifiTestState::Succeeded || status.persisted) return;

  DeviceConfig candidate;
  const Result prepareResult = wifiManager_->prepareTestCommit(status.testId, candidate);
  if (!prepareResult.ok()) return;

  const Result saveResult = configService_->updateWifi(candidate);
  wifiManager_->finishTestCommit(status.testId, saveResult.ok());
  Serial.printf("wifi connect: credential verified, save: %s (%u)\n",
                saveResult.message,
                static_cast<unsigned>(saveResult.code));
}

void RuntimeActionScheduler::rollbackFailedWifiTransition() {
  const WifiTestStatus status = wifiManager_->testStatus();
  if (status.state != WifiTestState::RollbackPending || !status.persisted) return;

  DeviceConfig previous;
  const Result prepareResult = wifiManager_->prepareTestRollback(status.testId, previous);
  if (!prepareResult.ok()) return;

  const Result saveResult = configService_->updateWifi(previous);
  wifiManager_->finishTestRollback(status.testId, saveResult.ok());
  Serial.printf("wifi transition: rollback save: %s (%u)\n",
                saveResult.message,
                static_cast<unsigned>(saveResult.code));
}

void RuntimeActionScheduler::applyPendingSystemReset() {
  const uint32_t now = millis();
  bool start = false;
  bool pending = false;
  portENTER_CRITICAL(&pendingMux_);
  if (systemResetPending_ && static_cast<int32_t>(now - systemResetDueMs_) >= 0) {
    start = !systemResetDraining_;
    systemResetDraining_ = true;
    pending = true;
  }
  portEXIT_CRITICAL(&pendingMux_);
  if (!pending) return;

  const bool rejected = start &&
      restartCoordinator_->requestRestart(now) == RestartRequest::Rejected;
  restartCoordinator_->pollRestart(now, !wifiManager_->scanBlocksRadio());
  if (rejected || restartCoordinator_->restartProgress() == RestartProgress::Failed) {
    portENTER_CRITICAL(&pendingMux_);
    systemResetPending_ = false;
    systemResetDraining_ = false;
    systemResetFailed_ = true;
    portEXIT_CRITICAL(&pendingMux_);
    Serial.println("api system: restart cancelled by safety coordinator");
  }
}
