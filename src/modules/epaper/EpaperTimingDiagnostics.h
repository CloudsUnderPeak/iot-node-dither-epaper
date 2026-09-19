#pragma once
#include <cstdint>

// Bounded cached counters, printed only by the existing backpressure-safe
// heartbeat. Wall-clock milliseconds; zero can mean below timer resolution.
struct EpaperTimingDiagnostics {
  uint32_t uploadValidationMs = 0;
  uint32_t storedValidationMs = 0;
  uint32_t frameReadMs = 0;
  uint32_t transferMs = 0;
  uint32_t totalOperationMs = 0;
  uint32_t downloadFailures = 0;
};
