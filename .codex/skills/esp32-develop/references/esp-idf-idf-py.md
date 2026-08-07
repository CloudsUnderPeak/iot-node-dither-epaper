# ESP-IDF idf.py Workflow

Use this for official ESP-IDF development, migration from Arduino, or debugging behavior that depends on sdkconfig, components, partition tables, bootloader, logs, or monitor behavior.

## Positioning

`idf.py` is important. It is the official ESP-IDF command front end. PlatformIO is useful, but ESP-IDF concepts such as target, sdkconfig, components, partition tables, bootloader, logging, flash monitor, and menuconfig are native to `idf.py`.

## Core Commands

```bash
idf.py set-target esp32c6
idf.py menuconfig
idf.py build
idf.py -p /dev/ttyACM0 flash
idf.py -p /dev/ttyACM0 monitor
idf.py -p /dev/ttyACM0 flash monitor
idf.py clean
idf.py fullclean
```

Use the actual target: `esp32`, `esp32s2`, `esp32s3`, `esp32c3`, `esp32c6`, `esp32h2`, or another supported Espressif target.

## New Project Flow

```bash
idf.py create-project app
cd app
idf.py set-target esp32c6
idf.py menuconfig
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

For an existing project, do not run `set-target` casually without understanding that it can reset target-specific build configuration.

## Development Checks

- Use `idf.py menuconfig` for sdkconfig changes instead of editing generated config blindly.
- Use ESP-IDF components for reusable modules.
- Check partition tables before OTA, NVS-heavy apps, large binaries, or filesystem work.
- Check logging level and console output when serial logs differ from Arduino behavior.
- Use `idf.py monitor` for interactive debugging, and a bounded serial helper for AI log capture when appropriate.

## Migration From Arduino

- Preserve known-good pin assignments and bus speeds from Arduino.
- Recreate one subsystem at a time as ESP-IDF components.
- Replace Arduino `Serial` assumptions with ESP-IDF logging and console configuration.
- Recheck USB console settings on native USB boards.
- Keep the Arduino smoke test available until the ESP-IDF path is proven.
