#include <cassert>
#include <cstdio>
#include <initializer_list>
#include <esp32-hal-ledc.h>
#include "modules/status_led/ArduinoStatusLedDriver.h"
#include "modules/hardware/StatusLedHardware.h"

namespace {
struct Hardware {
  bool attached = false, gpioOff = true, held = false;
  bool attachFails = false, writeFails = false, detachFails = false;
  bool wrongDuty = false, wrongFrequency = false;
  uint32_t duty = 0, pendingDuty = 0, frequency = 0;
  unsigned attaches = 0, detaches = 0;
  ledc_clk_cfg_t clock = LEDC_AUTO_CLK;
} hw;
}
ledc_clk_cfg_t ledcGetClockSource() { return hw.clock; }
bool ledcSetClockSource(ledc_clk_cfg_t clock) { hw.clock = clock; return true; }
bool ledcAttach(uint8_t pin, uint32_t frequency, uint8_t bits) {
  assert(pin == 15 && frequency == 1000 && bits == 10);
  ++hw.attaches;
  if (hw.attachFails || hw.attached || hw.held) return false;
  hw.attached = true; hw.frequency = frequency; return true;
}
bool ledcWrite(uint8_t, uint32_t duty) {
  if (!hw.attached || hw.writeFails) return false;
  hw.pendingDuty = duty; hw.gpioOff = false; return true;
}
void delay(uint32_t milliseconds) {
  assert(milliseconds == 2); hw.duty = hw.pendingDuty;
}
bool ledcDetach(uint8_t) {
  ++hw.detaches;
  if (hw.detachFails) return false;
  hw.attached = false; return true;
}
uint32_t ledcRead(uint8_t) { return hw.duty + (hw.wrongDuty ? 1 : 0); }
uint32_t ledcReadFreq(uint8_t) { return hw.frequency + (hw.wrongFrequency ? 1 : 0); }
namespace StatusLedHardware {
bool bootOff() { hw.gpioOff = true; hw.held = false; return true; }
bool write(bool on) { hw.gpioOff = !on; return true; }
bool holdOff() { assert(hw.gpioOff && !hw.attached); hw.held = true; return true; }
bool releaseHold() { hw.held = false; return true; }
int level() { return hw.gpioOff ? 0 : 1; }
}

int main() {
  ArduinoStatusLedDriver driver;
  StatusLed led; led.begin(driver);
  assert(led.setNormal(true));
  assert(driver.pwmAttached() && driver.duty() == 102 && driver.frequencyHz() == 1000);
  assert(hw.clock == LEDC_USE_XTAL_CLK);
  const auto attaches = hw.attaches;
  assert(led.setNormal(true) && hw.attaches == attaches);
  assert(led.prepareSleep());
  assert(!driver.pwmAttached() && driver.duty() == 0 && hw.gpioOff && hw.held);
  assert(!led.setNormal(true));
  assert(led.cancelSleep() && led.setNormal(true));
  assert(led.setNormal(false));

  // Failed setup/read-back must leave GPIO off and permit a clean retry.
  for (bool *fault : {&hw.attachFails, &hw.writeFails, &hw.wrongDuty, &hw.wrongFrequency}) {
    *fault = true;
    assert(!led.setNormal(true));
    assert(!led.on() && !driver.pwmAttached() && hw.gpioOff);
    *fault = false;
    assert(led.setNormal(true) && led.setNormal(false));
  }
  // A failed detach must block sleep, even though GPIO off is forced.
  assert(led.setNormal(true)); hw.detachFails = true;
  assert(!led.prepareSleep() && hw.gpioOff && driver.pwmAttached() && !hw.held);
  hw.detachFails = false;
  assert(led.prepareSleep() && !driver.pwmAttached() && hw.held);
  assert(led.cancelSleep());

  // Setup failure plus detach failure leaves a retained owner. Retry must
  // resolve it and reattach the PWM route instead of trusting stale duty.
  hw.wrongDuty = true; hw.detachFails = true;
  assert(!led.setNormal(true) && driver.pwmAttached() && hw.gpioOff);
  hw.wrongDuty = false;
  assert(!led.setNormal(true) && hw.gpioOff);
  hw.detachFails = false;
  assert(led.setNormal(true) && !hw.gpioOff && driver.duty() == 102);
  assert(led.prepareSleep());
  std::puts("Status LED PWM tests passed");
}
