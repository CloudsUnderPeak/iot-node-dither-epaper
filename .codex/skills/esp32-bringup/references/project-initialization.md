# Project Initialization

Use when the user only copied the skill folder into a new or empty project and asks the AI to initialize the ESP32 environment.

## Initialization Gate

Before writing firmware or board-specific config, confirm:

- [ ] Exact development board model, or enough visible markings/product information to identify it.
- [ ] ESP chip family, such as ESP32, ESP32-S2, ESP32-S3, ESP32-C3, or ESP32-C6.
- [ ] Framework choice: Arduino or ESP-IDF, after explaining the difference and recommending Arduino for first bring-up.
- [ ] Host environment: Linux, macOS, Windows PowerShell, or Windows WSL2.
- [ ] Serial/upload port, or a clear plan to make the port visible.
- [ ] Official source checked for board id, USB serial behavior, flash/PSRAM, and any LED pin used.

If any required item is missing, ask for it or run non-invasive detection first. Do not create `platformio.ini` with a guessed board id, and do not create any Arduino or ESP-IDF scaffold before the user confirms the framework.

## Choose Framework

Explain the choice briefly:

- Arduino: easiest first path for beginners, lots of maker examples, simple `setup()` and `loop()`.
- ESP-IDF: official Espressif SDK, better for production, FreeRTOS control, DMA, memory layout, security, OTA, power, and driver-level work.

Recommend Arduino for the first bring-up unless the user asks for ESP-IDF or the board/chip support requires ESP-IDF. The recommendation is not consent: wait for the user to confirm Arduino or choose ESP-IDF before writing `platformio.ini`, `src/main.cpp`, `CMakeLists.txt`, `main/main.c`, or other framework-specific files.

## PlatformIO + Arduino Scaffold

Create the smallest useful scaffold:

```text
platformio.ini
src/main.cpp
tools/serial-monitor/monitor_serial.py
```

Optionally create `tools/pio.sh` from `../scripts/pio.sh` when the host environment benefits from a project-local PlatformIO wrapper.

Create `boards/<board-id>.json` only when no suitable PlatformIO board id exists. Use verified official specs before filling flash size, MCU, variant, or upload settings.

Use `board = esp32dev` only for a confirmed generic ESP32 Dev Module-compatible board, not as a placeholder for unknown ESP32-family hardware.

`platformio.ini` should include:

- selected platform
- selected board id
- `framework = arduino`
- upload and monitor port only when confirmed; otherwise omit them until the port is known
- `monitor_speed = 115200`
- USB CDC flags only when board docs or chip behavior require them
- `STATUS_LED_PIN=-1` until LED pin is verified

## ESP-IDF Scaffold

Create the smallest valid ESP-IDF project:

```text
CMakeLists.txt
main/CMakeLists.txt
main/main.c
```

Then use the actual chip target:

```bash
idf.py set-target <target>
idf.py build
idf.py -p <port> flash monitor
```

Replace `<target>` and `<port>` only with verified values. Do not use sample targets or ports as defaults.

## Preserve User Work

Before creating or replacing files, inspect the repository. Do not overwrite existing application files unless the user explicitly asks for a fresh scaffold.

If the repo already has a project structure, adapt it instead of creating a parallel one.
