# Framework Choice

Use this when deciding whether to stay on Arduino, use ESP-IDF, or migrate.

## Prefer Arduino When

- The board is still in early experiments.
- The user is a beginner.
- Existing maker libraries cover the sensor, display, or module.
- Timing and memory requirements are ordinary.
- A small `setup()` / `loop()` example is enough to prove the idea.

## Prefer ESP-IDF When

- The feature needs precise FreeRTOS tasks, priorities, queues, or timers.
- DMA buffers, PSRAM, IRAM, or memory capabilities matter.
- The app needs production OTA, security, partition tables, logging, or power management.
- Arduino libraries hide too much of a driver, bus, or interrupt behavior.
- The project needs clean component boundaries or long-term maintainability.
- Official Espressif examples, sdkconfig options, or `idf.py` workflows are the clearest source of truth.

## Migration Guidance

- First prove the hardware path in Arduino if it is faster.
- Record working pins, bus speed, display settings, and serial logs.
- Move one subsystem at a time to ESP-IDF.
- Keep a known-good smoke test branch or example.
- Recheck USB serial settings because Arduino `Serial` behavior and ESP-IDF logging are configured differently.
- Use `idf.py` directly when debugging sdkconfig, partition table, bootloader, USB console, or component issues.
