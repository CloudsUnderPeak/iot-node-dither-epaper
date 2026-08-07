# ESP-IDF And idf.py Bring-Up

Use this when the user chooses ESP-IDF, PlatformIO support is incomplete after investigation, or official Espressif tooling is the clearest path.

## Why idf.py Matters

`idf.py` is the official ESP-IDF project command. PlatformIO can build ESP-IDF projects, but `idf.py` is the reference workflow for target selection, configuration, build, flash, monitor, partition tables, sdkconfig, and official examples.

## Typical First Commands

After installing and exporting ESP-IDF in the shell:

```bash
idf.py set-target esp32c6
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

Use the actual chip target, such as `esp32`, `esp32s3`, `esp32c3`, or `esp32c6`.

Do not run `idf.py set-target` until the chip family is verified from the board model, chip marking, module marking, or official docs.

Treat `/dev/ttyACM0` below as an example. Use only the confirmed port for the user's board.

## Serial Port

For native USB CDC/JTAG boards, the Linux port often appears as `/dev/ttyACM0`.

For USB-UART bridge boards, it often appears as `/dev/ttyUSB0`.

Examples:

```bash
idf.py -p /dev/ttyACM0 flash
idf.py -p /dev/ttyACM0 monitor
idf.py -p /dev/ttyUSB0 flash monitor
```

Exit monitor with `Ctrl+]`.

## Configuration

Use:

```bash
idf.py menuconfig
```

Beginner bring-up should avoid broad config changes. Only adjust the target, serial settings, console output, flash size, partition table, or USB console settings when the board docs or failure mode justify it.

## When To Prefer idf.py During Bring-Up

- The user explicitly wants ESP-IDF.
- Official Espressif examples are the reference for the board or chip.
- PlatformIO board/platform support is lagging for a new chip.
- The failure involves sdkconfig, partition tables, bootloader, USB console, or ESP-IDF components.
- The next project goal will need ESP-IDF features anyway.

If choosing ESP-IDF because of a technical constraint rather than user preference, explain that reason and confirm before replacing an Arduino scaffold.

## When PlatformIO Is Still Fine

- The user wants Arduino first.
- The PlatformIO board/platform is known to support the chip.
- The goal is only a fast smoke test, serial proof, or maker-library experiment.
