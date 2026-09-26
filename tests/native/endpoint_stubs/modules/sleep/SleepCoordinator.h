#pragma once
#include <cstdint>
#include "modules/sleep/SleepStore.h"
#include "modules/sleep/WakeClassifier.h"
#include "modules/time/TimeSource.h"
enum class SleepRunState : uint8_t { Disabled, Armed, AgendaRunning, Entering, Failed };
enum class SleepRequestState : uint8_t { None, Pending, Entering, Cancelled, Failed };
enum SleepBlocker : uint16_t {
  SleepBlockerEpaperBusy = 1U << 0,
  SleepBlockerEpaperUnsafe = 1U << 1,
  SleepBlockerMarkerActive = 1U << 2,
  SleepBlockerUpload = 1U << 3,
  SleepBlockerRestart = 1U << 4,
  SleepBlockerWifi = 1U << 5,
  SleepBlockerRuntime = 1U << 6,
  SleepBlockerNoWake = 1U << 7,
  SleepBlockerUsb = 1U << 8,
  SleepBlockerStorage = 1U << 9,
};
struct SleepSnapshot {
  bool available = true;
  SleepRecord record;
  SleepStoreState storage = SleepStoreState::Ready;
  WakeMode mode = WakeMode::Normal;
  SleepRunState state = SleepRunState::Armed;
  SleepRequestState request = SleepRequestState::None;
  const char *requestError = nullptr;
  bool idleArmed = true;
  uint32_t idleRemainingSeconds = 1200;
  SleepClockBasis basis = SleepClockBasis::Absolute;
  int64_t nextWakeEpoch = 1800000000;
  uint32_t nextWakeInSeconds = 3600;
  TimeSnapshot time{1799996400, TimeOrigin::Client, 1};
  uint16_t blockers = 0;
};
class SleepCoordinator {
 public:
  SleepSnapshot current;
  unsigned activityCount = 0;
  unsigned activeRequests = 0;
  bool admissionClosed = false;
  bool updateValue = true;
  bool timeUpdateValue = true;
  bool keepAwakeValue = true;
  unsigned updateCount = 0;
  SleepCoordinator() {
    current.record.enabled = true;
    current.record.periodHours = 24;
    current.record.anchorEpoch = 1800000000;
    current.record.scheduleGeneration = 1;
  }
  SleepSnapshot snapshot(uint32_t) { return current; }
  bool keepAwake(uint32_t) {
    return keepAwakeValue && !admissionClosed;
  }
  bool requestNow(uint32_t, uint16_t &blocked) {
    blocked = current.blockers;
    if (blocked || admissionClosed) return false;
    current.request = SleepRequestState::Pending;
    return true;
  }
  bool update(const SleepRecord &candidate, SleepRecord &committed) {
    ++updateCount;
    if (admissionClosed || !updateValue) return false;
    current.record = candidate;
    committed = candidate;
    return true;
  }
  bool beginApiRequest(bool activity, uint32_t) {
    if (admissionClosed) return false;
    ++activeRequests;
    if (activity) ++activityCount;
    return true;
  }
  void endApiRequest() { if (activeRequests) --activeRequests; }
  bool beginTimeUpdate() {
    return timeUpdateValue && !admissionClosed;
  }
  void endTimeUpdate() {}
  bool entering() const { return admissionClosed; }
};
