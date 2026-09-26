#pragma once

#include <cstdint>

#include "modules/config/storage/PreferencesBackend.h"

enum class SleepStoreState : uint8_t { Empty, Ready, Recovery, Error };

struct SleepTaskRecord {
  uint8_t name = 0;
  uint8_t status = 0;
  uint8_t code = 0;
};

struct SleepLastWake {
  uint8_t cause = 0;
  uint8_t mode = 0;
  uint8_t basis = 0;
  int64_t epoch = INT64_MIN;
  int64_t plannedDue = INT64_MIN;
  int32_t driftSeconds = INT32_MIN;
  uint8_t result = 0;
  uint16_t consecutiveFailures = 0;
  uint8_t staAttempts = 0;
  bool timeSynced = false;
  char lastErrorCode[32]{};
  uint8_t taskCount = 0;
  SleepTaskRecord tasks[4]{};
};

struct SleepRecord {
  uint64_t revision = 0;
  bool enabled = false;
  uint8_t periodHours = 24;
  int64_t anchorEpoch = 0;
  uint64_t scheduleGeneration = 0;
  int64_t lastHandledSlot = -1;
  SleepLastWake lastWake;
};

class SleepStore {
 public:
  explicit SleepStore(PreferencesBackend &backend) : backend_(backend) {}
  SleepStoreState load(SleepRecord &record);
  SleepStoreState save(const SleepRecord &candidate, SleepRecord &committed);
  SleepStoreState state() const { return state_; }

 private:
  PreferencesBackend &backend_;
  SleepStoreState state_ = SleepStoreState::Empty;
  uint8_t activeSlot_ = 0xff;
  SleepRecord active_;
};
