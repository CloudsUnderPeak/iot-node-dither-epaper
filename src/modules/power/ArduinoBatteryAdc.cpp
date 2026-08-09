#include "ArduinoBatteryAdc.h"

#include <Arduino.h>

bool ArduinoBatteryAdc::begin(uint8_t pin) {
  analogReadResolution(12);
  analogSetPinAttenuation(pin, ADC_11db);
  return true;
}

bool ArduinoBatteryAdc::readMilliVolts(uint8_t pin, uint32_t &milliVolts) {
  milliVolts = analogReadMilliVolts(pin);
  return true;
}
