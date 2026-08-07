# USB Serial And Monitor

Use this before upload or monitor, and whenever upload or serial monitor fails.

## Linux Device Types

- `/dev/ttyACM*`: native USB CDC/JTAG or CDC ACM devices.
- `/dev/ttyUSB*`: USB-UART bridges such as CH340, CP210x, FTDI.

## WSL2 With usbipd-win

If WSL2 cannot see `/dev/ttyACM*` or `/dev/ttyUSB*`, first check Windows-side visibility. The agent may run this from WSL when `powershell.exe` is available:

```bash
powershell.exe -NoProfile -Command "if (Get-Command usbipd -ErrorAction SilentlyContinue) { usbipd list } else { 'usbipd not found' }"
```

If `usbipd` is missing, ask the user to install `usbipd-win` on Windows, for example:

```powershell
winget install --id dorssel.usbipd-win
```

Explain that installation and `bind` may require administrator approval.

On Windows:

```powershell
usbipd list
usbipd bind --busid <BUSID>
usbipd attach --wsl --busid <BUSID>
```

On WSL2:

```bash
ls -l /dev/ttyACM* /dev/ttyUSB* 2>/dev/null
```

If permissions fail, add the user to `dialout`, restart WSL, and reattach.

Hard stop: do not run upload or monitor from WSL until the intended board appears as `/dev/ttyACM*` or `/dev/ttyUSB*` and is readable/writable by the current user.

If multiple ports appear, identify the target by plug/unplug comparison, `lsusb`, Device Manager, `usbipd list`, or serial port metadata before uploading.

## Monitor Expectations

A good smoke test should print:

```text
esp32-ai-bringup: Arduino smoke test
alive tick=...
```

For ESP-IDF smoke tests, the banner may use ESP-IDF logging, but it must still show repeated `alive tick` output.

## Standard Monitor Helper

For AI-driven bring-up, prefer a bounded helper over interactive monitors.

If the target repository does not have it, create:

```text
tools/serial-monitor/monitor_serial.py
```

Use the copy bundled at:

```text
../scripts/monitor_serial.py
```

Why:

- Exits after `--seconds` instead of hanging forever.
- Sets DTR high by default, useful for Arduino USB CDC.
- Keeps RTS low to avoid unwanted reset behavior.
- Decodes noisy boot logs with replacement characters instead of crashing.

Run:

```bash
python3 tools/serial-monitor/monitor_serial.py /dev/ttyACM0 --baud 115200 --seconds 10
```

Use the confirmed port, not `/dev/ttyACM0` as a blind default.

If Python reports `ModuleNotFoundError: No module named 'serial'`, install `pyserial` in the active Python environment.

If only ROM boot logs appear:

- Arduino `Serial` may be routed to UART0 instead of USB CDC.
- For native USB-Serial/JTAG, verify `ARDUINO_USB_MODE` and `ARDUINO_USB_CDC_ON_BOOT`.
- Ensure DTR is high in the monitor.

If no logs appear:

- Confirm the port.
- Close other monitors.
- Try lower upload speed.
- Press reset after opening monitor.
