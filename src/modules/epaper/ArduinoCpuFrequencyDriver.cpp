#include "ArduinoCpuFrequencyDriver.h"

#include <esp32-hal-cpu.h>

uint32_t ArduinoCpuFrequencyDriver::currentMhz() const {
  return getCpuFrequencyMhz();
}

bool ArduinoCpuFrequencyDriver::setMhz(uint32_t mhz) {
  return setCpuFrequencyMhz(mhz);
}
