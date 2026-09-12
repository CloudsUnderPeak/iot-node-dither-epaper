#!/usr/bin/env python3
"""Monitor e-paper cooldown cleanup and report whether the device recovers.

This tool only reads GET /api/epaper/status. It never bypasses the 180-second
protection gate, triggers a draw, restarts the device, or requests power-cycle
recovery.
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from epaper_tool import ApiError, EpaperApiClient, ToolError, normalize_base_url


def _stamp() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def _summary(document: dict[str, Any]) -> str:
    data = document.get("data") or {}
    operation = data.get("last_operation") or {}
    return (
        f"state={data.get('state', '?')} "
        f"retry={data.get('retry_after_seconds', '-')} "
        f"can_draw={data.get('can_draw', '?')} "
        f"panel={data.get('panel_state', '?')} "
        f"error={operation.get('error_code', 'none')} "
        f"result={operation.get('result', 'none')}"
    )


def _write(log: Path | None, line: str) -> None:
    print(line, flush=True)
    if log is not None:
        with log.open("a", encoding="utf-8") as stream:
            stream.write(line + "\n")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Read-only e-paper cooldown and marker-cleanup monitor."
    )
    parser.add_argument("--ip", required=True, help="device IP/hostname")
    parser.add_argument("--port", type=int, default=80)
    parser.add_argument("--https", action="store_true")
    parser.add_argument("--token", help="optional Bearer token")
    parser.add_argument("--interval", type=float, default=5.0,
                        help="seconds between status requests (default: 5)")
    parser.add_argument("--duration", type=float, default=600.0,
                        help="stop after this many seconds; 0 means until terminal state")
    parser.add_argument("--timeout", type=float, default=5.0)
    parser.add_argument("--log", type=Path, help="also append timestamped output to a file")
    parser.add_argument("--once", action="store_true", help="read one status and exit")
    return parser


def main() -> int:
    args = build_parser().parse_args()
    if args.interval < 5 or args.timeout <= 0 or args.duration < 0:
        print("error: interval >= 5, timeout > 0, duration >= 0 required", file=sys.stderr)
        return 2
    client = EpaperApiClient(
        normalize_base_url(args.ip, args.port, args.https),
        timeout=args.timeout,
        token=args.token,
    )
    started = time.monotonic()
    previous = None
    while True:
        if args.duration and time.monotonic() - started >= args.duration:
            _write(args.log, f"{_stamp()} TIMEOUT: recovery not confirmed")
            return 4
        try:
            document = client.status()
        except ApiError as exc:
            line = f"{_stamp()} HTTP {exc.status} {exc.code}: {exc.message}"
            _write(args.log, line)
            if args.once:
                return 2
            time.sleep(args.interval)
            continue
        except ToolError as exc:
            line = f"{_stamp()} transport error: {exc}"
            _write(args.log, line)
            if args.once:
                return 2
            time.sleep(args.interval)
            continue

        data = document.get("data") or {}
        current = json.dumps(data, sort_keys=True, ensure_ascii=False)
        if current != previous or args.once:
            _write(args.log, f"{_stamp()} {_summary(document)}")
            previous = current

        state = data.get("state")
        operation = data.get("last_operation") or {}
        error_code = operation.get("error_code")
        if state == "idle" and data.get("can_draw") is True:
            _write(args.log, f"{_stamp()} RECOVERED: e-paper is idle and drawable")
            return 0
        if state == "unavailable" and error_code != "marker_clear_failed":
            _write(args.log, f"{_stamp()} BLOCKED: manual recovery required")
            return 3
        if args.once:
            return 0
        time.sleep(args.interval)


if __name__ == "__main__":
    raise SystemExit(main())
