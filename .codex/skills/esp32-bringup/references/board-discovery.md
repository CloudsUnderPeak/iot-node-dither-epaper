# Board Discovery

Use this when the board model, chip, flash size, USB mode, or pins are unknown.

## Ask First

Ask for:

- Exact development board model.
- ESP chip or module marking, if visible.
- Product link, if available.
- Serial device path, such as `/dev/ttyACM0`, `/dev/ttyUSB0`, or `COMx`.
- Any connected peripherals.

Detect host OS yourself when possible. Ask whether WSL2, Windows PowerShell, macOS, or Linux is being used only when local detection cannot answer it.

If the user does not know the model, ask for:

- Chip marking.
- Silkscreen text.
- USB VID/PID from `lsusb`, Device Manager, or `usbipd list`.
- A clear photo, when available.

## Spec Lookup

Use official sources first:

- Board vendor product page.
- Schematic or pinout PDF.
- Espressif module or SoC datasheet.
- PlatformIO board registry only after hardware facts are known.

When internet access is available, actively search for the official board page and official schematic/pinout after the user provides the board model. If internet access is blocked, ask the user for a product link, datasheet, schematic, or pinout image instead of guessing.

Do not choose any of these before the official-source check:

- PlatformIO `board`.
- ESP-IDF target.
- USB CDC or USB mode flags.
- Flash size, PSRAM, upload speed, partitions, or LED pin.

Capture:

- Chip variant.
- Flash size and PSRAM.
- USB bridge type.
- LED pin and LED type.
- BOOT/RESET behavior.
- Strapping pins.
- Flash/PSRAM reserved pins.
- Default I2C/SPI/UART pins, if documented.

## Do Not Guess

Unknown is acceptable. Guessing a pin can break boot, damage attached hardware, or waste debugging time.

Do not use a generic PlatformIO board such as `esp32dev` merely because the board says "ESP32". Use it only after the chip and board class are verified and it is a reasonable match for the smoke test.

If the user cannot identify the board yet, stop at host/tool detection and give them a concrete way to identify it:

- Read the silkscreen on the board.
- Read the metal module or chip marking.
- Share a product link.
- Share a clear photo.
- On Windows, check Device Manager or `usbipd list`.
- On Linux/WSL, check `lsusb` and `/dev/ttyACM*` or `/dev/ttyUSB*`.

## Board Profile

Record verified facts in a local board profile file chosen by the project, such as `board-profile.local.md`, only when useful for future work. Do not block first bring-up on this file. Keep generic reusable files free of private board identity unless the repository is intentionally board-specific.
