#pragma once

#include <Arduino.h>

#include "../../core/Result.h"
#include "../config/ConfigService.h"

class AuthService {
 public:
  Result begin(ConfigService *configService);
  bool credentialsMatch(const char *username, const char *password) const;
  Result login(const char *username, const char *password, String &token);
  bool tokenValid(const String &token) const;
  void invalidateSession();
  bool hasActiveSession() const;

 private:
  ConfigService *configService_ = nullptr;
  mutable SemaphoreHandle_t mutex_ = nullptr;
  String activeToken_;

  String generateToken() const;
  bool lock() const;
  void unlock() const;
};
