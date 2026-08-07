#pragma once

#include "../modules/config/model/DeviceConfig.h"
#include "../modules/wifi/WifiManager.h"

#ifndef ENABLE_CONFIG_STORE_SELF_TEST
#define ENABLE_CONFIG_STORE_SELF_TEST 0
#endif

#ifndef ENABLE_WIFI_MODE_SELF_TEST
#define ENABLE_WIFI_MODE_SELF_TEST 0
#endif

namespace FirmwareSelfTest {

#if ENABLE_CONFIG_STORE_SELF_TEST
// Validates Preferences-backed config persistence, reload, and bad enum rejection.
bool runConfigStore(DeviceConfig &activeConfig);
#endif

#if ENABLE_WIFI_MODE_SELF_TEST
// Validates that persisted Wi-Fi settings can drive OFF, STA, AP, and AP+STA.
bool runWifiModes(DeviceConfig &activeConfig, WifiManager &wifiManager, WifiStatus &wifiStatus);
#endif

}  // namespace FirmwareSelfTest
