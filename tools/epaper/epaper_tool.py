#!/usr/bin/env python3
"""Convert images to EPDIMG and call the IOT-Node Dither E-Paper e-paper API."""

from __future__ import annotations

import argparse
import gzip
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

HEADER_BYTES = 40
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
class Geometry:
    width: int
    height: int

    def __post_init__(self):
        if (type(self.width) is not int or type(self.height) is not int
                or not 0 < self.width <= 4096 or not 0 < self.height <= 4096
                or self.width % 2):
            raise ToolError("invalid packed panel dimensions (positive, even width, at most 4096)")

    @property
    def frame_bytes(self) -> int:
        return self.width * self.height // 2

    @property
    def image_bytes(self) -> int:
        return HEADER_BYTES + self.frame_bytes

    @classmethod
    def from_capabilities(cls, data: dict[str, Any]) -> Geometry:
        try:
            panel, image, actions = data["panel"], data["image"], data["capabilities"]
            geometry = cls(panel["width"], panel["height"])
            codes = panel["color_codes"]
            valid = (isinstance(panel["model"], str) and panel["model"].strip()
                     and type(panel["colors"]) is int and panel["colors"] == 6
                     and isinstance(codes, list)
                     and all(type(code) is int for code in codes)
                     and sorted(codes) == [0, 1, 2, 3, 5, 6]
                     and image["format"] == "epdimg" and image["header_bytes"] == HEADER_BYTES
                     and image["frame_bytes"] == geometry.frame_bytes
                     and image["upload_bytes"] == geometry.image_bytes
                     and image["upload_uncompressed_bytes"] == geometry.image_bytes
                     and image["stored_encoding"] == "gzip"
                     and isinstance(image["upload_encodings"], list)
                     and "gzip" in image["upload_encodings"]
                     and type(image["max_compressed_bytes"]) is int
                     and 0 < image["max_compressed_bytes"] <= geometry.image_bytes * 2 + 2048
                     and actions["upload"] is True and actions["refresh"] is True)
            if valid:
                return geometry
        except (KeyError, TypeError, ValueError):
            pass
        raise ToolError("unsupported or inconsistent e-paper capability")


# Explicit offline conversion defaults; connected operations use capabilities.
OFFLINE_GEOMETRY = Geometry(800, 480)
WIDTH, HEIGHT = OFFLINE_GEOMETRY.width, OFFLINE_GEOMETRY.height
FRAME_BYTES, UPLOAD_BYTES = OFFLINE_GEOMETRY.frame_bytes, OFFLINE_GEOMETRY.image_bytes


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


def _orient_for_panel(image: Any, Image: Any, auto_rotate: bool, geometry: Geometry = OFFLINE_GEOMETRY) -> Any:
    """Rotate a portrait source clockwise when the target panel is landscape."""
    if auto_rotate and image.height > image.width and geometry.width > geometry.height:
        return image.transpose(Image.Transpose.ROTATE_270)
    return image


