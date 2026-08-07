#include "WifiEndpoints.h"

#include "api/shared/ApiResponse.h"
#include "api/shared/JsonReader.h"
#include "WifiPayload.h"

#include <cstring>

Result WifiEndpoints::begin(ConfigService *configService,
                            WifiManager *wifiManager,
                            WifiScanner *wifiScanner,
                            RuntimeActionScheduler *runtime) {
  if (configService == nullptr || wifiManager == nullptr || wifiScanner == nullptr || runtime == nullptr) {
    return invalidInput("missing Wi-Fi API dependencies");
  }
  configService_ = configService;
  wifiManager_ = wifiManager;
  wifiScanner_ = wifiScanner;
  runtime_ = runtime;
  return okResult();
}

Api::Response WifiEndpoints::get() const {
  const DeviceConfig config = configService_->snapshot();
  const WifiStatus status = wifiManager_->status();

  JsonDocument data;
  data["mode"] = WifiPayload::wifiModeToApiString(status.mode);
  data["configured_mode"] = WifiPayload::wifiModeToApiString(config.wifiMode);
  data["fallback_to_ap"] = config.fallbackToAp;
  JsonObject interfaces = data["interfaces"].to<JsonObject>();

  JsonObject sta = interfaces["sta"].to<JsonObject>();
  sta["enabled"] = status.staEnabled;
  sta["ssid"] = config.staSsid;
  sta["security"] = staSecurityToString(config.staSecurity);
  sta["state"] = wifiLinkStateToString(status.staState);
  sta["ip"] = status.staIp.toString();
  JsonObject staIp = sta["ip_config"].to<JsonObject>();
  staIp["mode"] = staIpModeToString(config.staIpMode);
  staIp["address"] = config.staIpAddress;
  staIp["gateway"] = config.staIpGateway;
  staIp["netmask"] = config.staIpNetmask;
  JsonArray dns = staIp["dns"].to<JsonArray>();
  if (config.staDns1[0] != '\0') dns.add(config.staDns1);
  if (config.staDns2[0] != '\0') dns.add(config.staDns2);

  JsonObject ap = interfaces["ap"].to<JsonObject>();
  ap["enabled"] = status.apEnabled;
  ap["state"] = wifiApStateToString(status.apState);
  ap["ssid"] = config.apSsid;
  ap["password_enabled"] = config.apPasswordEnabled;
  ap["ip"] = status.apIp.toString();
  JsonObject apIp = ap["ip_config"].to<JsonObject>();
  apIp["mode"] = apIpModeToString(config.apIpMode);
  apIp["address"] = config.apIpAddress;
  apIp["netmask"] = config.apIpNetmask;
  return Api::ok(Api::json(data));
}

Api::Response WifiEndpoints::scan() {
  if (wifiManager_->testBlocksScan()) {
    return Api::problem(409, "wifi_connect_busy", "wifi connection is using the radio");
  }
  if (wifiManager_->staConnectionBlocksScan()) {
    JsonDocument data;
    data["code"] = "wifi_scan_busy";
    data["retry_after_seconds"] = 1;
    return Api::error(409, Api::json(data), "wifi connection is using the radio");
  }
  const WifiScanResult scanResult = wifiScanner_->scan();
  if (scanResult.retryAfterSeconds > 0) {
    JsonDocument data;
    data["code"] = "rate_limited";
    data["retry_after_seconds"] = scanResult.retryAfterSeconds;
    return Api::error(429, Api::json(data), "wifi scan rate limited");
  }
  if (!scanResult.result.ok()) {
    return Api::problem(500, "wifi_scan_failed", "wifi scan failed");
  }

  JsonDocument data;
  JsonArray networks = data["networks"].to<JsonArray>();
  for (size_t index = 0; index < scanResult.count; ++index) {
    const WifiScanNetwork &source = scanResult.networks[index];
    JsonObject network = networks.add<JsonObject>();
    network["ssid"] = source.ssid;
    network["rssi"] = source.rssi;
    network["channel"] = source.channel;
    network["hidden"] = source.hidden;
    network["encryption"] = source.encryption;
    network["encryption_type"] = source.encryptionType;
  }
  return Api::ok(Api::json(data));
}

