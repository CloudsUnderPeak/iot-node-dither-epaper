#pragma once
#include "core/ProjectFeatures.h"

#ifndef SLEEP_IDLE_TIMEOUT_SECONDS
#define SLEEP_IDLE_TIMEOUT_SECONDS 600
#endif

#ifndef SLEEP_IGNORE_USB_HOST
#define SLEEP_IGNORE_USB_HOST 0
#endif

#if IOT_FEATURE_SLEEP != 0 && IOT_FEATURE_SLEEP != 1
#error "IOT_FEATURE_SLEEP must be 0 or 1"
#endif

#if SLEEP_IDLE_TIMEOUT_SECONDS < 1 || SLEEP_IDLE_TIMEOUT_SECONDS > 86400
#error "SLEEP_IDLE_TIMEOUT_SECONDS must be between 1 and 86400"
#endif

#if SLEEP_IGNORE_USB_HOST != 0 && SLEEP_IGNORE_USB_HOST != 1
#error "SLEEP_IGNORE_USB_HOST must be 0 or 1"
#endif
