# PlatformIO And Arduino Bring-Up

Use this when editing `platformio.ini`, `boards/*.json`, or first Arduino smoke-test code.

## Preferred First Bring-Up

For most beginners:

```ini
framework = arduino
monitor_speed = 115200
```

Use `STATUS_LED_PIN=-1` until the board profile verifies LED pin and type.

Do not create or edit PlatformIO Arduino files until the user has confirmed Arduino after hearing the Arduino versus ESP-IDF tradeoff, the board/chip has been identified, and official specs have been checked.

## ESP32-C6 With Arduino

If official PlatformIO support is incomplete for the selected chip, use a current Espressif Arduino-capable PlatformIO platform such as pioarduino after checking current support.

For native USB-Serial/JTAG serial output on ESP32-C3/C6/S2/S3 style boards, verify whether PlatformIO needs:

```ini
build_flags =
    -D ARDUINO_USB_MODE=1
    -D ARDUINO_USB_CDC_ON_BOOT=1
```

Some USB-UART bridge boards do not need these flags.

Do not add these flags merely because the chip family often uses native USB; verify the board USB path or use them only as a response to a matching serial-output failure.

## Build And Flash

Use `pio` directly when available:

```bash
pio run
pio run -t upload
python3 tools/serial-monitor/monitor_serial.py /dev/ttyACM0 --baud 115200 --seconds 10
```

Replace `/dev/ttyACM0` with the confirmed port; do not use it as a blind default.

If WSL or another Python environment needs a project-local wrapper, create `tools/pio.sh` from `../scripts/pio.sh` and use:

```bash
tools/pio.sh run
tools/pio.sh run -t upload
```

The wrapper enables local Python shim directories when present and otherwise delegates to `pio`.

Build may run after scaffold creation. Upload requires a confirmed, visible, accessible serial port.

## Local Board JSON

Create `boards/<generic-or-board-id>.json` when no PlatformIO board matches. Include only verified facts:

- MCU.
- Variant.
- Flash size.
- Upload speed.
- Debug target.
- Frameworks.

Avoid hiding private board identity inside generic starter projects.
