# Serial monitor

`monitor_serial.py` reads serial output for a bounded duration. It is suitable for automated smoke tests and does not occupy the terminal indefinitely like an interactive monitor.

## Requirements

- Python 3
- The `pyserial` package
- The currently connected board's confirmed serial port; never guess a port from an old record

## Usage

Run from the project root and replace the example port with the confirmed device:

```bash
python3 tools/serial-monitor/monitor_serial.py /dev/ttyACM0 --baud 115200 --seconds 15
```

Options:

- `port`: Required serial device path or Windows `COMx`
- `--baud`: Baud rate; defaults to `115200`
- `--seconds`: Monitoring duration in seconds; defaults to `8`
- `--no-dtr`: Keep DTR low while monitoring

The tool keeps RTS low by default to avoid an unexpected reset while monitoring.
