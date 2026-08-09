#include "EpaperSafetyStore.h"

namespace {
bool decodeStage(uint8_t raw, EpaperProtectionStage &stage) {
  if (raw > static_cast<uint8_t>(EpaperProtectionStage::ShutdownConfirmed)) {
    return false;
  }
  stage = static_cast<EpaperProtectionStage>(raw);
  return true;
}
}  // namespace

bool EpaperSafetyStore::begin(EpaperSafetyStorage *storage) {
  ready_ = false;
  storage_ = nullptr;
  stage_ = EpaperProtectionStage::None;
  if (storage == nullptr || !storage->begin()) return false;

  uint8_t raw = 0;
  const EpaperSafetyReadStatus status = storage->readStage(raw);
  if (status == EpaperSafetyReadStatus::Missing) {
    storage_ = storage;
    ready_ = true;
    return true;
  }
  EpaperProtectionStage loaded;
  if (status != EpaperSafetyReadStatus::Ok || !decodeStage(raw, loaded) ||
      loaded == EpaperProtectionStage::None) {
    return false;
  }
  storage_ = storage;
  stage_ = loaded;
  ready_ = true;
  return true;
}

bool EpaperSafetyStore::writeAndVerify(EpaperProtectionStage stage) {
  if (!ready_ || storage_ == nullptr || stage == EpaperProtectionStage::None ||
      !storage_->writeStage(static_cast<uint8_t>(stage))) {
    return false;
  }
  uint8_t raw = 0;
  if (storage_->readStage(raw) != EpaperSafetyReadStatus::Ok ||
      raw != static_cast<uint8_t>(stage)) {
    return false;
  }
  stage_ = stage;
  return true;
}

bool EpaperSafetyStore::markActive() {
  return writeAndVerify(EpaperProtectionStage::Active);
}

bool EpaperSafetyStore::markShutdownConfirmed() {
  return writeAndVerify(EpaperProtectionStage::ShutdownConfirmed);
}

bool EpaperSafetyStore::clear() {
  if (!ready_ || storage_ == nullptr || !storage_->clearStage()) return false;
  uint8_t raw = 0;
  if (storage_->readStage(raw) != EpaperSafetyReadStatus::Missing) return false;
  stage_ = EpaperProtectionStage::None;
  return true;
}

const char *epaperProtectionStageToString(EpaperProtectionStage stage) {
  switch (stage) {
    case EpaperProtectionStage::None:
      return "none";
    case EpaperProtectionStage::Active:
      return "active";
    case EpaperProtectionStage::ShutdownConfirmed:
      return "shutdown_confirmed";
  }
  return "unknown";
}
