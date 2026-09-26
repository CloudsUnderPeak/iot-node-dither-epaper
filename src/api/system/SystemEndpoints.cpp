#include "SystemEndpoints.h"

#include "api/shared/ApiResponse.h"
#include "api/shared/JsonReader.h"
#if IOT_FEATURE_SLEEP
#include "modules/sleep/SleepCoordinator.h"
#endif

namespace {
bool hasJsonKey(JsonObjectConst object, const char *key) {
  for (JsonPairConst pair : object) {
    if (strcmp(pair.key().c_str(), key) == 0) return true;
  }
  return false;
}

bool restoreClock(TimeSource &time, const TimeSnapshot &previous,
                  uint32_t startedMs) {
  if (!previous.synced()) {
    time.clear();
    return !time.snapshot().synced();
  }
  const int64_t restored = previous.epoch +
      static_cast<uint32_t>(millis() - startedMs) / 1000U;
  if (time.set(restored, previous.origin)) return true;
  time.clear();
  return false;
}
}  // namespace

Result SystemEndpoints::begin(ConfigService *configService,
                              StorageLifecycle *storageLifecycle,
                              RuntimeActionScheduler *runtime,
                              TimeSource *timeSource
#if IOT_FEATURE_SLEEP
                              , SleepCoordinator *sleepCoordinator
#endif
                              ) {
  if (configService == nullptr || storageLifecycle == nullptr || runtime == nullptr) {
    return invalidInput("missing system API dependencies");
  }
  configService_ = configService;
  storageLifecycle_ = storageLifecycle;
  runtime_ = runtime;
  timeSource_ = timeSource;
#if IOT_FEATURE_SLEEP
  sleepCoordinator_ = sleepCoordinator;
#endif
  return okResult();
}

Api::Response SystemEndpoints::updateTime(const Api::Request &request) {
  JsonObjectConst root;
  const Api::Response bodyResult = ApiRequest::requireObject(request, root);
  if (!bodyResult.success) return bodyResult;
  JsonDecodeError decodeError;
  JsonReader reader(root, "", decodeError);
  const uint32_t epoch = reader.requiredUint32("client_time", "client_time is required");
  reader.finish({"client_time"});
  if (!decodeError.ok()) return Api::decodeError(decodeError);
  if (epoch < 1704067200UL || epoch >= 4102444800UL) {
    return Api::problem(400, "invalid_field", "client_time is outside supported range",
                        "client_time");
  }
  if (timeSource_ == nullptr) {
    return Api::problem(503, "runtime_unavailable", "system clock unavailable");
  }
#if IOT_FEATURE_SLEEP
  if (sleepCoordinator_ && !sleepCoordinator_->beginTimeUpdate()) {
    return Api::problem(409, "sleep_entering", "sleep entry or clock update is in progress");
  }
  struct TimeUpdateLease {
    SleepCoordinator *owner;
    ~TimeUpdateLease() { if (owner) owner->endTimeUpdate(); }
  } lease{sleepCoordinator_};
#endif
  const TimeSnapshot previous = timeSource_->snapshot();
  const uint32_t startedMs = millis();
  if (!timeSource_->set(epoch, TimeOrigin::Client)) {
    const bool restored = restoreClock(*timeSource_, previous, startedMs);
    return Api::problem(500, "time_error",
                        restored ? "failed to set system clock"
                                 : "clock update and rollback failed");
  }
  const TimeSnapshot snapshot = timeSource_->snapshot();
  if (!snapshot.synced() || snapshot.epoch < epoch ||
      snapshot.epoch > static_cast<int64_t>(epoch) + 1) {
    const bool restored = restoreClock(*timeSource_, previous, startedMs);
    return Api::problem(500, "time_error",
                        restored ? "failed to verify system clock"
                                 : "clock verification and rollback failed");
  }
  JsonDocument data;
  data["epoch"] = snapshot.epoch;
  data["synced"] = snapshot.synced();
  data["source"] = timeOriginToString(snapshot.origin);
  return Api::ok(Api::json(data));
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

#if !IOT_FEATURE_AUTH
Api::Response SystemEndpoints::updateApPassword(const Api::Request &request) {
  JsonObjectConst root;
  const Api::Response bodyResult = ApiRequest::requireObject(request, root);
  if (!bodyResult.success) return bodyResult;
  JsonDecodeError error;
  JsonReader reader(root, "", error);
  const char *password = reader.requiredString("password", "password is required");
  reader.finish({"password"});
  if (!error.ok()) return Api::decodeError(error);
  const Result valid = validateAdminPasswordValue(password);
  if (!valid.ok()) return Api::problem(400, "invalid_field", valid.message, "password");
  DeviceConfig committed;
  const Result saved = configService_->updateAdminPassword(password, &committed, runtime_->ready());
  if (saved.code == ResultCode::Unsupported)
    return Api::problem(503, "runtime_unavailable", saved.message);
  if (!saved.ok()) return Api::problem(500, "storage_error", saved.message);
  if (committed.apPasswordEnabled) runtime_->scheduleSystemReset(300);
  return Api::ok("{}", "AP password updated");
}
#endif

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
