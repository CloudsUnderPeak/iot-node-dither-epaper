#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>

#include "core/Result.h"
#include "modules/captive/CaptivePortalDnsService.h"
#include "modules/config/ConfigService.h"
#include "modules/mdns/MdnsService.h"
#include "modules/runtime/SystemRestartCoordinator.h"
#include "modules/wifi/WifiManager.h"

struct RuntimeActionSnapshot {
  bool restartPending = false;
  bool restartFailed = false;
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
  void scheduleSystemReset(uint32_t delayMs);
  RuntimeActionSnapshot snapshot();

 private:
  ConfigService *configService_ = nullptr;
  WifiManager *wifiManager_ = nullptr;
  MdnsService *mdnsService_ = nullptr;
  CaptivePortalDnsService *captivePortalDnsService_ = nullptr;
  SystemRestartCoordinator *restartCoordinator_ = nullptr;
  portMUX_TYPE pendingMux_ = portMUX_INITIALIZER_UNLOCKED;
  bool ready_ = false;
  bool wifiApplyPending_ = false;
  uint32_t wifiApplyDueMs_ = 0;
  bool systemResetPending_ = false;
  bool systemResetFailed_ = false;
  uint32_t systemResetDueMs_ = 0;

  void applyPendingWifi();
  void commitVerifiedWifiConnection();
  void rollbackFailedWifiTransition();
  void applyPendingSystemReset();
};
