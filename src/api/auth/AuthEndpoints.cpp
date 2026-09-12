#include "AuthEndpoints.h"

#include "api/shared/ApiResponse.h"
#include "api/shared/JsonReader.h"

Result AuthEndpoints::begin(ConfigService *configService,
                            AuthService *authService,
                            RuntimeActionScheduler *runtime) {
  if (configService == nullptr || authService == nullptr || runtime == nullptr) {
    return invalidInput("missing auth API dependencies");
  }
  configService_ = configService;
  authService_ = authService;
  runtime_ = runtime;
  return okResult();
}

Api::Response AuthEndpoints::info() const {
  const DeviceConfig config = configService_->snapshot();
  JsonDocument data;
  data["username"] = config.adminUsername;
  return Api::ok(Api::json(data));
}

Api::Response AuthEndpoints::login(const Api::Request &request) {
  JsonObjectConst root;
  const Api::Response bodyResult = ApiRequest::requireObject(request, root);
  if (!bodyResult.success) return bodyResult;

  const char *username = "";
  const char *password = "";
  JsonDecodeError decodeError;
  if (!ApiRequest::decodeCredentials(root, username, password, decodeError)) {
    return Api::decodeError(decodeError);
  }

  String token;
  if (!authService_->login(username, password, token).ok()) {
    return Api::unauthorized("invalid credentials");
  }
  JsonDocument data;
  data["authenticated"] = true;
  data["token_type"] = "Bearer";
  data["token"] = token;
  return Api::ok(Api::json(data), "authenticated");
}

Api::Response AuthEndpoints::verify(const Api::Request &request) const {
  JsonObjectConst root;
  const Api::Response bodyResult = ApiRequest::requireObject(request, root);
  if (!bodyResult.success) return bodyResult;

  const char *username = "";
  const char *password = "";
  JsonDecodeError decodeError;
  if (!ApiRequest::decodeCredentials(root, username, password, decodeError)) {
    return Api::decodeError(decodeError);
  }
  const bool authenticated = authService_->credentialsMatch(username, password);
  return Api::ok(authenticated ? "{\"authenticated\":true}" : "{\"authenticated\":false}",
                 authenticated ? "credentials valid" : "credentials invalid");
}

Api::Response AuthEndpoints::session() const {
  return Api::ok("{\"authenticated\":true}", "authenticated");
}

Api::Response AuthEndpoints::logout() {
  authService_->invalidateSession();
  return Api::ok("{}", "logged out");
}

Api::Response AuthEndpoints::updatePassword(const Api::Request &request) {
  JsonObjectConst root;
  const Api::Response bodyResult = ApiRequest::requireObject(request, root);
  if (!bodyResult.success) return bodyResult;

  JsonDecodeError decodeError;
  JsonReader reader(root, "", decodeError);
  const char *password = reader.requiredString("password", "password is required");
  reader.finish({"password"});
  if (!decodeError.ok()) return Api::decodeError(decodeError);

  const Result validation = validateAdminPasswordValue(password);
  if (!validation.ok()) {
    return Api::problem(400, "invalid_field", validation.message, "password");
  }

  bool restartRequired = false;
  const Result saveResult = authService_->changePassword(
      password, runtime_->ready(), restartRequired);
  if (saveResult.code == ResultCode::Unsupported) {
    return Api::problem(503, "runtime_unavailable", saveResult.message);
  }
  if (!saveResult.ok()) {
    return Api::problem(Api::statusFor(saveResult.code), "storage_error", saveResult.message);
  }
  if (restartRequired) {
    runtime_->scheduleSystemReset(300);
  }
  return Api::ok("{\"session\":\"invalidated\"}", "admin password updated");
}
