#pragma once

#include <Arduino.h>

#include "../../core/Result.h"
#include "api/ApiRouter.h"
#include "../auth/AuthService.h"
#include "../config/ConfigService.h"
#include "../storage/FlashStorage.h"
#include "../storage/UserDataStorage.h"
#include "../wifi/WifiManager.h"
#include "../wifi/WifiScanner.h"
#include "ConsoleConfigCommand.h"
#include "modules/sleep/SleepFeatures.h"
#if IOT_FEATURE_SLEEP
class SleepCoordinator;
#endif

class ConsoleShell {
 public:
  static constexpr size_t kInputCapacity = 640;

  Result begin(ConfigService *configService,
               WifiManager *wifiManager,
               WifiScanner *wifiScanner,
               const FlashStorage *flashStorage,
               UserDataStorage *userData,
               AuthService *authService,
               ApiRouter *router);
  void poll();
#if IOT_FEATURE_SLEEP
  void setSleepCoordinator(SleepCoordinator *sleep) { sleep_ = sleep; }
#endif

 private:
  ConfigService *configService_ = nullptr;
  WifiManager *wifiManager_ = nullptr;
  WifiScanner *wifiScanner_ = nullptr;
  const FlashStorage *flashStorage_ = nullptr;
  UserDataStorage *userData_ = nullptr;
  AuthService *authService_ = nullptr;
  ApiRouter *router_ = nullptr;
#if IOT_FEATURE_SLEEP
  SleepCoordinator *sleep_ = nullptr;
#endif
  ConsoleConfigCommand configCommand_;
  char line_[kInputCapacity] = "";
  size_t lineLength_ = 0;
  bool discardingLine_ = false;
  bool discardedApiLine_ = false;
  Api::PendingRequest pendingApi_;
  uint32_t pendingHumanScan_ = 0;
  void clearLine();
  void handleLine(char *line);
  void handleHumanCommand(char *command);
  void handleApiCommand(char *command);
  void handleListCommand(char *arguments);
  void handleStatCommand(char *arguments);
  void printFilesystemError(const UserDataFileResult &result) const;
  void printHelp() const;
  void printStatus() const;
  void printDevice() const;
  void printWifi() const;
  void printScan();
  void printScanResult(const WifiScanResult &result);
  void printStorage() const;
  void printSession() const;
  void printApiResponse(const Api::Response &response) const;
};
