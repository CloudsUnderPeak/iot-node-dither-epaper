#include "ConfigService.h"

#include <cstring>

namespace {
void copyWifiFields(const DeviceConfig &source, DeviceConfig &target) {
  target.wifiMode = source.wifiMode;
  memcpy(target.staSsid, source.staSsid, sizeof(target.staSsid));
  memcpy(target.staPassword, source.staPassword, sizeof(target.staPassword));
  target.staSecurity = source.staSecurity;
  target.staIpMode = source.staIpMode;
  memcpy(target.staIpAddress, source.staIpAddress, sizeof(target.staIpAddress));
  memcpy(target.staIpGateway, source.staIpGateway, sizeof(target.staIpGateway));
  memcpy(target.staIpNetmask, source.staIpNetmask, sizeof(target.staIpNetmask));
  memcpy(target.staDns1, source.staDns1, sizeof(target.staDns1));
  memcpy(target.staDns2, source.staDns2, sizeof(target.staDns2));
  memcpy(target.apSsid, source.apSsid, sizeof(target.apSsid));
  target.apPasswordEnabled = source.apPasswordEnabled;
  target.apIpMode = source.apIpMode;
  memcpy(target.apIpAddress, source.apIpAddress, sizeof(target.apIpAddress));
  memcpy(target.apIpNetmask, source.apIpNetmask, sizeof(target.apIpNetmask));
  target.fallbackToAp = source.fallbackToAp;
}
}  // namespace

ConfigService::ConfigService(ConfigStore &store) : store_(store) {}

const char *configStartupStateToString(ConfigStartupState state) {
  switch (state) {
    case ConfigStartupState::Uninitialized: return "uninitialized";
    case ConfigStartupState::Persisted: return "persisted";
    case ConfigStartupState::FactoryDefaultsCreated: return "factory_defaults_created";
    case ConfigStartupState::RecoveryDefaults: return "recovery_defaults";
  }
  return "unknown";
}

const char *configRecoveryReasonToString(ResultCode reason) {
  switch (reason) {
    case ResultCode::Ok: return "none";
    case ResultCode::Unsupported: return "unsupported_schema";
    case ResultCode::StorageError: return "storage_error";
    case ResultCode::NotFound: return "not_found";
    default: return "invalid_persisted_config";
  }
}

Result ConfigService::begin() {
  if (mutex_ == nullptr) {
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) {
      return outOfSpace("failed to create config mutex");
    }
  }

  DeviceConfig loaded{};
  const Result loadResult = store_.load(loaded);
  if (loadResult.ok()) {
    active_ = loaded;
    ready_ = true;
    startupState_ = ConfigStartupState::Persisted;
    recoveryReason_ = ResultCode::Ok;
    return okResult();
  }

  loaded = defaultDeviceConfig();
  if (loadResult.code == ResultCode::NotFound) {
    const Result saveResult = store_.save(loaded);
    if (!saveResult.ok()) {
      return saveResult;
    }
    startupState_ = ConfigStartupState::FactoryDefaultsCreated;
    recoveryReason_ = ResultCode::Ok;
  } else {
    startupState_ = ConfigStartupState::RecoveryDefaults;
    recoveryReason_ = loadResult.code;
  }
  // Unsupported or damaged persisted data is preserved. Recovery defaults are
  // used in RAM so the setup AP remains reachable.
  active_ = loaded;
  ready_ = true;
  return okResult();
}

DeviceConfig ConfigService::snapshot() const {
  DeviceConfig copy{};
  if (!lock()) {
    return copy;
  }
  copy = active_;
  unlock();
  return copy;
}

Result ConfigService::commit(const DeviceConfig &updated) {
  const Result validation = validateDeviceConfig(updated);
  if (!validation.ok()) {
    return validation;
  }
  if (!lock()) {
    return storageError("config service unavailable");
  }
  const Result saveResult = saveLocked(updated, nullptr);
  unlock();
  return saveResult;
}

Result ConfigService::updateWifi(const DeviceConfig &source, DeviceConfig *committed) {
  if (!lock()) {
    return storageError("config service unavailable");
  }
  DeviceConfig updated = active_;
  copyWifiFields(source, updated);
  const Result result = saveLocked(updated, committed);
  unlock();
  return result;
}

Result ConfigService::updateHostname(const char *hostname, DeviceConfig *committed) {
  const Result validation = validateHostnameValue(hostname);
  if (!validation.ok()) {
    return validation;
  }
  if (!lock()) {
    return storageError("config service unavailable");
  }
  DeviceConfig updated = active_;
  strlcpy(updated.hostname, hostname, sizeof(updated.hostname));
  const Result result = saveLocked(updated, committed);
  unlock();
  return result;
}

Result ConfigService::updateAdminPassword(const char *password, DeviceConfig *committed) {
  const Result validation = validateAdminPasswordValue(password);
  if (!validation.ok()) {
    return validation;
  }
  if (!lock()) {
    return storageError("config service unavailable");
  }
  DeviceConfig updated = active_;
  strlcpy(updated.adminPassword, password, sizeof(updated.adminPassword));
  const Result result = saveLocked(updated, committed);
  unlock();
  return result;
}

bool ConfigService::ready() const {
  return ready_;
}

ConfigStartupState ConfigService::startupState() const {
  return startupState_;
}

ResultCode ConfigService::recoveryReason() const {
  return recoveryReason_;
}

bool ConfigService::lock() const {
  return mutex_ != nullptr && xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE;
}

void ConfigService::unlock() const {
  xSemaphoreGive(mutex_);
}

Result ConfigService::saveLocked(const DeviceConfig &updated, DeviceConfig *committed) {
  const Result validation = validateDeviceConfig(updated);
  if (!validation.ok()) {
    return validation;
  }
  const Result saveResult = store_.save(updated);
  if (!saveResult.ok()) {
    return saveResult;
  }
  active_ = updated;
  ready_ = true;
  if (committed != nullptr) {
    *committed = updated;
  }
  return saveResult;
}
