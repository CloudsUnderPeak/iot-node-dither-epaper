#pragma once

#include "api/shared/ApiTypes.h"
#include "modules/auth/AuthService.h"
#include "modules/config/ConfigService.h"
#include "modules/runtime/RuntimeActionScheduler.h"

// Handles every /api/auth and /api/auth/* endpoint.
class AuthEndpoints {
 public:
  Result begin(ConfigService *configService,
               AuthService *authService,
               RuntimeActionScheduler *runtime);
  Api::Response info() const;
  Api::Response login(const Api::Request &request);
  Api::Response verify(const Api::Request &request) const;
  Api::Response session() const;
  Api::Response logout();
  Api::Response updatePassword(const Api::Request &request);

 private:
  ConfigService *configService_ = nullptr;
  AuthService *authService_ = nullptr;
  RuntimeActionScheduler *runtime_ = nullptr;
};
