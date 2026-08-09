#!/usr/bin/env python3
"""Build, snapshot, verify, package, and flash ESP32 release images."""

from __future__ import annotations

import argparse
import configparser
import gzip
import hashlib
import io
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
import tarfile
import tempfile
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo


ROOT = Path(__file__).resolve().parents[2]
BUILD_ROOT = ROOT / "build"
LATEST_ROOT = BUILD_ROOT / "latest"
WEB_OUTPUT = LATEST_ROOT / "web"
WEB_MANIFEST_PATH = LATEST_ROOT / "web-manifest.json"
WORK_ROOT = BUILD_ROOT / ".work"
GENERATED_OUTPUT = WORK_ROOT / "esp-generated"
EMBEDDED_WEB_HEADER = GENERATED_OUTPUT / "EmbeddedWebAssets.generated.h"
PLATFORMIO_CONFIG = ROOT / "platformio.ini"
VERSION_PATH = ROOT / "VERSION"
SUPPORTED_ENVIRONMENT = "firebeetle2_esp32c6"
SUPPORTED_BOARD = "dfrobot_firebeetle2_esp32c6"
SUPPORTED_CHIP = "esp32c6"
SUPPORTED_UPLOAD_BAUD = 460800
SUPPORTED_FLASH_MODE = "dio"
SUPPORTED_FLASH_FREQUENCY = "80m"
SUPPORTED_FLASH_SIZE = "detect"
MANIFEST_SCHEMA = 5
PRODUCT = "iot-node-bedrock"
APP_OFFSET = 0x10000
APP_SIZE = 0x1F0000
USER_DATA_OFFSET = 0x200000
USER_DATA_SIZE = 0x1E8000
USER_NVS_OFFSET = 0x3E8000
USER_NVS_SIZE = 0x8000
IMAGE_NAMES = (
    "bootloader.bin",
    "partitions.bin",
    "boot_app0.bin",
    "firmware.bin",
)
PACKAGE_NAMES = ("manifest.json", *IMAGE_NAMES)
SNAPSHOT_PATTERN = re.compile(r"^\d{8}_\d{4}(?:_\d{2})?$")
TAIPEI = ZoneInfo("Asia/Taipei")


class ReleaseError(RuntimeError):
    """A release input or artifact is invalid."""


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Build, verify, or flash firmware snapshots under build/."
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    build_parser = subparsers.add_parser(
        "build",
        help="Build firmware from existing production build/latest/web.",
    )
    add_pio_options(build_parser)

    verify_parser = subparsers.add_parser(
        "verify",
        help="Verify a complete latest or historical snapshot.",
    )
    verify_parser.add_argument(
        "--image",
        help="firmware.img to verify (default: build/latest/firmware.img).",
    )

    clean_parser = subparsers.add_parser(
        "clean",
        help="Remove latest/transient outputs; preserve timestamp snapshots.",
    )
    clean_parser.add_argument(
        "--all",
        action="store_true",
        help="Also remove all timestamp snapshots.",
    )

    flash_parser = subparsers.add_parser(
        "flash",
        help="Flash discrete binaries belonging to a verified firmware.img.",
    )
    flash_parser.add_argument("--pio", default="pio", help="PlatformIO command.")
    flash_parser.add_argument("--port", required=True, help="Confirmed serial port.")
    flash_parser.add_argument(
        "--image",
        help="firmware.img to flash (default: build/latest/firmware.img).",
    )

    args = parser.parse_args()
    try:
        if args.command == "build":
            build_release(args.pio, args.environment)
        elif args.command == "verify":
            verify_release(args.image)
        elif args.command == "clean":
            clean_outputs(args.all)
        elif args.command == "flash":
            flash_release(args.pio, args.port, args.image)
    except (
        ReleaseError,
        OSError,
        subprocess.CalledProcessError,
        json.JSONDecodeError,
        tarfile.TarError,
    ) as error:
        print(f"release-build: {error}", file=sys.stderr)
        return 1
    return 0


