#include "EpaperCalibrationService.h"

const char *epaperCalibrationSourceToString(EpaperCalibrationSource source) {
  switch (source) {
    case EpaperCalibrationSource::Uninitialized: return "uninitialized";
    case EpaperCalibrationSource::Default: return "default";
    case EpaperCalibrationSource::Persisted: return "persisted";
    case EpaperCalibrationSource::RecoveryDefault: return "recovery_default";
  }
  return "uninitialized";
}

const char *epaperCalibrationRecoveryReasonToString(ResultCode reason) {
  switch (reason) {
    case ResultCode::Ok: return "none";
    case ResultCode::Unsupported: return "unsupported_schema";
    case ResultCode::StorageError: return "storage_error";
    case ResultCode::NotFound: return "not_found";
    default: return "invalid_persisted_calibration";
  }
}

EpaperCalibrationService::EpaperCalibrationService(
    EpaperCalibrationStore &store)
    : store_(store) {}

Result EpaperCalibrationService::begin() {
  if (mutex_ == nullptr) {
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) {
      return outOfSpace("failed to create e-paper calibration mutex");
    }
  }

  EpaperCalibration::Profile loaded;
  const Result loadResult = store_.load(loaded);
  state_.ready = true;
  if (loadResult.ok()) {
    state_.profile = loaded;
    state_.source = EpaperCalibrationSource::Persisted;
    state_.recoveryReason = ResultCode::Ok;
    return okResult();
  }

  state_.profile = EpaperCalibration::defaultProfile();
  if (loadResult.code == ResultCode::NotFound) {
    state_.source = EpaperCalibrationSource::Default;
    state_.recoveryReason = ResultCode::Ok;
  } else {
    state_.source = EpaperCalibrationSource::RecoveryDefault;
    state_.recoveryReason = loadResult.code;
  }
  return okResult();
}

EpaperCalibrationSnapshot EpaperCalibrationService::snapshot() const {
  EpaperCalibrationSnapshot copy;
  if (!lock()) return copy;
  copy = state_;
  unlock();
  return copy;
}

Result EpaperCalibrationService::update(
    const EpaperCalibration::Profile &profile,
    EpaperCalibrationSnapshot *updated) {
  const Result validation = EpaperCalibration::validate(profile);
  if (!validation.ok()) return validation;
  if (!lock()) return storageError("e-paper calibration service unavailable");
  if (state_.recoveryReason == ResultCode::Unsupported) {
    unlock();
    return unsupported("reset unsupported calibration schema before saving");
  }
  const Result saveResult = store_.save(profile);
  if (saveResult.ok()) {
    state_.ready = true;
    state_.profile = profile;
    state_.source = EpaperCalibrationSource::Persisted;
    state_.recoveryReason = ResultCode::Ok;
    if (updated != nullptr) *updated = state_;
  }
  unlock();
  return saveResult;
}

Result EpaperCalibrationService::reset(EpaperCalibrationSnapshot *updated) {
  if (!lock()) return storageError("e-paper calibration service unavailable");
  const Result resetResult = store_.reset();
  if (resetResult.ok()) {
    state_.ready = true;
    state_.profile = EpaperCalibration::defaultProfile();
    state_.source = EpaperCalibrationSource::Default;
    state_.recoveryReason = ResultCode::Ok;
    if (updated != nullptr) *updated = state_;
  }
  unlock();
  return resetResult;
}

bool EpaperCalibrationService::ready() const {
  return state_.ready;
}

bool EpaperCalibrationService::lock() const {
  return mutex_ != nullptr && xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE;
}

void EpaperCalibrationService::unlock() const {
  xSemaphoreGive(mutex_);
}
