#pragma once

#include "../../core/Result.h"
#include "PinRegistry.h"

namespace EpaperHardware {

constexpr const char *kOwner = "epaper";

// Claims all four device-specific pins atomically, then establishes the
// logical-quiesce boot levels. This does not claim protocol shutdown.
Result claimAndQuiescePins(PinRegistry *pins);

}  // namespace EpaperHardware
