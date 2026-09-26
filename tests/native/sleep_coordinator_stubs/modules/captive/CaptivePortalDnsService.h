#pragma once
#include "core/Result.h"
#include "modules/wifi/WifiManager.h"
class CaptivePortalDnsService {
 public:
  bool runningValue = true;
  unsigned stopCalls = 0;
  unsigned restartCalls = 0;
  Result restart(const WifiStatus &) {
    ++restartCalls; runningValue = true; return okResult();
  }
  void stop() { ++stopCalls; runningValue = false; }
};
