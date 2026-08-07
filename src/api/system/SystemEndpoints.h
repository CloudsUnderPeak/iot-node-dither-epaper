#pragma once

#include "api/shared/ApiTypes.h"
#include "modules/config/ConfigService.h"
#include "modules/runtime/RuntimeActionScheduler.h"
#include "modules/storage/StorageLifecycle.h"

// Handles PUT /api/system and the protected storage reset endpoints.
class SystemEndpoints {
 public:
  Result begin(ConfigService *configService,
               StorageLifecycle *storageLifecycle,
               RuntimeActionScheduler *runtime);
  Api::Response update(const Api::Request &request);
  Api::Response reset(StorageResetScope scope);

 private:
  ConfigService *configService_ = nullptr;
  StorageLifecycle *storageLifecycle_ = nullptr;
  RuntimeActionScheduler *runtime_ = nullptr;
};
