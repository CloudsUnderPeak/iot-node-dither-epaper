#pragma once

#include "modules/sleep/SleepFeatures.h"

#if IOT_FEATURE_SLEEP
#include "api/shared/ApiTypes.h"
#include "modules/sleep/SleepCoordinator.h"

class SleepEndpoints {
 public:
  static Api::Response get(SleepCoordinator &sleep);
  static Api::Response update(const Api::Request &request,
                              SleepCoordinator &sleep, TimeSource &time);
  static Api::Response keepAwake(const Api::Request &request,
                                 SleepCoordinator &sleep);
  static Api::Response now(const Api::Request &request,
                           SleepCoordinator &sleep);
};
#endif
