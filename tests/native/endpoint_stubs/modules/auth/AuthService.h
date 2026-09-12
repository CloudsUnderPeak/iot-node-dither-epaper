#pragma once

#include <Arduino.h>

#include "core/Result.h"
#include "modules/config/ConfigService.h"

class AuthService {
 public:
  ConfigService *config = nullptr;
  Result changePassword(const char *password, bool runtimeAvailable, bool &restartRequired) {
    restartRequired = false;
    if (config == nullptr) return storageError("missing config");
    if (config->value.apPasswordEnabled && !runtimeAvailable) return unsupported("runtime unavailable");
    DeviceConfig committed;
    auto result = config->updateAdminPassword(password, &committed);
    if (result.ok()) { invalidated = true; restartRequired = committed.apPasswordEnabled; }
    return result;
  }
  bool invalidated = false;
  String validToken = "valid-token";

  bool credentialsMatch(const char *, const char *) const { return false; }
  Result login(const char *, const char *, String &) { return invalidInput("invalid credentials"); }
  void invalidateSession() { invalidated = true; }
  bool tokenValid(const String &token) const { return token == validToken.c_str(); }
};
