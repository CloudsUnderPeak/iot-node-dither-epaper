#pragma once

#include "EpaperCalibrationStore.h"
#include "modules/config/storage/PreferencesBackend.h"

class PreferencesEpaperCalibrationStore final : public EpaperCalibrationStore {
 public:
  explicit PreferencesEpaperCalibrationStore(PreferencesBackend &backend);

  Result load(EpaperCalibration::Profile &profile) override;
  Result save(const EpaperCalibration::Profile &profile) override;
  Result reset() override;

 private:
  PreferencesBackend &backend_;
};
