#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "model/DeviceConfig.h"
#include "storage/ConfigStore.h"

enum class ConfigStartupState : uint8_t {
  Uninitialized,
  Persisted,
  FactoryDefaultsCreated,
  RecoveryDefaults,
};

const char *configStartupStateToString(ConfigStartupState state);
const char *configRecoveryReasonToString(ResultCode reason);

struct SystemConfigUpdate {
  bool hostnameProvided = false;
  const char *hostname = nullptr;
  bool wifiTxDbmProvided = false;
  uint8_t wifiTxDbm = kDefaultWifiTxDbm;
};

struct SystemConfigChanges {
  bool hostnameChanged = false;
  bool wifiTxDbmChanged = false;

  bool any() const { return hostnameChanged || wifiTxDbmChanged; }
};

// Owns the active configuration and serializes persistence across HTTP and
// loop tasks. Callers receive snapshots instead of a shared mutable pointer.
class ConfigService {
 public:
  explicit ConfigService(ConfigStore &store);

  Result begin();
  DeviceConfig snapshot() const;
  Result commit(const DeviceConfig &updated);
  Result updateWifi(const DeviceConfig &source, DeviceConfig *committed = nullptr);
  Result updateSystem(const SystemConfigUpdate &update,
                      DeviceConfig *committed = nullptr,
                      SystemConfigChanges *changes = nullptr);
  Result updateHostname(const char *hostname, DeviceConfig *committed = nullptr);
  Result updateAdminPassword(const char *password, DeviceConfig *committed = nullptr);
  bool ready() const;
  ConfigStartupState startupState() const;
  ResultCode recoveryReason() const;

 private:
  ConfigStore &store_;
  DeviceConfig active_{};
  mutable SemaphoreHandle_t mutex_ = nullptr;
  bool ready_ = false;
  ConfigStartupState startupState_ = ConfigStartupState::Uninitialized;
  ResultCode recoveryReason_ = ResultCode::Ok;

  bool lock() const;
  void unlock() const;
  Result saveLocked(const DeviceConfig &updated, DeviceConfig *committed);
};
