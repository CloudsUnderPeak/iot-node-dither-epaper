#include "ArduinoEpaperSafetyStorage.h"

namespace {
constexpr const char *kNamespace = "epaper_meta";
constexpr const char *kStageKey = "stage";
}

ArduinoEpaperSafetyStorage::~ArduinoEpaperSafetyStorage() {
  if (open_) preferences_.end();
}

bool ArduinoEpaperSafetyStorage::begin() {
  if (open_) return true;
  open_ = preferences_.begin(kNamespace, false);
  return open_;
}

EpaperSafetyReadStatus ArduinoEpaperSafetyStorage::readStage(
    uint8_t &stage) const {
  if (!open_) return EpaperSafetyReadStatus::StorageError;
  if (!preferences_.isKey(kStageKey)) return EpaperSafetyReadStatus::Missing;
  stage = preferences_.getUChar(kStageKey, 0xFF);
  if (stage < static_cast<uint8_t>(EpaperProtectionStage::Active) ||
      stage > static_cast<uint8_t>(EpaperProtectionStage::ShutdownConfirmed)) {
    return EpaperSafetyReadStatus::InvalidValue;
  }
  return EpaperSafetyReadStatus::Ok;
}

bool ArduinoEpaperSafetyStorage::writeStage(uint8_t stage) {
  return open_ && preferences_.putUChar(kStageKey, stage) == sizeof(stage);
}

bool ArduinoEpaperSafetyStorage::clearStage() {
  return open_ && (!preferences_.isKey(kStageKey) ||
                   preferences_.remove(kStageKey));
}