def _prepare_rgb(path: Path, fit: str, auto_rotate: bool, geometry: Geometry) -> Any:
    Image, ImageOps = _pillow()
    try:
        with Image.open(path) as opened:
            image = ImageOps.exif_transpose(opened)
            image = _orient_for_panel(image, Image, auto_rotate, geometry)
            if image.mode in ("RGBA", "LA") or "transparency" in image.info:
                rgba = image.convert("RGBA")
                white = Image.new("RGBA", rgba.size, (255, 255, 255, 255))
                image = Image.alpha_composite(white, rgba).convert("RGB")
            else:
                image = image.convert("RGB")

            resampling = Image.Resampling.LANCZOS
            if fit == "cover":
                return ImageOps.fit(image, (geometry.width, geometry.height), method=resampling)
            if fit == "stretch":
                return image.resize((geometry.width, geometry.height), resampling)
            if fit != "contain":
                raise ToolError(f"unsupported fit mode: {fit}")
            contained = ImageOps.contain(image, (geometry.width, geometry.height), method=resampling)
            canvas = Image.new("RGB", (geometry.width, geometry.height), (255, 255, 255))
            canvas.paste(
                contained,
                ((geometry.width - contained.width) // 2, (geometry.height - contained.height) // 2),
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


def _codes_with_floyd_steinberg(pixels: Sequence[Sequence[int]], geometry: Geometry) -> bytearray:
    output = bytearray(geometry.width * geometry.height)
    current = [[0.0, 0.0, 0.0] for _ in range(geometry.width + 2)]
    following = [[0.0, 0.0, 0.0] for _ in range(geometry.width + 2)]
    for y in range(geometry.height):
        for x in range(geometry.width):
            source = pixels[y * geometry.width + x]
            adjusted = [
                min(255.0, max(0.0, source[channel] + current[x + 1][channel]))
                for channel in range(3)
            ]
            code, chosen = _nearest(adjusted)
            output[y * geometry.width + x] = code
            error = [adjusted[channel] - chosen[channel] for channel in range(3)]
            for channel in range(3):
                current[x + 2][channel] += error[channel] * 7.0 / 16.0
                following[x][channel] += error[channel] * 3.0 / 16.0
                following[x + 1][channel] += error[channel] * 5.0 / 16.0
                following[x + 2][channel] += error[channel] * 1.0 / 16.0
        current, following = following, [[0.0, 0.0, 0.0] for _ in range(geometry.width + 2)]
    return output


def pack_codes(codes: Sequence[int], geometry: Geometry = OFFLINE_GEOMETRY) -> bytes:
    if len(codes) != geometry.width * geometry.height:
        raise ToolError(f"expected {geometry.width * geometry.height} pixels, got {len(codes)}")
    valid = {code for code, _ in COLORS}
    frame = bytearray(geometry.frame_bytes)
    for index in range(0, len(codes), 2):
        left = codes[index]
        right = codes[index + 1]
        if left not in valid or right not in valid:
            raise ToolError(f"invalid palette code at pixel {index}")
        frame[index // 2] = (left << 4) | right
    return bytes(frame)


def build_epdimg(frame: bytes, generation: int | None = None, geometry: Geometry = OFFLINE_GEOMETRY) -> ConvertedImage:
    if len(frame) != geometry.frame_bytes:
        raise ToolError(f"expected {geometry.frame_bytes} frame bytes, got {len(frame)}")
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
        geometry.width,
        geometry.height,
        geometry.frame_bytes,
        crc32,
        generation,
    )
    payload = header + frame
    if len(payload) != geometry.image_bytes:
        raise AssertionError("internal EPDIMG size mismatch")
    return ConvertedImage(payload=payload, generation=generation, crc32=crc32)


def convert_image(
    path: Path,
    *,
    fit: str = "contain",
    dither: str = "floyd-steinberg",
    auto_rotate: bool = True,
    generation: int | None = None,
    geometry: Geometry = OFFLINE_GEOMETRY,
) -> ConvertedImage:
    image = _prepare_rgb(path, fit, auto_rotate, geometry)
    flattened = getattr(image, "get_flattened_data", None)
    pixels = list(flattened() if flattened is not None else image.getdata())
    if dither == "none":
        codes = _codes_without_dither(pixels)
    elif dither == "floyd-steinberg":
        codes = _codes_with_floyd_steinberg(pixels, geometry)
    else:
        raise ToolError(f"unsupported dither mode: {dither}")
    return build_epdimg(pack_codes(codes, geometry), generation, geometry)


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
        self.panel_capabilities = None

    def _request(self, method: str, path: str, body: bytes | None = None, *, encoding: str | None = None) -> dict[str, Any]:
        headers = {"Accept": "application/json"}
        if body is not None:
            headers["Content-Type"] = "application/octet-stream"
            headers["Content-Length"] = str(len(body))
        if encoding:
            headers["Content-Encoding"] = encoding
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
        document = self._request("GET", "/api/epaper")
        self.panel_capabilities = document["data"]
        return document

    def status(self) -> dict[str, Any]:
        return self._request("GET", "/api/epaper/status")

    def upload(self, payload: bytes) -> dict[str, Any]:
        if self.panel_capabilities is None:
            self.capabilities()
        geometry = Geometry.from_capabilities(self.panel_capabilities)
        if len(payload) != geometry.image_bytes:
            raise ToolError(f"upload must be exactly {geometry.image_bytes} logical bytes")
        header = struct.unpack("<8sIIIIIIQ", payload[:HEADER_BYTES])
        if (header[:6] != (MAGIC, VERSION, HEADER_BYTES, geometry.width, geometry.height, geometry.frame_bytes)
                or not header[7] or header[6] != zlib.crc32(payload[HEADER_BYTES:])):
            raise ToolError("invalid EPDIMG header or CRC")
        compressed = gzip.compress(payload, mtime=0)
        if len(compressed) > self.panel_capabilities["image"]["max_compressed_bytes"]:
            raise ToolError("compressed image exceeds device upload limit")
        return self._request("POST", "/api/epaper/image", compressed, encoding="gzip")

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
    parser.add_argument("--width", type=int, help="offline width; supply together with --height")
    parser.add_argument("--height", type=int, help="offline height; supply together with --width")
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
        description="Convert images and control the IOT-Node Dither E-Paper capability-driven API."
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
        client = None
        geometry = OFFLINE_GEOMETRY
        if args.command in ("image", "convert"):
            if (args.width is None) != (args.height is None):
                raise ToolError("--width and --height must be supplied together")
            requested = Geometry(args.width, args.height) if args.width is not None else None
            if args.command == "image" and not args.output_only:
                client = EpaperApiClient(normalize_base_url(args.ip or "", args.port, args.https),
                                        timeout=args.timeout, token=args.token)
                geometry = Geometry.from_capabilities(client.capabilities()["data"])
                if requested is not None and requested != geometry:
                    raise ToolError("requested dimensions do not match the device capability")
            else:
                geometry = requested or OFFLINE_GEOMETRY
            converted = convert_image(
                args.image,
                fit=args.fit,
                dither=args.dither,
                auto_rotate=args.auto_rotate,
                generation=args.generation,
                geometry=geometry,
            )
            output = args.output
            if args.command == "convert" and output is None:
                output = args.image.with_suffix(".epd")
            written = _write_output(converted, output, args.image)
            summary = {
                "source": str(args.image),
                "output": str(written) if written else None,
                "upload_bytes": len(converted.payload),
                "frame_bytes": geometry.frame_bytes,
                "generation": str(converted.generation),
                "crc32": f"{converted.crc32:08X}",
            }
            if args.command == "convert" or args.output_only:
                _print_json(summary)
                return 0

        base_url = normalize_base_url(args.ip or "", args.port, args.https)
        client = client or EpaperApiClient(base_url, timeout=args.timeout, token=args.token)
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
