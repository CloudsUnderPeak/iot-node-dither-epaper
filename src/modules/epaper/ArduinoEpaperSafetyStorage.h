#pragma once

#include <Preferences.h>

#include "EpaperSafetyStore.h"

class ArduinoEpaperSafetyStorage final : public EpaperSafetyStorage {
 public:
  ~ArduinoEpaperSafetyStorage() override;

  bool begin() override;
  EpaperSafetyReadStatus readStage(uint8_t &stage) const override;
  bool writeStage(uint8_t stage) override;
  bool clearStage() override;

 private:
  mutable Preferences preferences_;
  bool open_ = false;
};
