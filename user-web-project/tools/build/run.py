#!/usr/bin/env python3
"""Build a timestamped static release folder for server or device storage."""

from __future__ import annotations

import argparse
from datetime import datetime
import gzip
import re
import shutil
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "shared"))

from demo_assets import generated_demo_data_paths  # noqa: E402


DEFAULT_OUTPUT = ROOT / "build"
COPY_PATHS = ("index.html", "assets", "src", "LICENSE")
EXCLUDE_PATHS = generated_demo_data_paths()
MINIFY_SUFFIXES = {".html", ".css", ".js", ".svg"}
# Preview block in index.html loads the dev/demo mock device API. Release builds
# strip the block and exclude the mock file; demo builds keep both.
MOCK_SCRIPT_PATH = Path("src") / "device" / "device-mock.js"
PREVIEW_BLOCK_RE = re.compile(
    r"[ \t]*<!-- PREVIEW START.*?PREVIEW END -->\r?\n?", re.DOTALL
)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Copy static app files into a timestamped build/ folder."
    )
    parser.add_argument(
        "--output",
        default=str(DEFAULT_OUTPUT),
        help="Output root directory. Defaults to ./build.",
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Remove the output root directory before creating this build.",
    )
    parser.add_argument(
        "--no-clean",
        action="store_true",
        help=argparse.SUPPRESS,
    )
    parser.add_argument(
        "--minify",
        dest="minify",
        action="store_true",
        default=True,
        help="Minify HTML/CSS/JS/SVG output files. Enabled by default.",
    )
    parser.add_argument(
        "--no-minify",
        dest="minify",
        action="store_false",
        help="Skip minifying output files.",
    )
    parser.add_argument(
        "--gzip",
        dest="gzip",
        action="store_true",
        default=True,
        help="Replace copied output files with .gz versions. Enabled by default.",
    )
    parser.add_argument(
        "--no-gzip",
        dest="gzip",
        action="store_false",
        help="Keep normal copied output files instead of replacing them with .gz versions.",
    )
    parser.add_argument(
        "--demo",
        action="store_true",
        help="Build a static demo that keeps the mock device API. Demo output never gzips.",
    )
    args = parser.parse_args()
    if args.no_clean:
        args.clean = False
    if args.demo:
        # Demo output serves plain static hosting (e.g. GitHub Pages); gzip-only files break it.
        args.gzip = False

    output_root = resolve_output_dir(args.output)
    if output_root == ROOT:
        raise SystemExit("Refusing to use the project root as the build output directory.")

    if output_root.exists() and args.clean:
        shutil.rmtree(output_root)
    output_root.mkdir(parents=True, exist_ok=True)

    # latest is a mirror of the newest build; clear it up front so a failed build
    # never leaves a stale copy behind.
    latest_dir = output_root / "latest"
    if latest_dir.exists():
        shutil.rmtree(latest_dir)

    exclude_paths = EXCLUDE_PATHS if args.demo else EXCLUDE_PATHS | {MOCK_SCRIPT_PATH}
    output_dir = create_build_dir(output_root, "-demo" if args.demo else "")
    copied_files = copy_static_files(output_dir, exclude_paths)
    if not args.demo:
        strip_preview_block(output_dir / "index.html")
    minified_files = minify_files(output_dir) if args.minify else 0
    gzipped_files = gzip_output_files(output_dir) if args.gzip else []
    shutil.copytree(output_dir, latest_dir)

    print(f"Build mode: {'demo' if args.demo else 'release'}")
    print(f"Build output: {display_path(output_dir)}")
    print(f"Latest mirror: {display_path(latest_dir)}")
    print(f"Copied files: {copied_files}")
    print(f"Minify: {'enabled' if args.minify else 'disabled'}")
    print(f"Minified files: {minified_files}")
    print(f"Gzip: {'enabled' if args.gzip else 'disabled'}")
    print(f"Gzip files: {len(gzipped_files)}")
    return 0


def resolve_output_dir(output: str) -> Path:
    path = Path(output)
    if not path.is_absolute():
        path = ROOT / path
    return path.resolve()


def create_build_dir(output_root: Path, name_suffix: str = "") -> Path:
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S") + name_suffix
    output_dir = output_root / stamp
    suffix = 2
    while output_dir.exists():
        output_dir = output_root / f"{stamp}-{suffix:02d}"
        suffix += 1
    output_dir.mkdir(parents=True)
    return output_dir