Api::Response WifiEndpoints::update(const Api::Request &request) {
  const DeviceConfig current = configService_->snapshot();
  DeviceConfig updated;
  Api::Response errorResponse;
  if (!decodeCandidate(request, current, updated, errorResponse)) return errorResponse;
  if (!runtime_->ready()) {
    return Api::problem(503, "runtime_unavailable", "runtime action scheduler unavailable");
  }
  if (wifiManager_->testBlocksScan()) {
    return Api::problem(409, "wifi_connect_busy", "wifi connection is using the radio");
  }

  if (current.wifiMode == WifiMode::Ap &&
      (updated.wifiMode == WifiMode::Sta || updated.wifiMode == WifiMode::ApSta)) {
    uint32_t testId = 0;
    const Result queueResult = wifiManager_->queueStaTest(updated, current, testId);
    if (!queueResult.ok()) {
      if (queueResult.code == ResultCode::Unsupported) {
        return Api::problem(409, "wifi_connect_busy", queueResult.message);
      }
      if (queueResult.code == ResultCode::NetworkError) {
        return Api::problem(409, "wifi_connect_requires_ap", queueResult.message);
      }
      return Api::problem(400, "invalid_field", queueResult.message);
    }

    JsonDocument data;
    data["state"] = "connecting";
    return Api::accepted(Api::json(data), "wifi transition started");
  }

  const Result saveResult = configService_->updateWifi(updated);
  if (!saveResult.ok()) {
    return Api::problem(Api::statusFor(saveResult.code), "storage_error", saveResult.message);
  }
  if (current.apPasswordEnabled != updated.apPasswordEnabled) {
    runtime_->scheduleSystemReset(300);
  } else {
    runtime_->scheduleWifiApply(100);
  }
  return Api::ok("{}", "wifi updated");
}

Api::Response WifiEndpoints::connect(const Api::Request &request) {
  if (!runtime_->ready()) {
    return Api::problem(503, "runtime_unavailable", "runtime action scheduler unavailable");
  }

  JsonObjectConst root;
  const Api::Response bodyResult = ApiRequest::requireObject(request, root);
  if (!bodyResult.success) return bodyResult;
  const Result stringValidation = WifiPayload::validateJsonStringTree(root);
  if (!stringValidation.ok()) {
    return Api::problem(400, "invalid_field", stringValidation.message);
  }

  JsonDecodeError decodeError;
  JsonReader reader(root, "", decodeError);
  const char *ssid = reader.requiredString("ssid", "ssid is required");
  bool passwordProvided = false;
  const char *password = reader.optionalString(
      "password", passwordProvided, "password must be a string");
  reader.finish({"ssid", "password"});
  if (!decodeError.ok()) return Api::decodeError(decodeError);

  const Result ssidValidation = validateStaSsidValue(ssid, true);
  if (!ssidValidation.ok()) {
    return Api::problem(400, "invalid_field", ssidValidation.message, "ssid");
  }

  const DeviceConfig current = configService_->snapshot();
  const bool reusePassword = !passwordProvided &&
                             strcmp(ssid, current.staSsid) == 0 &&
                             current.staSecurity == StaSecurity::Wpa &&
                             current.staPassword[0] != '\0';
  DeviceConfig candidate = current;
  candidate.wifiMode = WifiMode::ApSta;
  candidate.fallbackToAp = true;
  candidate.staIpMode = StaIpMode::Dhcp;
  candidate.staIpAddress[0] = candidate.staIpGateway[0] = candidate.staIpNetmask[0] = '\0';
  candidate.staDns1[0] = candidate.staDns2[0] = '\0';
  strlcpy(candidate.staSsid, ssid, sizeof(candidate.staSsid));

  if (!reusePassword) {
    if (!passwordProvided || password[0] == '\0') {
      candidate.staSecurity = StaSecurity::Open;
      candidate.staPassword[0] = '\0';
    } else {
      const Result passwordValidation = validateStaPasswordValue(password);
      if (!passwordValidation.ok()) {
        return Api::problem(400, "invalid_field", passwordValidation.message, "password");
      }
      candidate.staSecurity = StaSecurity::Wpa;
      strlcpy(candidate.staPassword, password, sizeof(candidate.staPassword));
    }
  }

  const DeviceConfigValidation candidateValidation = validateDeviceConfigDetailed(candidate);
  if (!candidateValidation.ok()) {
    return Api::problem(400, "invalid_field", candidateValidation.result.message);
  }

  uint32_t testId = 0;
  const Result queueResult = wifiManager_->queueStaTest(candidate, current, testId);
  if (!queueResult.ok()) {
    if (queueResult.code == ResultCode::Unsupported) {
      return Api::problem(409, "wifi_connect_busy", queueResult.message);
    }
    if (queueResult.code == ResultCode::NetworkError) {
      return Api::problem(409, "wifi_connect_requires_ap", queueResult.message);
    }
    return Api::problem(400, "invalid_field", queueResult.message);
  }

  JsonDocument data;
  data["state"] = "connecting";
  return Api::accepted(Api::json(data), "wifi connection started");
}

