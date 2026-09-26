#!/usr/bin/env python3
import argparse
import sys
import time

import serial


def main() -> int:
    parser = argparse.ArgumentParser(description="Read serial output for a short duration.")
    parser.add_argument("port", help="Serial port, for example COMx, /dev/ttyACM0, or /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--seconds", type=float, default=8.0)
    parser.add_argument("--no-dtr", action="store_true", help="Leave DTR low while monitoring.")
    args = parser.parse_args()

    # Configure control lines before opening the device: applying defaults
    # first can reset USB-Serial/JTAG boards even when --no-dtr is requested.
    connection = serial.Serial(port=None, baudrate=args.baud, timeout=0.2)
    connection.dtr = not args.no_dtr
    connection.rts = False
    connection.port = args.port
    with connection as ser:
        end_at = time.time() + args.seconds
        print(f"--- {args.port} monitor {args.baud} baud ---")
        while time.time() < end_at:
            data = ser.read(4096)
            if data:
                print(data.decode("utf-8", "replace"), end="")
                sys.stdout.flush()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
