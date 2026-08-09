#pragma once

#include "EpaperShutdownCoordinator.h"

class ArduinoRestartDriver final : public RestartDriver {
 public:
  void restart() override;
};
