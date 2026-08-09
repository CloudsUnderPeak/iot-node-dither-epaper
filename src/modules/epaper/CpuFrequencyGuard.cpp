#include "CpuFrequencyGuard.h"

CpuFrequencyGuard::~CpuFrequencyGuard() {
  release();
}

bool CpuFrequencyGuard::acquire(CpuFrequencyDriver *driver,
                                uint32_t targetMhz) {
  if (active_ || driver == nullptr || targetMhz == 0) return false;

  const uint32_t original = driver->currentMhz();
  if (original == 0) return false;
  if (!driver->setMhz(targetMhz) || driver->currentMhz() != targetMhz) {
    if (driver->currentMhz() != original) driver->setMhz(original);
    return false;
  }

  driver_ = driver;
  originalMhz_ = original;
  targetMhz_ = targetMhz;
  active_ = true;
  return true;
}

bool CpuFrequencyGuard::release() {
  if (!active_) return true;
  CpuFrequencyDriver *driver = driver_;
  const uint32_t restoreMhz = originalMhz_;
  active_ = false;
  driver_ = nullptr;
  originalMhz_ = 0;
  targetMhz_ = 0;
  return driver->setMhz(restoreMhz) && driver->currentMhz() == restoreMhz;
}
