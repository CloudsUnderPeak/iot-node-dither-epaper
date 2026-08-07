# GPIO Safety Checks

Use before assigning or modifying any GPIO, including LEDs, buttons, displays, sensors, and buses.

## Teach This First

GPIOs are not all equal. Some pins boot the chip, some are wired to flash, some cannot output, and some analog pins conflict with Wi-Fi on older variants.

## Required Checks

For every proposed pin assignment, check:

1. Chip variant.
2. Board schematic or pinout.
3. Whether the pin is exposed on this board.
4. Whether the pin is a strapping pin.
5. Whether the pin is used by flash, PSRAM, USB, JTAG, or onboard peripherals.
6. Whether the pin supports the requested direction.
7. Whether analog usage conflicts with Wi-Fi.
8. Whether external circuits pull the pin high/low at boot.

## Common Variant Rules

| Variant | Flash Pins To Avoid | Strapping Pins To Treat Carefully | Input-Only Pins | ADC2/Wi-Fi Conflict |
|---|---:|---:|---:|---|
| ESP32 | GPIO6-11 | GPIO0, 2, 5, 12, 15 | GPIO34-39 | Yes, ADC2 |
| ESP32-S2 | GPIO26-32 | GPIO0, 45, 46 | GPIO46 | Yes, ADC2 |
| ESP32-S3 | GPIO26-32 | GPIO0, 3, 45, 46 | GPIO46 | Yes, ADC2 |
| ESP32-C3 | GPIO12-17 | GPIO2, 8, 9 | none typical | No |
| ESP32-C6 | GPIO24-29 | GPIO4, 5, 8, 9, 15 | none typical | No |

These are chip-family rules. A development board can reserve additional pins for LEDs, buttons, USB, battery measurement, displays, cameras, or other onboard devices.

## High-Risk Examples

- Original ESP32 GPIO12: if pulled high during boot, it can select the wrong flash voltage and prevent boot.
- Original ESP32 GPIO6-11: connected to SPI flash; using them will crash the chip.
- Input-only pins: cannot drive LEDs, I2C SCL/SDA, SPI MOSI/SCLK, or UART TX.
- Strapping pins: can be used after boot, but attached circuits must not force the wrong boot state.

## Beginner-Safe Defaults

- Bring up one device at a time.
- For I2C, start with an I2C scanner before adding a sensor driver.
- For displays, verify controller, resolution, bus type, voltage, and reset/backlight pins.
