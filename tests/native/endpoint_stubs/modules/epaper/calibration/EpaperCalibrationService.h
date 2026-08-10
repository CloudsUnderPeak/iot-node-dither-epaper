#pragma once

#include "modules/epaper/calibration/EpaperCalibration.h"

enum class EpaperCalibrationSource : uint8_t {
  Uninitialized,
  Default,
  Persisted,
  RecoveryDefault,
};

struct EpaperCalibrationSnapshot {
  bool ready = true;
  EpaperCalibrationSource source = EpaperCalibrationSource::Default;
  ResultCode recoveryReason = ResultCode::Ok;
  EpaperCalibration::Profile profile = EpaperCalibration::defaultProfile();
};

inline const char *epaperCalibrationSourceToString(
    EpaperCalibrationSource source) {
  switch (source) {
    case EpaperCalibrationSource::Uninitialized: return "uninitialized";
    case EpaperCalibrationSource::Default: return "default";
    case EpaperCalibrationSource::Persisted: return "persisted";
    case EpaperCalibrationSource::RecoveryDefault: return "recovery_default";
  }
  return "uninitialized";
}

inline const char *epaperCalibrationRecoveryReasonToString(ResultCode reason) {
  switch (reason) {
    case ResultCode::Ok: return "none";
    case ResultCode::Unsupported: return "unsupported_schema";
    case ResultCode::StorageError: return "storage_error";
    case ResultCode::NotFound: return "not_found";
    default: return "invalid_persisted_calibration";
  }
}

class EpaperCalibrationService {
 public:
  EpaperCalibrationSnapshot current;
  Result updateResult = okResult();
  Result resetResult = okResult();

  EpaperCalibrationSnapshot snapshot() const { return current; }

  Result update(const EpaperCalibration::Profile &profile,
                EpaperCalibrationSnapshot *updated = nullptr) {
    if (!updateResult.ok()) return updateResult;
    const Result validation = EpaperCalibration::validate(profile);
    if (!validation.ok()) return validation;
    current.ready = true;
    current.profile = profile;
    current.source = EpaperCalibrationSource::Persisted;
    current.recoveryReason = ResultCode::Ok;
    if (updated != nullptr) *updated = current;
    return okResult();
  }

  Result reset(EpaperCalibrationSnapshot *updated = nullptr) {
    if (!resetResult.ok()) return resetResult;
    current.ready = true;
    current.profile = EpaperCalibration::defaultProfile();
    current.source = EpaperCalibrationSource::Default;
    current.recoveryReason = ResultCode::Ok;
    if (updated != nullptr) *updated = current;
    return okResult();
  }

  bool ready() const { return current.ready; }
};
