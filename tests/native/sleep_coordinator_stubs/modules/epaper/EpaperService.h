#pragma once
#include <cstdint>
#include "core/Result.h"
enum class EpaperServiceState : uint8_t { Idle, Uploading, Queued, Drawing, Cooldown, Unavailable };
enum class EpaperPanelState : uint8_t { Inactive, Active, Sleeping, Unknown };
enum class EpaperDrawAction : uint8_t { Stored };
struct EpaperStoredStub { bool present = true; bool valid = true; };
struct EpaperServiceSnapshot {
  EpaperServiceState state = EpaperServiceState::Idle;
  EpaperPanelState panelState = EpaperPanelState::Sleeping;
  bool recoveryRequired = false;
  bool canDraw = true;
  EpaperStoredStub stored;
  uint32_t completedOperationId = 0;
  const char *lastResult = "success";
};
struct EpaperServiceResult {
  Result result = okResult();
  uint32_t operationId = 0;
  bool ok() const { return result.ok(); }
};
class EpaperService {
 public:
  EpaperServiceSnapshot current;
  bool readyValue = true;
  bool requestSleepValue = true;
  bool sleepReadyValue = true;
  unsigned requestSleepCalls = 0;
  unsigned cancelSleepCalls = 0;
  uint32_t nextOperation = 1;
  bool ready() const { return readyValue; }
  EpaperServiceSnapshot snapshot(uint32_t) const { return current; }
  EpaperServiceResult requestDraw(EpaperDrawAction) {
    return {okResult(), nextOperation++};
  }
  bool requestSleep(uint32_t) { ++requestSleepCalls; return requestSleepValue; }
  bool sleepReady() const { return sleepReadyValue; }
  void cancelSleep() { ++cancelSleepCalls; }
};
