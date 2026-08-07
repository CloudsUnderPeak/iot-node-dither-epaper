# Smoke Test Code

Use before generating the first firmware used to prove build, upload, boot, and serial output.

Do not generate first firmware until the framework choice is confirmed, the board/chip is identified, and existing firmware entry files have been inspected.

## File Placement

Create a firmware entry point if the project does not already have one.

For PlatformIO + Arduino, create:

```text
src/main.cpp
```

For Arduino IDE style projects, create or update the project `.ino` file only after confirming that is the intended layout.

For ESP-IDF, create:

```text
main/main.c
```

Also create minimal `CMakeLists.txt` files if the ESP-IDF project does not already have them.

Do not overwrite existing app code blindly. Read the current entry file first, preserve user work, and only replace it when the user explicitly asks for a fresh smoke test.

## Smoke Test Responsibilities

The first `main` should do only enough to prove the board is alive and the toolchain is correct:

1. Initialize serial/log output at `115200`.
2. Wait briefly so USB serial monitors can attach.
3. Print a clear project banner.
4. Print chip model, revision, core count, flash size, and free heap.
5. Optionally configure and blink a status LED only after the LED pin is verified.
6. Print a monotonic `alive tick` line once per second.
7. Keep running forever so monitor logs can prove stability.

Do not add Wi-Fi, BLE, sensors, displays, filesystems, deep sleep, interrupts, or multitasking to the first smoke test. Add those later with `esp32-develop` after bring-up succeeds.

If LED pin or polarity is not verified, keep `STATUS_LED_PIN=-1` or omit LED code entirely.

## Arduino Smoke Test Pattern

```cpp
#include <Arduino.h>

#ifndef STATUS_LED_PIN
#define STATUS_LED_PIN -1
#endif

static uint32_t tick = 0;

void setup() {
#if STATUS_LED_PIN >= 0
  pinMode(STATUS_LED_PIN, OUTPUT);
#endif
  Serial.begin(115200);
  delay(500);
  Serial.println("esp32-ai-bringup: Arduino smoke test");
  Serial.printf("Chip model: %s, revision: %u, cores: %u\n",
                ESP.getChipModel(),
                ESP.getChipRevision(),
                ESP.getChipCores());
  Serial.printf("Flash: %u MB\n", ESP.getFlashChipSize() / (1024 * 1024));
  Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
}

void loop() {
#if STATUS_LED_PIN >= 0
  digitalWrite(STATUS_LED_PIN, tick & 1);
#endif
  Serial.printf("alive tick=%u, free_heap=%u\n", tick++, ESP.getFreeHeap());
  delay(1000);
}
```

## ESP-IDF Smoke Test Pattern

Use this only when ESP-IDF is selected:

```c
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "esp32-ai-bringup";

void app_main(void) {
    esp_chip_info_t chip_info;
    uint32_t flash_size = 0;
    uint32_t tick = 0;

    esp_chip_info(&chip_info);
    esp_flash_get_size(NULL, &flash_size);

    ESP_LOGI(TAG, "App started");
    ESP_LOGI(TAG, "Chip cores: %d", chip_info.cores);
    ESP_LOGI(TAG, "Chip revision: %d", chip_info.revision);
    ESP_LOGI(TAG, "Flash: %lu MB", flash_size / (1024 * 1024));
    ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());

    while (true) {
        ESP_LOGI(TAG, "alive tick=%lu, free_heap=%lu",
                 tick++,
                 esp_get_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

## Output Expectations

When giving code to a beginner:

- State which file to edit.
- State what successful serial output should look like.
- Explain any build flags added.
- Do not add peripheral code yet.
