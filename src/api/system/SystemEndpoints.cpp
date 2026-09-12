#include "SystemEndpoints.h"

#include "api/shared/ApiResponse.h"
#include "api/shared/JsonReader.h"

namespace {
bool hasJsonKey(JsonObjectConst object, const char *key) {
  for (JsonPairConst pair : object) {
    if (strcmp(pair.key().c_str(), key) == 0) return true;
  }
  return false;
}
}  // namespace

Result SystemEndpoints::begin(ConfigService *configService,
                              StorageLifecycle *storageLifecycle,
                              RuntimeActionScheduler *runtime) {
  if (configService == nullptr || storageLifecycle == nullptr || runtime == nullptr) {
    return invalidInput("missing system API dependencies");
  }
  configService_ = configService;
  storageLifecycle_ = storageLifecycle;
  runtime_ = runtime;
  return okResult();
}

Api::Response SystemEndpoints::update(const Api::Request &request) {
  JsonObjectConst root;
  const Api::Response bodyResult = ApiRequest::requireObject(request, root);
  if (!bodyResult.success) return bodyResult;

  JsonDecodeError decodeError;
  JsonReader reader(root, "", decodeError);
  SystemConfigUpdate update;
  update.hostnameProvided = hasJsonKey(root, "hostname");
  if (update.hostnameProvided) {
    update.hostname = reader.requiredString("hostname", "hostname is required");
  }
  update.wifiTxDbmProvided = hasJsonKey(root, "wifi_tx_dbm");
  if (update.wifiTxDbmProvided) {
    JsonVariantConst value = root["wifi_tx_dbm"];
    if (value.isNull() || !value.is<int64_t>()) {
      return Api::problem(400, "invalid_field",
                          "wifi_tx_dbm must be an integer",
                          "wifi_tx_dbm");
    } else {
      const int64_t dbm = value.as<int64_t>();
      if (dbm < kMinWifiTxDbm || dbm > kMaxWifiTxDbm) {
        return Api::problem(400, "invalid_field",
                            "wifi_tx_dbm must be between 2 and 20",
                            "wifi_tx_dbm");
      }
      update.wifiTxDbm = static_cast<uint8_t>(dbm);
    }
  }
  reader.finish({"hostname", "wifi_tx_dbm"});
  if (!decodeError.ok()) return Api::decodeError(decodeError);

  if (!update.hostnameProvided && !update.wifiTxDbmProvided) {
    return Api::problem(400, "missing_field",
                        "hostname or wifi_tx_dbm is required");
  }
  if (update.hostnameProvided) {
    const Result validation = validateHostnameValue(update.hostname);
    if (!validation.ok()) {
      return Api::problem(400, "invalid_field", validation.message, "hostname");
    }
  }
  if (!runtime_->ready()) {
    return Api::problem(503, "runtime_unavailable", "runtime action scheduler unavailable");
  }
  DeviceConfig updated;
  SystemConfigChanges changes;
  const Result saveResult = configService_->updateSystem(update, &updated, &changes);
  if (!saveResult.ok()) {
    return Api::problem(Api::statusFor(saveResult.code), "storage_error", saveResult.message);
  }
  if (changes.hostnameChanged) {
    runtime_->scheduleWifiApply(100);
  } else if (changes.wifiTxDbmChanged) {
    runtime_->scheduleWifiTxPowerApply(100);
  }

  JsonDocument data;
  data["hostname"] = updated.hostname;
  data["wifi_tx_dbm"] = updated.wifiTxDbm;
  return Api::ok(Api::json(data), "system updated");
}

Api::Response SystemEndpoints::reset(StorageResetScope scope) {
  if (!runtime_->ready()) {
    return Api::problem(503, "runtime_unavailable", "runtime action scheduler unavailable");
  }
  const Result resetResult = storageLifecycle_->requestReset(scope);
  if (!resetResult.ok()) {
    return Api::problem(Api::statusFor(resetResult.code), "storage_error", resetResult.message);
  }
  runtime_->scheduleSystemReset(300);
  return Api::ok("{}", "reset scheduled; restarting");
}
