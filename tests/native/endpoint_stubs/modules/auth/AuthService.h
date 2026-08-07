#pragma once

#include <Arduino.h>

#include "core/Result.h"

class AuthService {
 public:
  bool invalidated = false;
  String validToken = "valid-token";

  bool credentialsMatch(const char *, const char *) const { return false; }
  Result login(const char *, const char *, String &) { return invalidInput("invalid credentials"); }
  void invalidateSession() { invalidated = true; }
  bool tokenValid(const String &token) const { return token == validToken.c_str(); }
};
