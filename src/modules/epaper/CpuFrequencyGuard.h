#pragma once

#include <cstdint>

class CpuFrequencyDriver {
 public:
  virtual ~CpuFrequencyDriver() = default;
  virtual uint32_t currentMhz() const = 0;
  virtual bool setMhz(uint32_t mhz) = 0;
};

class CpuFrequencyGuard {
 public:
  static constexpr uint32_t kEpaperMhz = 80;

  ~CpuFrequencyGuard();

  bool acquire(CpuFrequencyDriver *driver,
               uint32_t targetMhz = kEpaperMhz);
  bool release();

  bool active() const { return active_; }
  uint32_t originalMhz() const { return originalMhz_; }
  uint32_t targetMhz() const { return targetMhz_; }

 private:
  CpuFrequencyDriver *driver_ = nullptr;
  uint32_t originalMhz_ = 0;
  uint32_t targetMhz_ = 0;
  bool active_ = false;
};
