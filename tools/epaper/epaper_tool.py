#!/usr/bin/env python3
"""Convert images to EPDIMG and call the IOT-Node Dither E-Paper e-paper API."""

from __future__ import annotations

import argparse
import json
import struct
import sys
import time
import urllib.error
import urllib.request
import zlib
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable, Sequence
from urllib.parse import urlsplit, urlunsplit

WIDTH = 800
HEIGHT = 480
HEADER_BYTES = 40
FRAME_BYTES = WIDTH * HEIGHT // 2
UPLOAD_BYTES = HEADER_BYTES + FRAME_BYTES
MAGIC = b"EPDIMG\x00\x00"
VERSION = 1

# Controller nibble -> conversion reference RGB. Output always uses the exact
# controller codes even though the physical pigments are not sRGB devices.
COLORS: tuple[tuple[int, tuple[int, int, int]], ...] = (
    (0, (0, 0, 0)),
    (1, (255, 255, 255)),
    (2, (255, 255, 0)),
    (3, (255, 0, 0)),
    (5, (0, 0, 255)),
    (6, (0, 255, 0)),
)


class ToolError(RuntimeError):
    """Expected command-line failure."""


class ApiError(ToolError):
    def __init__(self, status: int, code: str, message: str, data: Any = None):
        super().__init__(f"HTTP {status} {code}: {message}")
        self.status = status
        self.code = code
        self.message = message
        self.data = data


@dataclass(frozen=True)
class ConvertedImage:
    payload: bytes
    generation: int
    crc32: int


def _pillow() -> tuple[Any, Any]:
    try:
        from PIL import Image, ImageOps
    except ImportError as exc:  # pragma: no cover - exercised without dependency
        raise ToolError(
            "Pillow is required for image conversion; run "
            "python -m pip install -r tools/epaper/requirements.txt"
        ) from exc
    return Image, ImageOps


def _orient_for_panel(image: Any, Image: Any, auto_rotate: bool) -> Any:
    """Rotate a portrait source clockwise when the target panel is landscape."""
    if auto_rotate and image.height > image.width and WIDTH > HEIGHT:
        return image.transpose(Image.Transpose.ROTATE_270)
    return image


