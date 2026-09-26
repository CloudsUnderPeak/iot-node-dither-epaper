#pragma once

#include "api/shared/ApiTypes.h"
#include "modules/config/ConfigService.h"
#include "modules/runtime/RuntimeActionScheduler.h"
#include "modules/storage/StorageLifecycle.h"
#include "modules/time/TimeSource.h"
#include "modules/sleep/SleepFeatures.h"
#if ENABLE_SLEEP_SCHEDULER
class SleepCoordinator;
#endif

// Handles PUT /api/system and the protected storage reset endpoints.
class SystemEndpoints {
 public:
  Result begin(ConfigService *configService,
               StorageLifecycle *storageLifecycle,
               RuntimeActionScheduler *runtime,
               TimeSource *timeSource = nullptr
#if ENABLE_SLEEP_SCHEDULER
               , SleepCoordinator *sleepCoordinator = nullptr
#endif
               );
  Api::Response update(const Api::Request &request);
  Api::Response updateTime(const Api::Request &request);
  Api::Response reset(StorageResetScope scope);

 private:
  ConfigService *configService_ = nullptr;
  StorageLifecycle *storageLifecycle_ = nullptr;
  RuntimeActionScheduler *runtime_ = nullptr;
  TimeSource *timeSource_ = nullptr;
#if ENABLE_SLEEP_SCHEDULER
  SleepCoordinator *sleepCoordinator_ = nullptr;
#endif
};
