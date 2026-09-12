#pragma once

#include <cstdint>

enum class EpaperProtectionStage : uint8_t {
  None = 0,
  Active = 1,
  ShutdownConfirmed = 2,
};

enum class EpaperSafetyReadStatus : uint8_t {
  Ok,
  Missing,
  StorageError,
  InvalidValue,
};

class EpaperSafetyStorage {
 public:
  virtual ~EpaperSafetyStorage() = default;
  virtual bool begin() = 0;
  virtual EpaperSafetyReadStatus readStage(uint8_t &stage) const = 0;
  virtual bool writeStage(uint8_t stage) = 0;
  virtual bool clearStage() = 0;
};

class EpaperSafetyStore {
 public:
  bool begin(EpaperSafetyStorage *storage);
  bool markActive();
  bool markShutdownConfirmed();
  bool clear();
  bool recoverActiveAfterConfirmedPowerCycle();

  bool ready() const { return ready_; }
  EpaperProtectionStage stage() const { return stage_; }

 private:
  EpaperSafetyStorage *storage_ = nullptr;
  EpaperProtectionStage stage_ = EpaperProtectionStage::None;
  bool ready_ = false;

  bool writeAndVerify(EpaperProtectionStage stage);
};

const char *epaperProtectionStageToString(EpaperProtectionStage stage);