def add_pio_options(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--pio", default="pio", help="PlatformIO command.")
    parser.add_argument(
        "--environment",
        default=SUPPORTED_ENVIRONMENT,
        help=f"PlatformIO environment. Supported: {SUPPORTED_ENVIRONMENT}.",
    )


def build_release(pio: str, environment: str) -> Path:
    require_supported_environment(environment)
    web_manifest = require_web_input()
    dirty = git_dirty()
    if os.environ.get("CI") and dirty:
        raise ReleaseError("official CI builds require a clean Git worktree")

    clean_work()
    stage = Path(tempfile.mkdtemp(prefix=".release-stage-", dir=BUILD_ROOT))
    try:
        generate_embedded_web_header(web_manifest)
        run_command([*pio_command(pio), "run", "-e", environment])

        shutil.copytree(WEB_OUTPUT, stage / "web", copy_function=shutil.copy2)
        shutil.copy2(WEB_MANIFEST_PATH, stage / "web-manifest.json")
        build_id = next_build_id()
        collect_images(
            pio,
            environment,
            stage / "binary",
            web_manifest,
            build_id,
            dirty,
        )
        create_firmware_package(stage / "binary", stage / "firmware.img")
        verify_snapshot(stage, stage / "firmware.img")
        timestamp_root = publish_snapshot(stage, build_id)
    except Exception:
        if stage.exists():
            shutil.rmtree(stage)
        raise
    finally:
        clean_work()

    print(f"Timestamp snapshot: {display_path(timestamp_root)}")
    print(f"Latest snapshot: {display_path(LATEST_ROOT)}")
    print(f"Firmware package: {display_path(LATEST_ROOT / 'firmware.img')}")
    return timestamp_root


def require_web_input() -> dict:
    if not WEB_OUTPUT.is_dir() or not WEB_MANIFEST_PATH.is_file():
        raise ReleaseError(
            "processed web input is missing; run make web before make esp"
        )
    try:
        manifest = json.loads(WEB_MANIFEST_PATH.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        raise ReleaseError("web-manifest.json contains invalid JSON") from error
    if manifest.get("schema") != 1:
        raise ReleaseError("unsupported web manifest schema")
    if manifest.get("target") != "production":
        raise ReleaseError("make esp requires production web; demo cannot be embedded")
    validated_web_identity(manifest)
    if manifest.get("web_sha256") != tree_sha256(WEB_OUTPUT):
        raise ReleaseError("build/latest/web no longer matches web-manifest.json")
    if manifest.get("web") == "none":
        if manifest.get("web_process") != "none" or any(
            path.is_file() for path in WEB_OUTPUT.rglob("*")
        ):
            raise ReleaseError("WEB=none must use an empty unprocessed web output")
    elif not has_logical_index(WEB_OUTPUT):
        raise ReleaseError("build/latest/web has no logical index.html")
    return manifest


def validated_web_identity(manifest: dict) -> tuple[str, str | None]:
    source = manifest.get("web")
    sha256_value = manifest.get("web_sha256")
    if source not in ("builtin", "user", "none"):
        raise ReleaseError("web manifest has an invalid source")
    if not isinstance(sha256_value, str) or not re.fullmatch(
        r"[0-9a-f]{64}", sha256_value
    ):
        raise ReleaseError("web manifest has an invalid SHA-256")
    return source, None if source == "none" else sha256_value


def generate_embedded_web_header(web_manifest: dict) -> None:
    source, sha256_value = validated_web_identity(web_manifest)
    if source == "none":
        if any(path.is_file() for path in WEB_OUTPUT.rglob("*")):
            raise ReleaseError("WEB=none must not generate embedded web assets")
        records = []
    else:
        records = embedded_web_records(WEB_OUTPUT)
    lines = [
        "#pragma once",
        "",
        "// Generated from build/latest/web by tools/release-build/build_release.py.",
        "// Do not edit this file directly.",
        "namespace EmbeddedWebGenerated {",
        "",
        f'constexpr char kWebSource[] = "{source}";',
        (
            "constexpr const char *kWebSha256 = nullptr;"
            if sha256_value is None
            else f'constexpr char kWebSha256[] = "{sha256_value}";'
        ),
    ]
    payload_bytes = 0
    for index, record in enumerate(records):
        symbol = f"kAssetBytes{index}"
        content = record["path"].read_bytes()
        record["symbol"] = symbol
        record["size"] = len(content)
        payload_bytes += len(content)
        lines.extend(
            [
                "",
                f"alignas(4) const uint8_t {symbol}[] PROGMEM = {{",
                *cpp_byte_lines(content),
                "};",
            ]
        )

    if records:
        lines.extend(["", "const EmbeddedWebAsset kAssets[] = {"])
        for record in records:
            lines.append(
                "    "
                f'{{"{record["request_path"]}", "{record["content_type"]}", '
                f'"{record["content_encoding"]}", {record["symbol"]}, '
                f'{record["size"]}}},'
            )
        lines.append("};")
    else:
        lines.extend(["", "const EmbeddedWebAsset kAssets[1] = {};"])
    lines.extend(
        [
            f"constexpr size_t kAssetCount = {len(records)};",
            f"constexpr size_t kPayloadBytes = {payload_bytes};",
            "}  // namespace EmbeddedWebGenerated",
            "",
        ]
    )
    GENERATED_OUTPUT.mkdir(parents=True, exist_ok=True)
    with EMBEDDED_WEB_HEADER.open("w", encoding="utf-8", newline="\n") as output:
        output.write("\n".join(lines))


def embedded_web_records(root: Path) -> list[dict]:
    records: list[dict] = []
    logical_paths: set[str] = set()
    for path in sorted(item for item in root.rglob("*") if item.is_file()):
        relative = path.relative_to(root).as_posix()
        compressed = relative.endswith(".gz")
        logical = relative.removesuffix(".gz") if compressed else relative
        if logical in logical_paths:
            raise ReleaseError(f"web input has raw/gzip collision for {logical}")
        if compressed:
            try:
                gzip.decompress(path.read_bytes())
            except (EOFError, OSError) as error:
                raise ReleaseError(f"web input has invalid gzip file: {relative}") from error
        logical_paths.add(logical)
        request_path = "/" + logical
        records.append(
            {
                "path": path,
                "request_path": request_path,
                "content_type": web_content_type(request_path),
                "content_encoding": "gzip" if compressed else "",
            }
        )
    if "index.html" not in logical_paths:
        raise ReleaseError("web input has no logical index.html")
    return records


def cpp_byte_lines(content: bytes) -> list[str]:
    return [
        "    "
        + ", ".join(f"0x{byte:02x}" for byte in content[index : index + 16])
        + ","
        for index in range(0, len(content), 16)
    ]


def web_content_type(path: str) -> str:
    suffix = Path(path).suffix.lower()
    return {
        ".html": "text/html; charset=utf-8",
        ".css": "text/css; charset=utf-8",
        ".js": "application/javascript; charset=utf-8",
        ".mjs": "application/javascript; charset=utf-8",
        ".json": "application/json; charset=utf-8",
        ".map": "application/json; charset=utf-8",
        ".txt": "text/plain; charset=utf-8",
        ".xml": "application/xml; charset=utf-8",
        ".svg": "image/svg+xml",
        ".ico": "image/x-icon",
        ".png": "image/png",
        ".jpg": "image/jpeg",
        ".jpeg": "image/jpeg",
        ".gif": "image/gif",
        ".webp": "image/webp",
        ".avif": "image/avif",
        ".bmp": "image/bmp",
        ".woff": "font/woff",
        ".woff2": "font/woff2",
        ".ttf": "font/ttf",
        ".otf": "font/otf",
        ".eot": "application/vnd.ms-fontobject",
        ".wasm": "application/wasm",
        ".mp3": "audio/mpeg",
        ".ogg": "audio/ogg",
        ".wav": "audio/wav",
        ".mp4": "video/mp4",
        ".webm": "video/webm",
        ".pdf": "application/pdf",
    }.get(suffix, "application/octet-stream")


def collect_images(
    pio: str,
    environment: str,
    binary_output: Path,
    web_manifest: dict,
    build_id: str,
    dirty: bool,
) -> dict:
    core_dir = platformio_core_dir(pio)
    framework_dir = core_dir / "packages/framework-arduinoespressif32"
    pio_build_dir = ROOT / ".pio/build" / environment
    config = environment_config(environment)
    partition_csv = resolve_partition_csv(framework_dir, config["partitions"])
    partitions = parse_partitions(partition_csv)
    app_partition = find_partition(partitions, subtype="ota_0")
    user_data_partition = find_named_partition(partitions, "userdata")
    user_nvs_partition = find_named_partition(partitions, "user_nvs")
    validate_partition_layout(
        app_partition,
        user_data_partition,
        user_nvs_partition,
    )

    sources = {
        "bootloader.bin": pio_build_dir / "bootloader.bin",
        "partitions.bin": pio_build_dir / "partitions.bin",
        "boot_app0.bin": framework_dir / "tools/partitions/boot_app0.bin",
        "firmware.bin": pio_build_dir / "firmware.bin",
    }
    for source in sources.values():
        if not source.is_file():
            raise ReleaseError(f"required image is missing: {source}")
    if sources["firmware.bin"].stat().st_size > int(app_partition["size"]):
        raise ReleaseError("firmware image exceeds the application partition")

    binary_output.mkdir(parents=True)
    for name, source in sources.items():
        shutil.copy2(source, binary_output / name)

    board = load_board_definition(core_dir, config["board"])
    flash_mode = str(board["build"].get("flash_mode", "dio"))
    if flash_mode in ("qio", "qout"):
        flash_mode = "dio"
    flash_frequency = normalize_frequency(board["build"].get("f_flash", "80000000L"))
    image_layout = (
        ("0x0", "bootloader.bin"),
        ("0x8000", "partitions.bin"),
        ("0xe000", "boot_app0.bin"),
        (hex(int(app_partition["offset"])), "firmware.bin"),
    )
    images = [
        {
            "offset": offset,
            "file": name,
            "size": (binary_output / name).stat().st_size,
            "sha256": sha256(binary_output / name),
        }
        for offset, name in image_layout
    ]

    version = read_version()
    manifest = {
        "schema": MANIFEST_SCHEMA,
        "product": PRODUCT,
        "version": version,
        "release": f"v{version}",
        "build_id": build_id,
        "created_at": datetime.now(TAIPEI).isoformat(),
        "git_commit": git_commit(),
        "git_dirty": dirty,
        "environment": environment,
        "board": config["board"],
        "chip": str(board["build"]["mcu"]).lower(),
        "upload_baud": int(board["upload"].get("speed", 460800)),
        "flash_mode": flash_mode,
        "flash_frequency": flash_frequency,
        "flash_size": "detect",
        "app_partition": app_partition["name"],
        "app_offset": app_partition["offset"],
        "app_size": app_partition["size"],
        "firmware_size": sources["firmware.bin"].stat().st_size,
        "app_available_size": (
            int(app_partition["size"]) - sources["firmware.bin"].stat().st_size
        ),
        "frontend_delivery": (
            "none" if web_manifest["web"] == "none" else "embedded"
        ),
        "frontend_file_count": web_manifest["file_count"],
        "frontend_payload_size": web_manifest["payload_bytes"],
        "web": web_manifest["web"],
        "web_process": web_manifest["web_process"],
        "web_sha256": web_manifest["web_sha256"],
        "user_data_partition": user_data_partition["name"],
        "user_data_offset": user_data_partition["offset"],
        "user_data_size": user_data_partition["size"],
        "user_data_reserve_bytes": 64 * 1024,
        "user_nvs_partition": user_nvs_partition["name"],
        "user_nvs_offset": user_nvs_partition["offset"],
        "user_nvs_size": user_nvs_partition["size"],
        "images": images,
    }
    if web_manifest.get("user_web_sha256"):
        manifest["user_web_sha256"] = web_manifest["user_web_sha256"]
    write_json(binary_output / "manifest.json", manifest)
    return manifest


def validate_partition_layout(
    app_partition: dict[str, int | str],
    user_data_partition: dict[str, int | str],
    user_nvs_partition: dict[str, int | str],
) -> None:
    if (
        app_partition["name"] != "app0"
        or app_partition["offset"] != APP_OFFSET
        or app_partition["size"] != APP_SIZE
    ):
        raise ReleaseError("application partition must consume 0x10000-0x200000")
    if user_data_partition["subtype"] != "spiffs":
        raise ReleaseError(
            "user-data partition must use the LittleFS-compatible spiffs subtype"
        )
    if (
        user_data_partition["offset"] != USER_DATA_OFFSET
        or user_data_partition["size"] != USER_DATA_SIZE
    ):
        raise ReleaseError("userdata partition must consume 0x200000-0x3e8000")
    if (
        user_nvs_partition["subtype"] != "nvs"
        or user_nvs_partition["offset"] != USER_NVS_OFFSET
        or user_nvs_partition["size"] != USER_NVS_SIZE
    ):
        raise ReleaseError("user_nvs partition must consume 0x3e8000-0x3f0000")


def create_firmware_package(binary_root: Path, image_path: Path) -> None:
    for name in PACKAGE_NAMES:
        if not (binary_root / name).is_file():
            raise ReleaseError(f"firmware package input is missing: {name}")
    unexpected = sorted(
        path.name
        for path in binary_root.iterdir()
        if path.is_file() and path.name not in PACKAGE_NAMES
    )
    if unexpected:
        raise ReleaseError(f"unexpected binary package input: {unexpected[0]}")

    with image_path.open("wb") as image:
        with gzip.GzipFile(
            filename="",
            mode="wb",
            compresslevel=9,
            fileobj=image,
            mtime=0,
        ) as compressed:
            with tarfile.open(
                fileobj=compressed,
                mode="w|",
                format=tarfile.USTAR_FORMAT,
            ) as archive:
                for name in PACKAGE_NAMES:
                    content = (binary_root / name).read_bytes()
                    information = tarfile.TarInfo(name)
                    information.size = len(content)
                    information.mode = 0o644
                    information.mtime = 0
                    information.uid = 0
                    information.gid = 0
                    information.uname = ""
                    information.gname = ""
                    archive.addfile(information, io.BytesIO(content))


def verify_release(image: str | None = None) -> dict:
    image_path = resolve_image_path(image)
    snapshot_root = image_path.parent
    manifest = verify_snapshot(snapshot_root, image_path)
    print(f"Verified snapshot: {display_path(snapshot_root)}")
    print(f"Verified release images: {len(manifest['images'])}")
    return manifest


def resolve_image_path(image: str | None) -> Path:
    path = Path(image) if image else LATEST_ROOT / "firmware.img"
    if not path.is_absolute():
        path = ROOT / path
    path = path.resolve()
    if path.name != "firmware.img" or not path.is_file():
        raise ReleaseError(
            f"firmware image is missing or not named firmware.img: {display_path(path)}"
        )
    return path


def verify_snapshot(snapshot_root: Path, image_path: Path) -> dict:
    binary_root = snapshot_root / "binary"
    web_root = snapshot_root / "web"
    web_manifest_path = snapshot_root / "web-manifest.json"
    manifest_path = binary_root / "manifest.json"
    if not binary_root.is_dir() or not web_root.is_dir() or not web_manifest_path.is_file():
        raise ReleaseError(
            "full verification requires firmware.img beside binary/, web/, "
            "and web-manifest.json"
        )
    if not manifest_path.is_file():
        raise ReleaseError("snapshot binary/manifest.json is missing")

    manifest = read_json(manifest_path, "release manifest")
    web_manifest = read_json(web_manifest_path, "web manifest")
    verify_manifest_metadata(manifest, web_manifest, web_root)
    verify_images(manifest, binary_root)
    verify_package(image_path, binary_root)
    return manifest


def verify_manifest_metadata(
    manifest: dict,
    web_manifest: dict,
    web_root: Path,
) -> None:
    if manifest.get("schema") != MANIFEST_SCHEMA:
        raise ReleaseError("unsupported release manifest schema")
    if manifest.get("product") != PRODUCT:
        raise ReleaseError("release manifest has an invalid product")
    if (
        manifest.get("environment") != SUPPORTED_ENVIRONMENT
        or manifest.get("board") != SUPPORTED_BOARD
        or manifest.get("chip") != SUPPORTED_CHIP
        or manifest.get("upload_baud") != SUPPORTED_UPLOAD_BAUD
        or manifest.get("flash_mode") != SUPPORTED_FLASH_MODE
        or manifest.get("flash_frequency") != SUPPORTED_FLASH_FREQUENCY
        or manifest.get("flash_size") != SUPPORTED_FLASH_SIZE
    ):
        raise ReleaseError("release manifest has incompatible board or flash settings")
    version = manifest.get("version")
    if not isinstance(version, str) or not re.fullmatch(r"\d+\.\d+\.\d+", version):
        raise ReleaseError("release manifest has an invalid version")
    if manifest.get("release") != f"v{version}":
        raise ReleaseError("release label does not match its version")
    if not SNAPSHOT_PATTERN.fullmatch(str(manifest.get("build_id", ""))):
        raise ReleaseError("release manifest has an invalid build id")
    if web_manifest.get("schema") != 1 or web_manifest.get("target") != "production":
        raise ReleaseError("snapshot web manifest is not production")
    if web_manifest.get("version") != version:
        raise ReleaseError("firmware and web versions do not match")
    if manifest.get("web") != web_manifest.get("web"):
        raise ReleaseError("release web selection does not match web-manifest.json")
    if manifest.get("web_process") != web_manifest.get("web_process"):
        raise ReleaseError("release web processing does not match web-manifest.json")
    source, _ = validated_web_identity(web_manifest)
    user_hash = web_manifest.get("user_web_sha256")
    if source == "user":
        if not isinstance(user_hash, str) or manifest.get("user_web_sha256") != user_hash:
            raise ReleaseError("user web source hash does not match its manifests")
    elif "user_web_sha256" in web_manifest or "user_web_sha256" in manifest:
        raise ReleaseError("non-user web manifest must not record a user source hash")
    web_hash = tree_sha256(web_root)
    if (
        web_manifest.get("web_sha256") != web_hash
        or manifest.get("web_sha256") != web_hash
    ):
        raise ReleaseError("snapshot web no longer matches its manifests")
    files = [path for path in web_root.rglob("*") if path.is_file()]
    payload = sum(path.stat().st_size for path in files)
    if (
        web_manifest.get("file_count") != len(files)
        or manifest.get("frontend_file_count") != len(files)
        or web_manifest.get("payload_bytes") != payload
        or manifest.get("frontend_payload_size") != payload
    ):
        raise ReleaseError("snapshot web counts no longer match its manifests")
    if source == "none":
        if web_manifest.get("web_process") != "none" or files:
            raise ReleaseError("WEB=none snapshot must contain an empty unprocessed web output")
        frontend_delivery = "none"
    elif not has_logical_index(web_root):
        raise ReleaseError("snapshot web has no logical index.html")
    else:
        frontend_delivery = "embedded"

    app_start = manifest.get("app_offset")
    app_size = manifest.get("app_size")
    firmware_size = manifest.get("firmware_size")
    if (
        manifest.get("app_partition") != "app0"
        or app_start != APP_OFFSET
        or app_size != APP_SIZE
        or not isinstance(firmware_size, int)
        or firmware_size <= 0
        or firmware_size > app_size
        or manifest.get("app_available_size") != app_size - firmware_size
        or manifest.get("frontend_delivery") != frontend_delivery
    ):
        raise ReleaseError("release manifest has an invalid application layout")
    if (
        manifest.get("user_data_partition") != "userdata"
        or manifest.get("user_data_offset") != USER_DATA_OFFSET
        or manifest.get("user_data_size") != USER_DATA_SIZE
        or manifest.get("user_data_reserve_bytes") != 64 * 1024
    ):
        raise ReleaseError("release manifest has an invalid user-data layout")
    if (
        manifest.get("user_nvs_partition") != "user_nvs"
        or manifest.get("user_nvs_offset") != USER_NVS_OFFSET
        or manifest.get("user_nvs_size") != USER_NVS_SIZE
    ):
        raise ReleaseError("release manifest has an invalid user NVS layout")


def verify_images(manifest: dict, binary_root: Path) -> None:
    images = manifest.get("images")
    if not isinstance(images, list) or len(images) != len(IMAGE_NAMES):
        raise ReleaseError("release manifest has an invalid image list")
    if {image.get("file") for image in images} != set(IMAGE_NAMES):
        raise ReleaseError("release manifest has unexpected image names")
    allowed_files = set(PACKAGE_NAMES)
    actual_files = {path.name for path in binary_root.iterdir() if path.is_file()}
    if actual_files != allowed_files:
        raise ReleaseError("binary directory has missing or unexpected files")

    user_data_end = USER_DATA_OFFSET + USER_DATA_SIZE
    user_nvs_end = USER_NVS_OFFSET + USER_NVS_SIZE
    previous_end = 0
    firmware_found = False
    for image in images:
        name = image.get("file", "")
        path = binary_root / name
        if not name or path.parent != binary_root or not path.is_file():
            raise ReleaseError(f"release image is missing or unsafe: {name}")
        if path.stat().st_size != image.get("size"):
            raise ReleaseError(f"release image size mismatch: {name}")
        if sha256(path) != image.get("sha256"):
            raise ReleaseError(f"release image hash mismatch: {name}")
        try:
            offset = int(str(image.get("offset", "")), 0)
        except ValueError as error:
            raise ReleaseError(f"release image has an invalid offset: {name}") from error
        if offset < previous_end:
            raise ReleaseError(f"release images overlap at {name}")
        image_end = offset + path.stat().st_size
        if name == "littlefs.bin":
            raise ReleaseError("release must not contain a standalone frontend filesystem")
        if name == "firmware.bin":
            firmware_found = (
                offset == APP_OFFSET
                and path.stat().st_size == manifest.get("firmware_size")
            )
        if offset < user_data_end and image_end > USER_DATA_OFFSET:
            raise ReleaseError(f"release image would overwrite user data: {name}")
        if offset < user_nvs_end and image_end > USER_NVS_OFFSET:
            raise ReleaseError(f"release image would overwrite user settings: {name}")
        previous_end = image_end
    if not firmware_found:
        raise ReleaseError("release firmware does not match the application layout")


def verify_package(image_path: Path, binary_root: Path) -> None:
    with tarfile.open(image_path, "r:gz") as archive:
        members = archive.getmembers()
        names = [member.name for member in members]
        if names != list(PACKAGE_NAMES):
            raise ReleaseError(
                "firmware.img must contain only flat tar.gz manifest and binary entries"
            )
        for member, name in zip(members, PACKAGE_NAMES):
            if not member.isfile():
                raise ReleaseError(f"firmware.img entry is not a regular file: {name}")
            if (
                member.mode != 0o644
                or member.mtime != 0
                or member.uid != 0
                or member.gid != 0
                or member.uname
                or member.gname
                or member.pax_headers
            ):
                raise ReleaseError(
                    f"firmware.img entry has non-deterministic metadata: {name}"
                )
            packaged = archive.extractfile(member)
            if packaged is None or packaged.read() != (binary_root / name).read_bytes():
                raise ReleaseError(f"firmware.img entry does not match binary/{name}")


def flash_release(pio: str, port: str, image: str | None = None) -> None:
    if not port.strip():
        raise ReleaseError("a confirmed serial port is required")
    image_path = resolve_image_path(image)
    snapshot_root = image_path.parent
    manifest = verify_snapshot(snapshot_root, image_path)
    binary_root = snapshot_root / "binary"
    core_dir = platformio_core_dir(pio)
    python_executable = platformio_python(core_dir)
    esptool_module = core_dir / "packages/tool-esptoolpy/esptool/__main__.py"
    if not esptool_module.is_file():
        raise ReleaseError(f"PlatformIO esptool module is missing: {esptool_module}")

    command = [
        str(python_executable),
        "-m",
        "esptool",
        "--chip",
        manifest["chip"],
        "--port",
        port,
        "--baud",
        str(manifest["upload_baud"]),
        "--before",
        "default-reset",
        "--after",
        "hard-reset",
        "write-flash",
        "-z",
        "--flash-mode",
        manifest["flash_mode"],
        "--flash-freq",
        manifest["flash_frequency"],
        "--flash-size",
        manifest["flash_size"],
    ]
    for image_record in manifest["images"]:
        command.extend(
            [
                image_record["offset"],
                str(binary_root / image_record["file"]),
            ]
        )
    run_command(command)


def publish_snapshot(stage: Path, build_id: str) -> Path:
    timestamp_root = BUILD_ROOT / build_id
    if timestamp_root.exists():
        raise ReleaseError(f"snapshot already exists: {display_path(timestamp_root)}")
    os.replace(stage, timestamp_root)

    latest_stage = Path(tempfile.mkdtemp(prefix=".latest-stage-", dir=BUILD_ROOT))
    latest_stage.rmdir()
    shutil.copytree(timestamp_root, latest_stage, copy_function=shutil.copy2)
    replace_directory(latest_stage, LATEST_ROOT)
    return timestamp_root


def replace_directory(source: Path, destination: Path) -> None:
    backup = BUILD_ROOT / ".latest-backup"
    if backup.exists():
        shutil.rmtree(backup)
    had_destination = destination.exists()
    if had_destination:
        os.replace(destination, backup)
    try:
        os.replace(source, destination)
    except Exception:
        if had_destination and backup.exists():
            os.replace(backup, destination)
        raise
    if backup.exists():
        shutil.rmtree(backup)


def next_build_id(now: datetime | None = None) -> str:
    current = now.astimezone(TAIPEI) if now is not None else datetime.now(TAIPEI)
    base = current.strftime("%Y%m%d_%H%M")
    candidate = base
    suffix = 2
    while (BUILD_ROOT / candidate).exists():
        candidate = f"{base}_{suffix:02d}"
        suffix += 1
    return candidate


def clean_work() -> None:
    if WORK_ROOT.exists():
        shutil.rmtree(WORK_ROOT)


def clean_outputs(remove_all: bool = False) -> None:
    if LATEST_ROOT.exists():
        shutil.rmtree(LATEST_ROOT)
    clean_work()
    for legacy_name in ("web", "demo", "esp-image", "esp-generated"):
        legacy_path = BUILD_ROOT / legacy_name
        if legacy_path.is_dir():
            shutil.rmtree(legacy_path)
    if not BUILD_ROOT.is_dir():
        return
    for path in list(BUILD_ROOT.iterdir()):
        if path.name in (".latest-backup",) or path.name.startswith(
            (".web-stage-", ".release-stage-", ".latest-stage-")
        ):
            if path.is_dir():
                shutil.rmtree(path)
            elif path.is_file():
                path.unlink()
        elif remove_all and path.is_dir() and SNAPSHOT_PATTERN.fullmatch(path.name):
            shutil.rmtree(path)
    label = "latest, transient, and timestamp outputs" if remove_all else "latest and transient outputs"
    print(f"Removed {label}; build/.gitignore was preserved")


def environment_config(environment: str) -> dict[str, str]:
    require_supported_environment(environment)
    parser = configparser.ConfigParser(interpolation=None)
    if not parser.read(PLATFORMIO_CONFIG, encoding="utf-8"):
        raise ReleaseError("platformio.ini is missing")
    section = f"env:{environment}"
    if not parser.has_section(section):
        raise ReleaseError(f"PlatformIO environment is missing: {environment}")
    return {
        "board": parser.get(section, "board"),
        "partitions": parser.get(section, "board_build.partitions"),
    }


def require_supported_environment(environment: str) -> None:
    if environment != SUPPORTED_ENVIRONMENT:
        raise ReleaseError(
            f"release layout is only verified for {SUPPORTED_ENVIRONMENT}, got {environment}"
        )


def platformio_core_dir(pio: str) -> Path:
    result = subprocess.run(
        [*pio_command(pio), "system", "info", "--json-output"],
        cwd=ROOT,
        check=True,
        capture_output=True,
        text=True,
    )
    try:
        information = json.loads(result.stdout)
        return Path(information["core_dir"]["value"])
    except (json.JSONDecodeError, KeyError, TypeError) as error:
        raise ReleaseError("PlatformIO did not report a valid core directory") from error


def platformio_python(core_dir: Path) -> Path:
    candidates = (
        core_dir / "penv/bin/python",
        core_dir / "penv/Scripts/python.exe",
    )
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise ReleaseError("PlatformIO Python environment is missing")


def load_board_definition(core_dir: Path, board: str) -> dict:
    path = core_dir / f"platforms/espressif32/boards/{board}.json"
    if not path.is_file():
        raise ReleaseError(f"board definition is missing: {path}")
    return json.loads(path.read_text(encoding="utf-8"))


def resolve_partition_csv(framework_dir: Path, configured: str) -> Path:
    path = Path(configured)
    if not path.suffix:
        path = path.with_suffix(".csv")
    candidates = (ROOT / path, framework_dir / "tools/partitions" / path.name)
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise ReleaseError(f"partition table is missing: {configured}")


def parse_partitions(path: Path) -> list[dict[str, int | str]]:
    partitions: list[dict[str, int | str]] = []
    next_offset = 0
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        tokens = [token.strip() for token in line.split(",")]
        if len(tokens) < 5:
            raise ReleaseError(f"invalid partition row: {raw_line}")
        alignment = 0x10000 if tokens[1] in ("0", "app") else 4
        offset = parse_size(tokens[3]) if tokens[3] else align(next_offset, alignment)
        size = parse_size(tokens[4])
        partitions.append(
            {
                "name": tokens[0],
                "type": tokens[1],
                "subtype": tokens[2],
                "offset": offset,
                "size": size,
            }
        )
        next_offset = offset + size
    return partitions


def find_partition(
    partitions: list[dict[str, int | str]],
    *,
    subtype: str,
) -> dict[str, int | str]:
    for partition in partitions:
        if partition["subtype"] == subtype:
            return partition
    raise ReleaseError(f"partition subtype is missing: {subtype}")


def find_named_partition(
    partitions: list[dict[str, int | str]],
    name: str,
) -> dict[str, int | str]:
    for partition in partitions:
        if partition["name"] == name:
            return partition
    raise ReleaseError(f"partition is missing: {name}")


def parse_size(value: str) -> int:
    normalized = value.strip().lower()
    if normalized.endswith("k"):
        return int(normalized[:-1], 0) * 1024
    if normalized.endswith("m"):
        return int(normalized[:-1], 0) * 1024 * 1024
    return int(normalized, 0)


def align(value: int, alignment: int) -> int:
    return (value + alignment - 1) & ~(alignment - 1)


def normalize_frequency(value: str | int) -> str:
    normalized = str(value).replace("L", "")
    return f"{int(normalized) // 1_000_000}m"


def pio_command(value: str) -> list[str]:
    command = shlex.split(value)
    if not command:
        raise ReleaseError("PlatformIO command is empty")
    return command


def run_command(command: list[str]) -> None:
    subprocess.run(command, cwd=ROOT, check=True)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(64 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def tree_sha256(root: Path) -> str:
    digest = hashlib.sha256()
    files = (item for item in root.rglob("*") if item.is_file())
    for path in sorted(
        files,
        key=lambda item: item.relative_to(root).as_posix().encode("utf-8"),
    ):
        relative = path.relative_to(root).as_posix().encode("utf-8")
        digest.update(len(relative).to_bytes(4, "big"))
        digest.update(relative)
        digest.update(path.stat().st_size.to_bytes(8, "big"))
        with path.open("rb") as source:
            for chunk in iter(lambda: source.read(64 * 1024), b""):
                digest.update(chunk)
    return digest.hexdigest()


def has_logical_index(root: Path) -> bool:
    return (root / "index.html").is_file() or (root / "index.html.gz").is_file()


def read_version() -> str:
    if not VERSION_PATH.is_file():
        raise ReleaseError("VERSION is missing")
    version = VERSION_PATH.read_text(encoding="utf-8").strip()
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        raise ReleaseError(f"VERSION must use MAJOR.MINOR.PATCH, got: {version}")
    return version


def git_commit() -> str:
    result = subprocess.run(
        ["git", "rev-parse", "HEAD"],
        cwd=ROOT,
        check=False,
        capture_output=True,
        text=True,
    )
    return result.stdout.strip() if result.returncode == 0 else "unknown"


def git_dirty() -> bool:
    result = subprocess.run(
        ["git", "status", "--porcelain", "--untracked-files=normal"],
        cwd=ROOT,
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise ReleaseError("Git worktree status could not be determined")
    return bool(result.stdout.strip())


def read_json(path: Path, label: str) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        raise ReleaseError(f"{label} contains invalid JSON") from error
    if not isinstance(value, dict):
        raise ReleaseError(f"{label} must be a JSON object")
    return value


def write_json(path: Path, value: dict) -> None:
    with path.open("w", encoding="utf-8", newline="\n") as output:
        output.write(json.dumps(value, indent=2, sort_keys=True) + "\n")


def display_path(path: Path) -> str:
    try:
        return str(path.relative_to(ROOT))
    except ValueError:
        return str(path)


if __name__ == "__main__":
    raise SystemExit(main())
