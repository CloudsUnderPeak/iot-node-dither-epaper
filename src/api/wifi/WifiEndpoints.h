#pragma once

#include "api/shared/ApiTypes.h"
#include "modules/config/ConfigService.h"
#include "modules/runtime/RuntimeActionScheduler.h"
#include "modules/wifi/WifiManager.h"
#include "modules/wifi/WifiScanner.h"

// Handles /api/wifi and its scan, connect, update, and reconnect operations.
class WifiEndpoints {
 public:
  Result begin(ConfigService *configService,
               WifiManager *wifiManager,
               WifiScanner *wifiScanner,
               RuntimeActionScheduler *runtime);
  Api::Response get() const;
  Api::Response scan();
  Api::Response update(const Api::Request &request);
  Api::Response connect(const Api::Request &request);
  Api::Response connectionStatus() const;
  Api::Response reconnect();

 private:
  ConfigService *configService_ = nullptr;
  WifiManager *wifiManager_ = nullptr;
  WifiScanner *wifiScanner_ = nullptr;
  RuntimeActionScheduler *runtime_ = nullptr;

  bool decodeCandidate(const Api::Request &request,
                       const DeviceConfig &current,
                       DeviceConfig &candidate,
                       Api::Response &errorResponse) const;
};