Api::Response WifiEndpoints::connectionStatus() const {
  const WifiTestStatus status = wifiManager_->testStatus();
  JsonDocument data;
  const char *state = "idle";
  if (status.state == WifiTestState::Queued || status.state == WifiTestState::Testing ||
      status.state == WifiTestState::Committing ||
      status.state == WifiTestState::RollbackPending ||
      status.state == WifiTestState::RollingBack ||
      status.state == WifiTestState::Restoring ||
      (status.state == WifiTestState::Finalizing && status.apShutdownInSeconds == 0) ||
      (status.state == WifiTestState::Succeeded && !status.persisted)) {
    state = "connecting";
  } else if ((status.state == WifiTestState::Finalizing && status.apShutdownInSeconds > 0) ||
             (status.state == WifiTestState::Succeeded && status.persisted)) {
    state = "connected";
  } else if (status.state == WifiTestState::Failed || status.state == WifiTestState::Expired) {
    state = "failed";
  }
  data["state"] = state;
  data["failure_code"] = wifiTestFailureToString(status.failure);
  data["ip"] = status.staIp.toString();
  data["ap_shutdown_in_seconds"] = status.apShutdownInSeconds;
  return Api::ok(Api::json(data));
}

Api::Response WifiEndpoints::reconnect() {
  if (!runtime_->ready()) {
    return Api::problem(503, "runtime_unavailable", "runtime action scheduler unavailable");
  }
  if (wifiManager_->testBlocksScan()) {
    return Api::problem(409, "wifi_connect_busy", "wifi connection is using the radio");
  }
  runtime_->scheduleWifiApply(100);
  return Api::ok("{}", "wifi reconnecting");
}

bool WifiEndpoints::decodeCandidate(const Api::Request &request,
                                    const DeviceConfig &current,
                                    DeviceConfig &candidate,
                                    Api::Response &errorResponse) const {
  JsonObjectConst root;
  const Api::Response bodyResult = ApiRequest::requireObject(request, root);
  if (!bodyResult.success) {
    errorResponse = bodyResult;
    return false;
  }

  const Result stringValidation = WifiPayload::validateJsonStringTree(root);
  if (!stringValidation.ok()) {
    errorResponse = Api::problem(400, "invalid_field", stringValidation.message);
    return false;
  }

  WifiPayload::Request decoded;
  JsonDecodeError decodeError;
  if (!WifiPayload::decode(root, decoded, decodeError)) {
    errorResponse = Api::decodeError(decodeError);
    return false;
  }
  if (!WifiPayload::apply(decoded, current, candidate, decodeError)) {
    errorResponse = Api::decodeError(decodeError);
    return false;
  }
  return true;
}
