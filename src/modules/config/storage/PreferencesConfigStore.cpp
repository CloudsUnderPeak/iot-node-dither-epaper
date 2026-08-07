#include "PreferencesConfigStore.h"

#include <cstring>

#include "ConfigSchema.h"

namespace {
constexpr const char *kSlotNamespaces[] = {"devcfg_a", "devcfg_b"};
constexpr const char *kMetaNamespace = "devcfg_meta";
constexpr const char *kActiveSlotKey = "active";

constexpr const char *kSchemaKey = "schema";
constexpr const char *kWifiModeKey = "wifi_mode";
constexpr const char *kHostnameKey = "hostname";
constexpr const char *kStaSsidKey = "sta_ssid";
constexpr const char *kStaPassKey = "sta_pass";
constexpr const char *kStaSecurityKey = "sta_sec";
constexpr const char *kStaIpModeKey = "sta_ipmode";
constexpr const char *kStaIpAddressKey = "sta_addr";
constexpr const char *kStaIpGatewayKey = "sta_gw";
constexpr const char *kStaIpNetmaskKey = "sta_mask";
constexpr const char *kStaDns1Key = "sta_dns1";
constexpr const char *kStaDns2Key = "sta_dns2";
constexpr const char *kApSsidKey = "ap_ssid";
constexpr const char *kApPassEnabledKey = "ap_pass_on";
constexpr const char *kApIpModeKey = "ap_ipmode";
constexpr const char *kApIpAddressKey = "ap_addr";
constexpr const char *kApIpNetmaskKey = "ap_mask";
constexpr const char *kFallbackToApKey = "fb_ap";
constexpr const char *kAdminUserKey = "admin_user";
constexpr const char *kAdminPassKey = "admin_pass";

bool readRequiredString(PreferencesBackend &backend,
                        const char *key,
                        char *target,
                        size_t targetSize) {
  return backend.getString(key, target, targetSize);
}

bool hasAllScalarKeys(const PreferencesBackend &backend) {
  return backend.hasKey(kWifiModeKey) &&
         backend.hasKey(kStaSecurityKey) &&
         backend.hasKey(kStaIpModeKey) &&
         backend.hasKey(kApPassEnabledKey) &&
         backend.hasKey(kApIpModeKey) &&
         backend.hasKey(kFallbackToApKey);
}

Result loadSlot(PreferencesBackend &backend,
                uint8_t slot,
                DeviceConfig &config) {
  const PreferencesNamespaceState namespaceState =
      backend.inspectNamespace(kSlotNamespaces[slot]);
  if (namespaceState == PreferencesNamespaceState::Missing) {
    return notFound("config slot is empty");
  }
  if (namespaceState == PreferencesNamespaceState::StorageError) {
    return storageError("failed to inspect config slot");
  }

  if (!backend.open(kSlotNamespaces[slot], true)) {
    return storageError("failed to open config slot for read");
  }
  if (!backend.hasKey(kSchemaKey)) {
    backend.close();
    return notFound("config slot is empty");
  }

  const uint16_t storedSchema = backend.getUShort(kSchemaKey, 0);
  if (storedSchema != kDeviceConfigSchemaVersion) {
    backend.close();
    return unsupported("unsupported config schema; preserved without overwrite");
  }
  if (!hasAllScalarKeys(backend)) {
    backend.close();
    return storageError("config slot is incomplete");
  }

  DeviceConfig loaded = defaultDeviceConfig();
  loaded.schemaVersion = storedSchema;
  loaded.wifiMode = static_cast<WifiMode>(backend.getUChar(kWifiModeKey, 0));
  loaded.staSecurity =
      static_cast<StaSecurity>(backend.getUChar(kStaSecurityKey, 0));
  loaded.staIpMode =
      static_cast<StaIpMode>(backend.getUChar(kStaIpModeKey, 0));
  loaded.apPasswordEnabled = backend.getBool(kApPassEnabledKey, false);
  loaded.apIpMode =
      static_cast<ApIpMode>(backend.getUChar(kApIpModeKey, 0));
  loaded.fallbackToAp = backend.getBool(kFallbackToApKey, false);

  const bool stringsLoaded =
      readRequiredString(backend, kHostnameKey, loaded.hostname, sizeof(loaded.hostname)) &&
      readRequiredString(backend, kStaSsidKey, loaded.staSsid, sizeof(loaded.staSsid)) &&
      readRequiredString(backend, kStaPassKey, loaded.staPassword, sizeof(loaded.staPassword)) &&
      readRequiredString(backend, kStaIpAddressKey, loaded.staIpAddress, sizeof(loaded.staIpAddress)) &&
      readRequiredString(backend, kStaIpGatewayKey, loaded.staIpGateway, sizeof(loaded.staIpGateway)) &&
      readRequiredString(backend, kStaIpNetmaskKey, loaded.staIpNetmask, sizeof(loaded.staIpNetmask)) &&
      readRequiredString(backend, kStaDns1Key, loaded.staDns1, sizeof(loaded.staDns1)) &&
      readRequiredString(backend, kStaDns2Key, loaded.staDns2, sizeof(loaded.staDns2)) &&
      readRequiredString(backend, kApSsidKey, loaded.apSsid, sizeof(loaded.apSsid)) &&
      readRequiredString(backend, kApIpAddressKey, loaded.apIpAddress, sizeof(loaded.apIpAddress)) &&
      readRequiredString(backend, kApIpNetmaskKey, loaded.apIpNetmask, sizeof(loaded.apIpNetmask)) &&
      readRequiredString(backend, kAdminUserKey, loaded.adminUsername, sizeof(loaded.adminUsername)) &&
      readRequiredString(backend, kAdminPassKey, loaded.adminPassword, sizeof(loaded.adminPassword));
  backend.close();

  if (!stringsLoaded) {
    return storageError("config slot has invalid strings");
  }
  const Result validation = validateDeviceConfig(loaded);
  if (!validation.ok()) {
    return validation;
  }
  config = loaded;
  return okResult();
}

bool writeSlot(PreferencesBackend &backend,
               uint8_t slot,
               const DeviceConfig &config) {
  if (!backend.open(kSlotNamespaces[slot], false)) {
    return false;
  }

  bool saved = backend.clear();
  saved = saved && backend.putUChar(kWifiModeKey, static_cast<uint8_t>(config.wifiMode));
  saved = saved && backend.putString(kHostnameKey, config.hostname);
  saved = saved && backend.putString(kStaSsidKey, config.staSsid);
  saved = saved && backend.putString(kStaPassKey, config.staPassword);
  saved = saved && backend.putUChar(kStaSecurityKey, static_cast<uint8_t>(config.staSecurity));
  saved = saved && backend.putUChar(kStaIpModeKey, static_cast<uint8_t>(config.staIpMode));
  saved = saved && backend.putString(kStaIpAddressKey, config.staIpAddress);
  saved = saved && backend.putString(kStaIpGatewayKey, config.staIpGateway);
  saved = saved && backend.putString(kStaIpNetmaskKey, config.staIpNetmask);
  saved = saved && backend.putString(kStaDns1Key, config.staDns1);
  saved = saved && backend.putString(kStaDns2Key, config.staDns2);
  saved = saved && backend.putString(kApSsidKey, config.apSsid);
  saved = saved && backend.putBool(kApPassEnabledKey, config.apPasswordEnabled);
  saved = saved && backend.putUChar(kApIpModeKey, static_cast<uint8_t>(config.apIpMode));
  saved = saved && backend.putString(kApIpAddressKey, config.apIpAddress);
  saved = saved && backend.putString(kApIpNetmaskKey, config.apIpNetmask);
  saved = saved && backend.putBool(kFallbackToApKey, config.fallbackToAp);
  saved = saved && backend.putString(kAdminUserKey, config.adminUsername);
  saved = saved && backend.putString(kAdminPassKey, config.adminPassword);
  // Schema is the commit marker inside a slot and is always written last.
  saved = saved && backend.putUShort(kSchemaKey, kDeviceConfigSchemaVersion);
  backend.close();
  return saved;
}

bool configsEqual(const DeviceConfig &left, const DeviceConfig &right) {
  return left.schemaVersion == right.schemaVersion &&
         left.wifiMode == right.wifiMode &&
         strcmp(left.hostname, right.hostname) == 0 &&
         strcmp(left.staSsid, right.staSsid) == 0 &&
         strcmp(left.staPassword, right.staPassword) == 0 &&
         left.staSecurity == right.staSecurity &&
         left.staIpMode == right.staIpMode &&
         strcmp(left.staIpAddress, right.staIpAddress) == 0 &&
         strcmp(left.staIpGateway, right.staIpGateway) == 0 &&
         strcmp(left.staIpNetmask, right.staIpNetmask) == 0 &&
         strcmp(left.staDns1, right.staDns1) == 0 &&
         strcmp(left.staDns2, right.staDns2) == 0 &&
         strcmp(left.apSsid, right.apSsid) == 0 &&
         left.apPasswordEnabled == right.apPasswordEnabled &&
         left.apIpMode == right.apIpMode &&
         strcmp(left.apIpAddress, right.apIpAddress) == 0 &&
         strcmp(left.apIpNetmask, right.apIpNetmask) == 0 &&
         left.fallbackToAp == right.fallbackToAp &&
         strcmp(left.adminUsername, right.adminUsername) == 0 &&
         strcmp(left.adminPassword, right.adminPassword) == 0;
}

bool readActiveSlot(PreferencesBackend &backend, uint8_t &slot) {
  if (backend.inspectNamespace(kMetaNamespace) !=
      PreferencesNamespaceState::Exists) {
    return false;
  }

  if (!backend.open(kMetaNamespace, true)) {
    return false;
  }
  const bool found = backend.hasKey(kActiveSlotKey);
  if (found) {
    slot = backend.getUChar(kActiveSlotKey, 0xff);
  }
  backend.close();
  return found && slot < 2;
}

bool writeActiveSlot(PreferencesBackend &backend, uint8_t slot) {
  if (!backend.open(kMetaNamespace, false)) {
    return false;
  }
  const bool saved = backend.putUChar(kActiveSlotKey, slot);
  backend.close();
  return saved;
}

}  // namespace

