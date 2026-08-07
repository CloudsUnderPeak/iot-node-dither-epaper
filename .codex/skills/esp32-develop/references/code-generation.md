# Arduino And ESP-IDF Code Generation

Use before generating firmware code after first bring-up.

## Bus Code Generation Rules

- Generate I2C/SPI/UART code only after pins pass GPIO safety checks.
- Include explicit pin numbers, not implicit defaults, unless the board profile says defaults are correct.
- For I2C, generate an I2C scanner before a sensor-specific example.
- For SPI displays, require controller, resolution, MOSI/SCLK/CS/DC/RST/BL pins, color order, and bus speed.
- For PWM/LED, check output capability and boot strapping behavior.
- For ADC, check whether Wi-Fi will be active and whether the variant has ADC2/Wi-Fi conflicts.
- For interrupts, keep ISRs short and avoid blocking calls.
- For DMA, verify memory capabilities and buffer alignment.

## Arduino Pattern

Use Arduino when speed of iteration and maker library availability matter more than low-level control.

```cpp
#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("feature test start");
}

void loop() {
  delay(1000);
}
```

Add one peripheral at a time and print enough serial output to prove each step.

## ESP-IDF Pattern

Use ESP-IDF when the feature needs driver-level control, DMA, FreeRTOS task structure, power management, OTA/security, or production configuration.

```c
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "app";

void app_main(void) {
    ESP_LOGI(TAG, "feature test start");
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

## Output Expectations

When giving generated code to a beginner:

- State which file to edit.
- State what successful serial output should look like.
- Explain any build flags, libraries, or sdkconfig values added.
- Explain any pin that is risky or board-specific.
