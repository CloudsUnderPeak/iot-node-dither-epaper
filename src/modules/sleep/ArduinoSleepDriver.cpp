#include "SleepFeatures.h"
#if ENABLE_SLEEP_SCHEDULER
#include "ArduinoSleepDriver.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/usb_serial_jtag.h>
#include <esp_sleep.h>
#include <soc/soc_caps.h>

#include "board/BoardProfile.h"

namespace {
RTC_DATA_ATTR SleepRtcRecord retainedRecord;

constexpr Board::EpaperPins kPins = Board::ActiveProfile::kEpaper;

bool releaseOne(int pin) {
  return gpio_hold_dis(static_cast<gpio_num_t>(pin)) == ESP_OK;
}
}  // namespace

bool ArduinoSleepDriver::usbHostConnected() const {
  return usb_serial_jtag_is_connected();
}

WakeCause ArduinoSleepDriver::wakeupCause() const {
  switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_TIMER: return WakeCause::Timer;
    case ESP_SLEEP_WAKEUP_EXT1: return WakeCause::Gpio;
    case ESP_SLEEP_WAKEUP_UNDEFINED: return WakeCause::None;
    default: return WakeCause::Unknown;
  }
}

bool ArduinoSleepDriver::armTimer(uint32_t seconds) {
  return seconds >= 5 &&
         esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(seconds) * 1000000ULL) == ESP_OK;
}

bool ArduinoSleepDriver::disarmTimer() {
  return esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER) == ESP_OK;
}

bool ArduinoSleepDriver::holdEpaperPins() {
  if (gpio_set_level(static_cast<gpio_num_t>(kPins.reset), 1) != ESP_OK ||
      gpio_set_level(static_cast<gpio_num_t>(kPins.cs), 1) != ESP_OK ||
      gpio_set_level(static_cast<gpio_num_t>(kPins.dc), 0) != ESP_OK) return false;
  const bool held = gpio_hold_en(static_cast<gpio_num_t>(kPins.reset)) == ESP_OK &&
                    gpio_hold_en(static_cast<gpio_num_t>(kPins.cs)) == ESP_OK &&
                    gpio_hold_en(static_cast<gpio_num_t>(kPins.dc)) == ESP_OK;
  if (!held) {
    releaseEpaperPins();
    return false;
  }
#if !SOC_GPIO_SUPPORT_HOLD_SINGLE_IO_IN_DSLP
  gpio_deep_sleep_hold_en();
#endif
  return true;
}

bool ArduinoSleepDriver::releaseEpaperPins() {
#if !SOC_GPIO_SUPPORT_HOLD_SINGLE_IO_IN_DSLP
  gpio_deep_sleep_hold_dis();
#endif
  const bool resetReleased = releaseOne(kPins.reset);
  const bool csReleased = releaseOne(kPins.cs);
  const bool dcReleased = releaseOne(kPins.dc);
  return resetReleased && csReleased && dcReleased;
}

SleepRtcRecord ArduinoSleepDriver::bootRecord() const {
  return retainedRecord;
}

void ArduinoSleepDriver::writeRecord(const SleepRtcRecord &record) {
  retainedRecord = record;
  sealSleepRtcRecord(retainedRecord);
}

void ArduinoSleepDriver::invalidateRecord() {
  retainedRecord.intent = false;
}

void ArduinoSleepDriver::drainSerial(uint32_t timeoutMs) {
  if (!Serial) return;
  const uint32_t started = millis();
  while (Serial.availableForWrite() < 2048 &&
         static_cast<uint32_t>(millis() - started) < timeoutMs) {
    delay(1);
  }
}

void ArduinoSleepDriver::deepSleep() {
  esp_deep_sleep_start();
}
#endif
