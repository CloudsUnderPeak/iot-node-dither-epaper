#pragma once
#include "IPAddress.h"
#include "core/Result.h"
#include "modules/config/model/DeviceConfig.h"
enum class WifiLinkState : uint8_t { Disabled, Connecting, Connected, Failed };
struct WifiStatus {
  WifiMode mode = WifiMode::Off;
  bool staEnabled = false;
  bool apEnabled = false;
  WifiLinkState staState = WifiLinkState::Disabled;
  IPAddress staIp;
};
class WifiManager {
 public:
  WifiStatus current;
  bool transition = false;
  bool scan = false;
  bool applyValue = true;
  unsigned applyCalls = 0;
  unsigned powerSaveCalls = 0;
  WifiStatus status() const { return current; }
  Result apply(const DeviceConfig &config, WifiStatus &out) {
    ++applyCalls;
    current.mode = config.wifiMode;
    out = current;
    return applyValue ? okResult() : networkError("apply failed");
  }
  Result applyPowerSave(bool) { ++powerSaveCalls; return okResult(); }
  bool testBlocksScan() const { return transition; }
  bool scanBlocksRadio() const { return scan; }
};
