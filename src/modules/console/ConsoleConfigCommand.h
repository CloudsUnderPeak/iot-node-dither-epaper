#pragma once

#include <Arduino.h>

#include "ConfigStaging.h"
#include "api/ApiRouter.h"
#include "modules/config/ConfigService.h"

class ConsoleConfigCommand {
 public:
  Result begin(ConfigService *configService, ApiRouter *router);
  void handle(char *arguments);

 private:
  ConfigService *configService_ = nullptr;
  ApiRouter *router_ = nullptr;
  ConfigStaging staging_;

  void show(char *arguments);
  void get(char *arguments);
  void set(char *arguments);
  void changes(char *arguments);
  void revert(char *arguments);
  void status(char *arguments);
  void commit(char *arguments);
  void printKeyValue(ConsoleConfigKey key, const DeviceConfig &config) const;
  String valueLiteral(ConsoleConfigKey key, const DeviceConfig &config) const;
  JsonDocument buildPayload(ConsoleConfigGroup group,
                            const DeviceConfig &config) const;
  void printRouterError(const Api::Response &response) const;
};
