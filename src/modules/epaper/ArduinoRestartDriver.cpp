#include "ArduinoRestartDriver.h"

#include <Arduino.h>

void ArduinoRestartDriver::restart() {
  ESP.restart();
}
