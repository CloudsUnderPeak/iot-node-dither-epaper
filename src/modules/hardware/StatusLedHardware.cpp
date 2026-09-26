#include "StatusLedHardware.h"

#include <driver/gpio.h>
#include "board/BoardProfile.h"

namespace StatusLedHardware {
namespace {
constexpr auto kPin = static_cast<gpio_num_t>(Board::ActiveProfile::kStatusLedPin);
constexpr bool kActiveHigh = Board::ActiveProfile::kStatusLedActiveHigh;
}

bool bootOff() {
  // Board-owned strapping pin: never relax generic PinRegistry restrictions.
  // INPUT_OUTPUT enables electrical read-back as well as driving the LED.
  gpio_config_t config{};
  config.pin_bit_mask = 1ULL << kPin;
  config.mode = GPIO_MODE_INPUT_OUTPUT;
  config.pull_up_en = GPIO_PULLUP_DISABLE;
  config.pull_down_en = GPIO_PULLDOWN_DISABLE;
  config.intr_type = GPIO_INTR_DISABLE;
  if (gpio_set_level(kPin, !kActiveHigh) != ESP_OK ||
      gpio_config(&config) != ESP_OK) return false;
  return gpio_hold_dis(kPin) == ESP_OK && level() == int(!kActiveHigh);
}

bool write(bool on) {
  const int wanted = on == kActiveHigh;
  return gpio_set_level(kPin, wanted) == ESP_OK && level() == wanted;
}

bool holdOff() {
  return write(false) && gpio_hold_en(kPin) == ESP_OK;
}

bool releaseHold() { return gpio_hold_dis(kPin) == ESP_OK; }
int level() { return gpio_get_level(kPin); }
}  // namespace StatusLedHardware
