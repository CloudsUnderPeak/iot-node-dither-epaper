#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>

#include "core/Result.h"
#include "modules/captive/CaptivePortalDnsService.h"
#include "modules/config/ConfigService.h"
#include "modules/mdns/MdnsService.h"
#include "modules/runtime/SystemRestartCoordinator.h"
#include "modules/wifi/WifiManager.h"
#include "modules/sleep/SleepFeatures.h"
#if IOT_FEATURE_SLEEP
class SleepCoordinator;
#endif

struct RuntimeActionSnapshot {
  bool restartPending = false;
  bool restartFailed = false;
  bool runtimeActionPending = false;
};

class RuntimeActionScheduler {
 public:
  Result begin(ConfigService *configService,
               WifiManager *wifiManager,
               MdnsService *mdnsService,
               CaptivePortalDnsService *captivePortalDnsService,
               SystemRestartCoordinator *restartCoordinator);
  void poll();
  bool ready() const;
  void scheduleWifiApply(uint32_t delayMs);
  void scheduleWifiTxPowerApply(uint32_t delayMs);
  void scheduleSystemReset(uint32_t delayMs);
  RuntimeActionSnapshot snapshot();
#if IOT_FEATURE_SLEEP
  void setSleepCoordinator(SleepCoordinator *sleep) { sleepCoordinator_ = sleep; }
#endif

 private:
  ConfigService *configService_ = nullptr;
  WifiManager *wifiManager_ = nullptr;
  MdnsService *mdnsService_ = nullptr;
  CaptivePortalDnsService *captivePortalDnsService_ = nullptr;
  SystemRestartCoordinator *restartCoordinator_ = nullptr;
  portMUX_TYPE pendingMux_ = portMUX_INITIALIZER_UNLOCKED;
  bool ready_ = false;
#if IOT_FEATURE_SLEEP
  SleepCoordinator *sleepCoordinator_ = nullptr;
#endif
  bool wifiApplyPending_ = false;
  uint32_t wifiApplyDueMs_ = 0;
  bool wifiTxPowerApplyPending_ = false;
  uint32_t wifiTxPowerApplyDueMs_ = 0;
  uint8_t wifiTxPowerRetryCount_ = 0;
  bool systemResetPending_ = false;
  bool systemResetDraining_ = false;
  bool systemResetFailed_ = false;
  uint32_t systemResetDueMs_ = 0;

  void applyPendingWifi();
  void applyPendingWifiTxPower();
  void commitVerifiedWifiConnection();
  void rollbackFailedWifiTransition();
  void applyPendingSystemReset();
};
