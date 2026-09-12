#pragma once

#include "core/Result.h"
#include "modules/config/model/DeviceConfig.h"

enum class ConfigStartupState : uint8_t {
  Uninitialized,
  Persisted,
  FactoryDefaultsCreated,
  RecoveryDefaults,
};

inline const char *configStartupStateToString(ConfigStartupState state) {
  return state == ConfigStartupState::Persisted ? "persisted" : "uninitialized";
}

inline const char *configRecoveryReasonToString(ResultCode reason) {
  return reason == ResultCode::Ok ? "none" : "storage_error";
}

struct SystemConfigUpdate {
  bool hostnameProvided = false;
  const char *hostname = nullptr;
  bool wifiTxDbmProvided = false;
  uint8_t wifiTxDbm = kDefaultWifiTxDbm;
};

struct SystemConfigChanges {
  bool hostnameChanged = false;
  bool wifiTxDbmChanged = false;
};

class ConfigService {
 public:
  DeviceConfig value = defaultDeviceConfig();
  Result commitResult = okResult();
  unsigned commitCount = 0;

  DeviceConfig snapshot() const { return value; }
  ConfigStartupState startupState() const { return ConfigStartupState::Persisted; }
  ResultCode recoveryReason() const { return ResultCode::Ok; }
  Result commit(const DeviceConfig &updated) {
    ++commitCount;
    if (commitResult.ok()) value = updated;
    return commitResult;
  }
  Result updateWifi(const DeviceConfig &source, DeviceConfig *committed = nullptr) {
    DeviceConfig updated = value;
    updated.wifiMode = source.wifiMode;
    strlcpy(updated.staSsid, source.staSsid, sizeof(updated.staSsid));
    strlcpy(updated.staPassword, source.staPassword, sizeof(updated.staPassword));
    updated.staSecurity = source.staSecurity;
    updated.staIpMode = source.staIpMode;
    strlcpy(updated.staIpAddress, source.staIpAddress, sizeof(updated.staIpAddress));
    strlcpy(updated.staIpGateway, source.staIpGateway, sizeof(updated.staIpGateway));
    strlcpy(updated.staIpNetmask, source.staIpNetmask, sizeof(updated.staIpNetmask));
    strlcpy(updated.staDns1, source.staDns1, sizeof(updated.staDns1));
    strlcpy(updated.staDns2, source.staDns2, sizeof(updated.staDns2));
    strlcpy(updated.apSsid, source.apSsid, sizeof(updated.apSsid));
    updated.apPasswordEnabled = source.apPasswordEnabled;
    updated.apIpMode = source.apIpMode;
    strlcpy(updated.apIpAddress, source.apIpAddress, sizeof(updated.apIpAddress));
    strlcpy(updated.apIpNetmask, source.apIpNetmask, sizeof(updated.apIpNetmask));
    updated.fallbackToAp = source.fallbackToAp;
    return commitUpdated(updated, committed);
  }
  Result updateHostname(const char *hostname, DeviceConfig *committed = nullptr) {
    DeviceConfig updated = value;
    strlcpy(updated.hostname, hostname, sizeof(updated.hostname));
    return commitUpdated(updated, committed);
  }
  Result updateSystem(const SystemConfigUpdate &update,
                      DeviceConfig *committed = nullptr,
                      SystemConfigChanges *changes = nullptr) {
    DeviceConfig updated = value;
    SystemConfigChanges actual;
    if (update.hostnameProvided && strcmp(updated.hostname, update.hostname) != 0) {
      strlcpy(updated.hostname, update.hostname, sizeof(updated.hostname));
      actual.hostnameChanged = true;
    }
    if (update.wifiTxDbmProvided && updated.wifiTxDbm != update.wifiTxDbm) {
      updated.wifiTxDbm = update.wifiTxDbm;
      actual.wifiTxDbmChanged = true;
    }
    Result result = okResult();
    if (actual.hostnameChanged || actual.wifiTxDbmChanged) {
      result = commitUpdated(updated, committed);
    } else if (committed != nullptr) {
      *committed = value;
    }
    if (result.ok() && changes != nullptr) *changes = actual;
    return result;
  }
  Result updateAdminPassword(const char *password, DeviceConfig *committed = nullptr) {
    DeviceConfig updated = value;
    strlcpy(updated.adminPassword, password, sizeof(updated.adminPassword));
    return commitUpdated(updated, committed);
  }

 private:
  Result commitUpdated(const DeviceConfig &updated, DeviceConfig *committed) {
    const Result result = commit(updated);
    if (result.ok() && committed != nullptr) *committed = value;
    return result;
  }
};
