#pragma once

#include "modules/epaper/calibration/EpaperCalibration.h"

class EpaperCalibrationStore {
 public:
  virtual ~EpaperCalibrationStore() = default;
  virtual Result load(EpaperCalibration::Profile &profile) = 0;
  virtual Result save(const EpaperCalibration::Profile &profile) = 0;
  virtual Result reset() = 0;
};
