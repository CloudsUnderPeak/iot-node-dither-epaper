#include "AuthService.h"

#include <esp_random.h>

namespace {
void appendHexByte(String &target, uint8_t value) {
  constexpr char kHex[] = "0123456789abcdef";
  target += kHex[(value >> 4) & 0x0f];
  target += kHex[value & 0x0f];
}
}

Result AuthService::begin(ConfigService *configService) {
  if (configService == nullptr) {
    return invalidInput("missing auth config");
  }
  mutex_ = xSemaphoreCreateMutex();
  if (mutex_ == nullptr) {
    return outOfSpace("failed to create auth mutex");
  }
  configService_ = configService;
  activeToken_ = "";
  return okResult();
}

bool AuthService::credentialsMatch(const char *username, const char *password) const {
  if (configService_ == nullptr || username == nullptr || password == nullptr) {
    return false;
  }
  const DeviceConfig config = configService_->snapshot();
  return strcmp(username, config.adminUsername) == 0 &&
         strcmp(password, config.adminPassword) == 0;
}

Result AuthService::login(const char *username, const char *password, String &token) {
  if (!credentialsMatch(username, password)) {
    return invalidInput("invalid credentials");
  }

  if (!lock()) {
    return outOfSpace("auth service unavailable");
  }
  activeToken_ = generateToken();
  token = activeToken_;
  unlock();
  return okResult();
}

bool AuthService::tokenValid(const String &token) const {
  if (!lock()) {
    return false;
  }
  const bool valid = activeToken_.length() > 0 && token.length() > 0 && token == activeToken_;
  unlock();
  return valid;
}

void AuthService::invalidateSession() {
  if (!lock()) {
    return;
  }
  activeToken_ = "";
  unlock();
}

bool AuthService::hasActiveSession() const {
  if (!lock()) {
    return false;
  }
  const bool active = activeToken_.length() > 0;
  unlock();
  return active;
}

String AuthService::generateToken() const {
  // Opaque random tokens keep ESP32-side validation cheap: the device only
  // compares the active runtime token instead of signing/verifying a JWT.
  String token;
  token.reserve(32);
  for (uint8_t i = 0; i < 16; ++i) {
    appendHexByte(token, static_cast<uint8_t>(esp_random() & 0xff));
  }
  return token;
}

bool AuthService::lock() const {
  return mutex_ != nullptr && xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE;
}

void AuthService::unlock() const {
  xSemaphoreGive(mutex_);
}
