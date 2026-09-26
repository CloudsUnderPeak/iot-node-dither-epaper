#pragma once

// Production values come from the build-config resolver. Defaults also allow
// native service tests to build the complete product without PlatformIO.
#ifndef IOT_FEATURE_SLEEP
#define IOT_FEATURE_SLEEP 1
#endif
#ifndef IOT_FEATURE_EPAPER
#define IOT_FEATURE_EPAPER 1
#endif
#ifndef IOT_FEATURE_STORAGE
#define IOT_FEATURE_STORAGE 1
#endif
#ifndef IOT_FEATURE_AUTH
#define IOT_FEATURE_AUTH 1
#endif
#ifndef IOT_FEATURE_USER_FILES
#define IOT_FEATURE_USER_FILES 1
#endif
#ifndef IOT_FEATURE_MDNS
#define IOT_FEATURE_MDNS 1
#endif
#ifndef IOT_FEATURE_BATTERY
#define IOT_FEATURE_BATTERY 1
#endif
#ifndef IOT_FEATURE_CONSOLE
#define IOT_FEATURE_CONSOLE 1
#endif
#if IOT_FEATURE_EPAPER && !IOT_FEATURE_STORAGE
#error "EPAPER requires STORAGE=1"
#endif
#if IOT_FEATURE_USER_FILES && !IOT_FEATURE_STORAGE
#error "USER_FILES requires STORAGE=1"
#endif