def display_path(path: Path) -> str:
    return str(path.relative_to(ROOT) if path.is_relative_to(ROOT) else path)


def copy_static_files(output_dir: Path, exclude_paths: set[Path]) -> int:
    copied = 0
    for item in COPY_PATHS:
        source = ROOT / item
        target = output_dir / item
        if source.is_dir():
            for path in source.rglob("*"):
                relative_path = path.relative_to(ROOT)
                if path.is_file() and relative_path not in exclude_paths:
                    copy_file(path, output_dir / relative_path)
                    copied += 1
        elif source.is_file() and Path(item) not in exclude_paths:
            copy_file(source, target)
            copied += 1
    return copied


def strip_preview_block(index_path: Path) -> None:
    text = index_path.read_text(encoding="utf-8")
    stripped = PREVIEW_BLOCK_RE.sub("", text)
    if stripped == text:
        raise SystemExit(
            "index.html preview block not found; release build cannot verify the mock was stripped."
        )
    index_path.write_text(stripped, encoding="utf-8", newline="\n")


def copy_file(source: Path, target: Path) -> None:
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)


def minify_files(output_dir: Path) -> int:
    count = 0
    for path in output_dir.rglob("*"):
        if path.is_file() and path.suffix.lower() in MINIFY_SUFFIXES:
            original = path.read_text(encoding="utf-8")
            minified = minify_text(original, path.suffix.lower())
            path.write_text(minified, encoding="utf-8", newline="\n")
            count += 1
    return count


def minify_text(text: str, suffix: str) -> str:
    if suffix == ".html":
        return minify_html(text)
    if suffix == ".css":
        return minify_css(text)
    if suffix == ".js":
        return minify_js(text)
    if suffix == ".svg":
        return minify_svg(text)
    return text


def minify_html(text: str) -> str:
    text = re.sub(r"<!--(?!\[if).*?-->", "", text, flags=re.DOTALL)
    text = re.sub(r">\s+<", "><", text)
    text = re.sub(r"\s{2,}", " ", text)
    return text.strip() + "\n"


def minify_css(text: str) -> str:
    text = strip_comments(text)
    text = re.sub(r"\s+", " ", text)
    text = re.sub(r"\s*([{}:;,>+~])\s*", r"\1", text)
    text = re.sub(r";}", "}", text)
    return text.strip() + "\n"


def minify_js(text: str) -> str:
    text = strip_comments(text)
    lines = [line.strip() for line in text.splitlines()]
    return "\n".join(line for line in lines if line) + "\n"


def minify_svg(text: str) -> str:
    text = strip_comments(text)
    text = re.sub(r">\s+<", "><", text)
    text = re.sub(r"\s{2,}", " ", text)
    return text.strip() + "\n"


def strip_comments(text: str) -> str:
    result: list[str] = []
    i = 0
    quote = ""
    escape = False
    length = len(text)

    while i < length:
        char = text[i]
        next_char = text[i + 1] if i + 1 < length else ""

        if quote:
            result.append(char)
            if escape:
                escape = False
            elif char == "\\":
                escape = True
            elif char == quote:
                quote = ""
            i += 1
            continue

        if char in ("'", '"', "`"):
            quote = char
            result.append(char)
            i += 1
            continue

        if char == "/" and next_char == "*":
            i += 2
            while i + 1 < length and not (text[i] == "*" and text[i + 1] == "/"):
                i += 1
            i += 2
            continue

        if char == "/" and next_char == "/":
            i += 2
            while i < length and text[i] not in "\r\n":
                i += 1
            continue

        result.append(char)
        i += 1

    return "".join(result)


def gzip_output_files(output_dir: Path) -> list[Path]:
    gzipped: list[Path] = []
    for path in sorted(output_dir.rglob("*")):
        if not path.is_file() or path.suffix.lower() == ".gz":
            continue
        gzip_path = path.with_name(path.name + ".gz")
        with path.open("rb") as source_file:
            data = source_file.read()
        gzip_path.write_bytes(gzip.compress(data, compresslevel=9, mtime=0))
        path.unlink()
        gzipped.append(gzip_path)
    return gzipped


if __name__ == "__main__":
    raise SystemExit(main())
