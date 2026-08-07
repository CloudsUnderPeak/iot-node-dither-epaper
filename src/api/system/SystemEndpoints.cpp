#include "SystemEndpoints.h"

#include "api/shared/ApiResponse.h"
#include "api/shared/JsonReader.h"

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
  const char *hostname = reader.requiredString("hostname", "hostname is required");
  reader.finish({"hostname"});
  if (!decodeError.ok()) return Api::decodeError(decodeError);

  const Result validation = validateHostnameValue(hostname);
  if (!validation.ok()) {
    return Api::problem(400, "invalid_field", validation.message, "hostname");
  }
  if (!runtime_->ready()) {
    return Api::problem(503, "runtime_unavailable", "runtime action scheduler unavailable");
  }
  DeviceConfig updated;
  const Result saveResult = configService_->updateHostname(hostname, &updated);
  if (!saveResult.ok()) {
    return Api::problem(Api::statusFor(saveResult.code), "storage_error", saveResult.message);
  }
  runtime_->scheduleWifiApply(100);

  JsonDocument data;
  data["hostname"] = updated.hostname;
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
