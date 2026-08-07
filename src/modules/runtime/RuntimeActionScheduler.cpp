#include "RuntimeActionScheduler.h"

Result RuntimeActionScheduler::begin(ConfigService *configService,
                                     WifiManager *wifiManager,
                                     MdnsService *mdnsService,
                                     CaptivePortalDnsService *captivePortalDnsService) {
  ready_ = false;
  if (configService == nullptr || wifiManager == nullptr || mdnsService == nullptr || captivePortalDnsService == nullptr) {
    return invalidInput("missing runtime action scheduler dependencies");
  }

  configService_ = configService;
  wifiManager_ = wifiManager;
  mdnsService_ = mdnsService;
  captivePortalDnsService_ = captivePortalDnsService;
  ready_ = true;
  return okResult();
}

void RuntimeActionScheduler::poll() {
  // A persisted reset intent has priority over every network-side action.
  // This keeps a pending factory/settings/data reset from being delayed by a
  // Wi-Fi transition or another coalesced apply.
  applyPendingSystemReset();
  rollbackFailedWifiTransition();
  commitVerifiedWifiConnection();
  applyPendingWifi();
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

void RuntimeActionScheduler::scheduleSystemReset(uint32_t delayMs) {
  const uint32_t dueMs = millis() + delayMs;
  portENTER_CRITICAL(&pendingMux_);
  if (!systemResetPending_ || static_cast<int32_t>(dueMs - systemResetDueMs_) < 0) {
    systemResetDueMs_ = dueMs;
  }
  systemResetPending_ = true;
  portEXIT_CRITICAL(&pendingMux_);
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
  if (wifiManager_->testBlocksScan()) {
    scheduleWifiApply(100);
    return;
  }

  const DeviceConfig config = configService_->snapshot();
  WifiStatus status{};
  const Result applyResult = wifiManager_->apply(config, status);
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
  bool reset = false;
  const uint32_t now = millis();
  portENTER_CRITICAL(&pendingMux_);
  if (systemResetPending_ && static_cast<int32_t>(now - systemResetDueMs_) >= 0) {
    systemResetPending_ = false;
    reset = true;
  }
  portEXIT_CRITICAL(&pendingMux_);
  if (!reset) {
    return;
  }

  Serial.println("api system: scheduled restart");
  ESP.restart();
}
