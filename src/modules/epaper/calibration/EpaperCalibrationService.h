#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "EpaperCalibration.h"
#include "storage/EpaperCalibrationStore.h"

enum class EpaperCalibrationSource : uint8_t {
  Uninitialized,
  Default,
  Persisted,
  RecoveryDefault,
};

struct EpaperCalibrationSnapshot {
  bool ready = false;
  EpaperCalibrationSource source = EpaperCalibrationSource::Uninitialized;
  ResultCode recoveryReason = ResultCode::Ok;
  EpaperCalibration::Profile profile = EpaperCalibration::defaultProfile();
};

const char *epaperCalibrationSourceToString(EpaperCalibrationSource source);
const char *epaperCalibrationRecoveryReasonToString(ResultCode reason);

class EpaperCalibrationService {
 public:
  explicit EpaperCalibrationService(EpaperCalibrationStore &store);

  Result begin();
  EpaperCalibrationSnapshot snapshot() const;
  Result update(const EpaperCalibration::Profile &profile,
                EpaperCalibrationSnapshot *updated = nullptr);
  Result reset(EpaperCalibrationSnapshot *updated = nullptr);
  bool ready() const;

 private:
  EpaperCalibrationStore &store_;
  mutable SemaphoreHandle_t mutex_ = nullptr;
  EpaperCalibrationSnapshot state_;

  bool lock() const;
  void unlock() const;
};