PreferencesConfigStore::PreferencesConfigStore(PreferencesBackend &backend)
    : backend_(backend) {}

Result PreferencesConfigStore::load(DeviceConfig &config) {
  uint8_t activeSlot = 0;
  const bool hasActiveSlot = readActiveSlot(backend_, activeSlot);
  bool sawUnsupported = false;
  bool sawCorruption = false;
  if (hasActiveSlot) {
    const Result activeResult = loadSlot(backend_, activeSlot, config);
    if (activeResult.ok()) {
      return activeResult;
    }
    sawUnsupported = activeResult.code == ResultCode::Unsupported;
    sawCorruption = activeResult.code != ResultCode::NotFound &&
                    activeResult.code != ResultCode::Unsupported;
  }

  for (uint8_t slot = 0; slot < 2; ++slot) {
    if (hasActiveSlot && slot == activeSlot) {
      continue;
    }
    const Result result = loadSlot(backend_, slot, config);
    if (result.ok()) {
      writeActiveSlot(backend_, slot);
      return result;
    }
    sawUnsupported = sawUnsupported || result.code == ResultCode::Unsupported;
    sawCorruption = sawCorruption || (result.code != ResultCode::NotFound && result.code != ResultCode::Unsupported);
  }

  if (sawUnsupported) {
    return unsupported("unsupported config schema; preserved without overwrite");
  }
  if (sawCorruption) {
    return storageError("no valid config slot");
  }
  return notFound("config is missing");
}

Result PreferencesConfigStore::save(const DeviceConfig &config) {
  const Result validation = validateDeviceConfig(config);
  if (!validation.ok()) {
    return validation;
  }

  uint8_t activeSlot = 0;
  const bool hasActiveSlot = readActiveSlot(backend_, activeSlot);
  const uint8_t targetSlot = hasActiveSlot ? static_cast<uint8_t>(1U - activeSlot) : 0;
  if (!writeSlot(backend_, targetSlot, config)) {
    return storageError("failed to save config values");
  }

  DeviceConfig verified{};
  const Result verifyResult = loadSlot(backend_, targetSlot, verified);
  if (!verifyResult.ok() || !configsEqual(config, verified)) {
    return storageError("failed to verify saved config values");
  }
  if (!writeActiveSlot(backend_, targetSlot)) {
    return storageError("failed to commit config slot");
  }
  return okResult();
}