def _prepare_rgb(path: Path, fit: str, auto_rotate: bool) -> Any:
    Image, ImageOps = _pillow()
    try:
        with Image.open(path) as opened:
            image = ImageOps.exif_transpose(opened)
            image = _orient_for_panel(image, Image, auto_rotate)
            if image.mode in ("RGBA", "LA") or "transparency" in image.info:
                rgba = image.convert("RGBA")
                white = Image.new("RGBA", rgba.size, (255, 255, 255, 255))
                image = Image.alpha_composite(white, rgba).convert("RGB")
            else:
                image = image.convert("RGB")

            resampling = Image.Resampling.LANCZOS
            if fit == "cover":
                return ImageOps.fit(image, (WIDTH, HEIGHT), method=resampling)
            if fit == "stretch":
                return image.resize((WIDTH, HEIGHT), resampling)
            if fit != "contain":
                raise ToolError(f"unsupported fit mode: {fit}")
            contained = ImageOps.contain(image, (WIDTH, HEIGHT), method=resampling)
            canvas = Image.new("RGB", (WIDTH, HEIGHT), (255, 255, 255))
            canvas.paste(
                contained,
                ((WIDTH - contained.width) // 2, (HEIGHT - contained.height) // 2),
            )
            return canvas
    except (OSError, ValueError) as exc:
        raise ToolError(f"cannot read image {path}: {exc}") from exc


def _nearest(rgb: Sequence[float]) -> tuple[int, tuple[int, int, int]]:
    red, green, blue = rgb
    return min(
        COLORS,
        key=lambda item: (red - item[1][0]) ** 2
        + (green - item[1][1]) ** 2
        + (blue - item[1][2]) ** 2,
    )


def _codes_without_dither(pixels: Iterable[Sequence[int]]) -> bytearray:
    return bytearray(_nearest(pixel)[0] for pixel in pixels)


def _codes_with_floyd_steinberg(pixels: Sequence[Sequence[int]]) -> bytearray:
    output = bytearray(WIDTH * HEIGHT)
    current = [[0.0, 0.0, 0.0] for _ in range(WIDTH + 2)]
    following = [[0.0, 0.0, 0.0] for _ in range(WIDTH + 2)]
    for y in range(HEIGHT):
        for x in range(WIDTH):
            source = pixels[y * WIDTH + x]
            adjusted = [
                min(255.0, max(0.0, source[channel] + current[x + 1][channel]))
                for channel in range(3)
            ]
            code, chosen = _nearest(adjusted)
            output[y * WIDTH + x] = code
            error = [adjusted[channel] - chosen[channel] for channel in range(3)]
            for channel in range(3):
                current[x + 2][channel] += error[channel] * 7.0 / 16.0
                following[x][channel] += error[channel] * 3.0 / 16.0
                following[x + 1][channel] += error[channel] * 5.0 / 16.0
                following[x + 2][channel] += error[channel] * 1.0 / 16.0
        current, following = following, [[0.0, 0.0, 0.0] for _ in range(WIDTH + 2)]
    return output


def pack_codes(codes: Sequence[int]) -> bytes:
    if len(codes) != WIDTH * HEIGHT:
        raise ToolError(f"expected {WIDTH * HEIGHT} pixels, got {len(codes)}")
    valid = {code for code, _ in COLORS}
    frame = bytearray(FRAME_BYTES)
    for index in range(0, len(codes), 2):
        left = codes[index]
        right = codes[index + 1]
        if left not in valid or right not in valid:
            raise ToolError(f"invalid palette code at pixel {index}")
        frame[index // 2] = (left << 4) | right
    return bytes(frame)


def build_epdimg(frame: bytes, generation: int | None = None) -> ConvertedImage:
    if len(frame) != FRAME_BYTES:
        raise ToolError(f"expected {FRAME_BYTES} frame bytes, got {len(frame)}")
    if generation is None:
        generation = time.time_ns() & 0xFFFFFFFFFFFFFFFF
    if generation <= 0 or generation > 0xFFFFFFFFFFFFFFFF:
        raise ToolError("generation must be between 1 and 18446744073709551615")
    crc32 = zlib.crc32(frame) & 0xFFFFFFFF
    header = struct.pack(
        "<8sIIIIIIQ",
        MAGIC,
        VERSION,
        HEADER_BYTES,
        WIDTH,
        HEIGHT,
        FRAME_BYTES,
        crc32,
        generation,
    )
    payload = header + frame
    if len(payload) != UPLOAD_BYTES:
        raise AssertionError("internal EPDIMG size mismatch")
    return ConvertedImage(payload=payload, generation=generation, crc32=crc32)


def convert_image(
    path: Path,
    *,
    fit: str = "contain",
    dither: str = "floyd-steinberg",
    auto_rotate: bool = True,
    generation: int | None = None,
) -> ConvertedImage:
    image = _prepare_rgb(path, fit, auto_rotate)
    flattened = getattr(image, "get_flattened_data", None)
    pixels = list(flattened() if flattened is not None else image.getdata())
    if dither == "none":
        codes = _codes_without_dither(pixels)
    elif dither == "floyd-steinberg":
        codes = _codes_with_floyd_steinberg(pixels)
    else:
        raise ToolError(f"unsupported dither mode: {dither}")
    return build_epdimg(pack_codes(codes), generation)


def normalize_base_url(ip: str, port: int, https: bool) -> str:
    raw = ip.strip()
    if not raw:
        raise ToolError("--ip is required for API commands")
    if port < 1 or port > 65535:
        raise ToolError("--port must be between 1 and 65535")
    parsed = urlsplit(raw if "://" in raw else f"{'https' if https else 'http'}://{raw}")
    if parsed.scheme not in ("http", "https") or not parsed.hostname:
        raise ToolError(f"invalid device address: {ip}")
    host = f"[{parsed.hostname}]" if ":" in parsed.hostname else parsed.hostname
    try:
        parsed_port = parsed.port
    except ValueError as exc:
        raise ToolError(f"invalid device address: {ip}") from exc
    selected_port = parsed_port if parsed_port is not None else port
    default_port = 443 if parsed.scheme == "https" else 80
    netloc = host if selected_port == default_port else f"{host}:{selected_port}"
    return urlunsplit((parsed.scheme, netloc, parsed.path.rstrip("/"), "", ""))


class EpaperApiClient:
    def __init__(self, base_url: str, *, timeout: float = 15.0, token: str | None = None):
        self.base_url = base_url.rstrip("/")
        self.timeout = timeout
        self.token = token

    def _request(self, method: str, path: str, body: bytes | None = None) -> dict[str, Any]:
        headers = {"Accept": "application/json"}
        if body is not None:
            headers["Content-Type"] = "application/octet-stream"
            headers["Content-Length"] = str(len(body))
        if self.token:
            headers["Authorization"] = f"Bearer {self.token}"
        request = urllib.request.Request(
            f"{self.base_url}{path}", data=body, headers=headers, method=method
        )
        try:
            with urllib.request.urlopen(request, timeout=self.timeout) as response:
                raw = response.read()
                status = response.status
        except urllib.error.HTTPError as exc:
            raw = exc.read()
            status = exc.code
        except (urllib.error.URLError, TimeoutError, OSError) as exc:
            raise ToolError(f"cannot reach {self.base_url}: {exc}") from exc
        try:
            document = json.loads(raw.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise ToolError(f"device returned non-JSON HTTP {status} response") from exc
        if not 200 <= status < 300 or document.get("success") is not True:
            data = document.get("data") or {}
            raise ApiError(
                status,
                str(data.get("code", "api_error")),
                str(document.get("message", "request failed")),
                data,
            )
        return document

    def capabilities(self) -> dict[str, Any]:
        return self._request("GET", "/api/epaper")

    def status(self) -> dict[str, Any]:
        return self._request("GET", "/api/epaper/status")

    def upload(self, payload: bytes) -> dict[str, Any]:
        if len(payload) != UPLOAD_BYTES:
            raise ToolError(f"upload must be exactly {UPLOAD_BYTES} bytes")
        return self._request("POST", "/api/epaper/image", payload)

    def action(self, name: str) -> dict[str, Any]:
        if name not in ("white", "palette", "refresh"):
            raise ToolError(f"unsupported action: {name}")
        return self._request("POST", f"/api/epaper/image/{name}", b"")

    def wait_for_draw(self, timeout: float, interval: float = 1.0) -> dict[str, Any]:
        deadline = time.monotonic() + timeout
        last: dict[str, Any] | None = None
        while time.monotonic() < deadline:
            last = self.status()
            data = last.get("data") or {}
            state = data.get("state")
            if state == "cooldown":
                return last
            if state == "unavailable":
                raise ToolError(
                    "panel became unavailable; perform the full power-cycle recovery "
                    "reported by /api/epaper/status"
                )
            if state == "idle":
                operation = data.get("last_operation") or {}
                if operation.get("result") == "failed":
                    raise ToolError(
                        "draw failed: "
                        f"{operation.get('error_code', 'unknown_error')}"
                    )
                if operation.get("result") == "success":
                    return last
            time.sleep(interval)
        state = (last or {}).get("data", {}).get("state", "unknown")
        raise ToolError(f"timed out waiting for draw completion; last state={state}")


def _print_json(document: Any) -> None:
    print(json.dumps(document, ensure_ascii=False, indent=2, sort_keys=True))


def _add_conversion_options(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("image", type=Path, help="PNG/JPEG/BMP/WebP or another Pillow image")
    parser.add_argument("--fit", choices=("contain", "cover", "stretch"), default="contain")
    parser.add_argument(
        "--no-auto-rotate",
        dest="auto_rotate",
        action="store_false",
        help="keep portrait sources upright instead of rotating them for the landscape panel",
    )
    parser.set_defaults(auto_rotate=True)
    parser.add_argument(
        "--dither", choices=("floyd-steinberg", "none"), default="floyd-steinberg"
    )
    parser.add_argument("--generation", type=int, help="optional non-zero uint64 generation")
    parser.add_argument("--output", type=Path, help="also save the generated .epd file")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Convert images and control the IOT-Node Dither E-Paper 7.3-inch e-paper API."
    )
    parser.add_argument("--ip", help="device IP/hostname, optionally including http:// or https://")
    parser.add_argument("--port", type=int, default=80, help="HTTP port (default: 80)")
    parser.add_argument("--https", action="store_true", help="use HTTPS when --ip has no scheme")
    parser.add_argument("--token", help="optional Bearer token")
    parser.add_argument("--timeout", type=float, default=15.0, help="per-request timeout seconds")
    parser.add_argument(
        "--draw-timeout", type=float, default=120.0, help="queued/drawing wait timeout seconds"
    )
    parser.add_argument("--no-wait", action="store_true", help="return immediately after HTTP 202")
    subparsers = parser.add_subparsers(dest="command", required=True)

    image_parser = subparsers.add_parser("image", help="convert, upload, and draw an image")
    _add_conversion_options(image_parser)
    image_parser.add_argument(
        "--output-only", action="store_true", help="convert without calling the device API"
    )

    convert_parser = subparsers.add_parser("convert", help="only create an EPDIMG file")
    _add_conversion_options(convert_parser)

    for action in ("white", "palette", "refresh"):
        subparsers.add_parser(action, help=f"request the device {action} action")
    subparsers.add_parser("status", help="show dynamic e-paper status")
    subparsers.add_parser("capabilities", help="show fixed panel/API capabilities")
    return parser


def _write_output(converted: ConvertedImage, output: Path | None, source: Path) -> Path | None:
    if output is None:
        return None
    target = output
    if target.is_dir():
        target = target / f"{source.stem}.epd"
    try:
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(converted.payload)
    except OSError as exc:
        raise ToolError(f"cannot write {target}: {exc}") from exc
    return target


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        converted: ConvertedImage | None = None
        if args.command in ("image", "convert"):
            converted = convert_image(
                args.image,
                fit=args.fit,
                dither=args.dither,
                auto_rotate=args.auto_rotate,
                generation=args.generation,
            )
            output = args.output
            if args.command == "convert" and output is None:
                output = args.image.with_suffix(".epd")
            written = _write_output(converted, output, args.image)
            summary = {
                "source": str(args.image),
                "output": str(written) if written else None,
                "upload_bytes": len(converted.payload),
                "frame_bytes": FRAME_BYTES,
                "generation": str(converted.generation),
                "crc32": f"{converted.crc32:08X}",
            }
            if args.command == "convert" or args.output_only:
                _print_json(summary)
                return 0

        base_url = normalize_base_url(args.ip or "", args.port, args.https)
        client = EpaperApiClient(base_url, timeout=args.timeout, token=args.token)
        if args.command == "status":
            _print_json(client.status())
            return 0
        if args.command == "capabilities":
            _print_json(client.capabilities())
            return 0
        if args.command == "image":
            assert converted is not None
            accepted = client.upload(converted.payload)
        else:
            accepted = client.action(args.command)
        _print_json(accepted)
        if not args.no_wait:
            _print_json(client.wait_for_draw(args.draw_timeout))
        return 0
    except ToolError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
