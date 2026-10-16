#!/usr/bin/env python3
"""Build a timestamped static release folder for server or device storage."""

from __future__ import annotations

import argparse
from datetime import datetime
import gzip
import json
import re
import shutil
import sys
from pathlib import Path
from urllib.parse import quote


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
# Bundling merges what index.html loads into a few files so HTTP/device serving needs only a
# handful of requests. Source keeps one classic script per module; script-loader.js and
# svg-icons.js read the generated DitherApp.bundle registry. Help stays lazy in its own bundle.
APP_SCRIPT_BUNDLE = "bundle/app.js"
APP_STYLE_BUNDLE = "bundle/app.css"
HELP_SCRIPT_BUNDLE = "bundle/help.js"
HELP_SCRIPT_DIR = "src/pages/help/"
WORKER_SCRIPT = "src/pages/dither-editor/worker/dither-worker.js"
ICON_DIR = "assets/icons"
STYLESHEET_TAG_RE = re.compile(r'(?P<indent>[ \t]*)<link\b[^>]*\brel="stylesheet"[^>]*\bhref="(?P<path>[^"]+)"[^>]*>(?P<newline>\r?\n)?')
SCRIPT_TAG_RE = re.compile(r'(?P<indent>[ \t]*)<script\b[^>]*\bsrc="(?P<path>[^"]+)"[^>]*></script>(?P<newline>\r?\n)?')
# Wrapped modules run inside a function, so each must be one IIFE without top-level declarations.
SINGLE_IIFE_RE = re.compile(r"^\(function\b.*\}\)(?:\([\w.]*\)|\.call\(this\));?$", re.DOTALL)
IMPORT_SCRIPTS_RE = re.compile(r"importScripts\((?P<args>[^)]*)\);?")


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
    bundled_files = bundle_output(output_dir)
    gzipped_files = gzip_output_files(output_dir) if args.gzip else []
    shutil.copytree(output_dir, latest_dir)

    print(f"Build mode: {'demo' if args.demo else 'release'}")
    print(f"Build output: {display_path(output_dir)}")
    print(f"Latest mirror: {display_path(latest_dir)}")
    print(f"Copied files: {copied_files}")
    print(f"Minify: {'enabled' if args.minify else 'disabled'}")
    print(f"Minified files: {minified_files}")
    print(f"Bundled files: {bundled_files}")
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
    # Preserve lexical islands and token boundaries, including CSS math whitespace.
    # Comments stay intact: deleting them can join tokens or change selectors.
    tokens = re.findall(r"/\*.*?\*/|\"(?:\\.|[^\"\\])*\"|'(?:\\.|[^'\\])*'|\\[\s\S]|\s+|[^\s]", text, re.DOTALL)
    return "".join(" " if token.isspace() else token for token in tokens).strip() + "\n"


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


def bundle_output(output_dir: Path) -> int:
    """Merge the scripts, stylesheets and SVG icons index.html needs; returns merged file count."""
    index_path = output_dir / "index.html"
    index = index_path.read_text(encoding="utf-8")
    styles = [match.group("path") for match in STYLESHEET_TAG_RE.finditer(index)]
    entries = [match.group("path") for match in SCRIPT_TAG_RE.finditer(index)]
    if not styles and not entries:
        return 0
    not_deferred = [m.group("path") for m in SCRIPT_TAG_RE.finditer(index) if not re.search(r"\sdefer\b", m.group(0))]
    if not_deferred:
        raise SystemExit(f"index.html scripts must be deferred to be bundled: {not_deferred}")

    def read(relative: str) -> str:
        return (output_dir / relative).read_text(encoding="utf-8")

    sources = sorted(path.relative_to(output_dir).as_posix() for path in (output_dir / "src").rglob("*.js"))
    lazy = [path for path in sources if path.startswith(HELP_SCRIPT_DIR) and path not in entries]
    wrapped = [path for path in sources if path not in entries and path not in lazy and path != WORKER_SCRIPT]
    for path in wrapped + lazy:
        if not SINGLE_IIFE_RE.match(strip_comments(read(path)).strip()):
            raise SystemExit(f"{path} must be a single IIFE to be bundled.")
    icons = {
        path.relative_to(output_dir).as_posix(): svg_data_url(path.read_text(encoding="utf-8"))
        for path in sorted((output_dir / ICON_DIR).rglob("*.svg"))
    }
    inline_worker_imports(output_dir)

    def module(path: str) -> str:
        return f"{json.dumps(path)}:function(){{\n{read(path)}\n}}"

    registry = [f"{json.dumps(path)}:true" for path in entries] + [module(path) for path in wrapped]
    app_script = (
        "window.DitherApp=window.DitherApp||{};window.DitherApp.bundle={scripts:{\n"
        + ",\n".join(registry)
        + "\n},files:" + json.dumps({path: HELP_SCRIPT_BUNDLE for path in lazy})
        + ",icons:" + json.dumps(icons) + "};\n"
        + "".join(read(path) + "\n;\n" for path in entries)
    )
    bundle_dir = output_dir / "bundle"
    bundle_dir.mkdir(exist_ok=True)
    (output_dir / APP_SCRIPT_BUNDLE).write_text(app_script, encoding="utf-8", newline="\n")
    if lazy:
        help_script = "(function(scripts){\n" + "".join(
            f"scripts[{json.dumps(path)}]=function(){{\n{read(path)}\n}};\n" for path in lazy
        ) + "})(window.DitherApp.bundle.scripts);\n"
        (output_dir / HELP_SCRIPT_BUNDLE).write_text(help_script, encoding="utf-8", newline="\n")
    style = "\n".join(read(path) for path in styles)
    (output_dir / APP_STYLE_BUNDLE).write_text(style, encoding="utf-8", newline="\n")

    index = replace_first_tag(STYLESHEET_TAG_RE, f'<link rel="stylesheet" href="{APP_STYLE_BUNDLE}">', index)
    index = replace_first_tag(SCRIPT_TAG_RE, f'<script defer src="{APP_SCRIPT_BUNDLE}"></script>', index)
    merged = set(entries + wrapped + lazy + styles) | set(icons)
    referenced = re.findall(r'(?:src|href)="([^"]+)"', index) + re.findall(r"url\(['\"]?([^'\")]+)", style)
    stale = sorted(path for path in referenced if path in merged)
    if stale:
        raise SystemExit(f"Bundled files are still referenced directly: {stale}")
    index_path.write_text(index, encoding="utf-8", newline="\n")
    for path in merged:
        (output_dir / path).unlink()
    for directory in sorted((path for path in output_dir.rglob("*") if path.is_dir()), reverse=True):
        if not any(directory.iterdir()):
            directory.rmdir()
    return len(merged)


def replace_first_tag(pattern: re.Pattern[str], tag: str, text: str) -> str:
    replaced = False

    def substitute(match: re.Match[str]) -> str:
        nonlocal replaced
        if replaced:
            return ""
        replaced = True
        return match.group("indent") + tag + (match.group("newline") or "")

    return pattern.sub(substitute, text)


def svg_data_url(text: str) -> str:
    return "data:image/svg+xml;charset=utf-8," + quote(text.strip(), safe="=:/;,'()!*-._~@$&+?")


def inline_worker_imports(output_dir: Path) -> None:
    """Inline the worker's importScripts dependencies so it starts with one request."""
    worker = output_dir / WORKER_SCRIPT
    if not worker.is_file():
        return
    text = worker.read_text(encoding="utf-8")
    matches = list(IMPORT_SCRIPTS_RE.finditer(text))
    if len(matches) != 1:
        raise SystemExit(f"{WORKER_SCRIPT} must contain exactly one importScripts(...) call to be bundled.")
    match = matches[0]
    dependencies = re.findall(r"""['"]([^'"]+)['"]""", match.group("args"))
    if not dependencies:
        raise SystemExit(f"{WORKER_SCRIPT} importScripts(...) must list literal script paths.")
    inlined = "".join(
        (worker.parent / dependency).resolve().read_text(encoding="utf-8") + "\n;\n" for dependency in dependencies
    )
    worker.write_text(text[: match.start()] + inlined + text[match.end():], encoding="utf-8", newline="\n")


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
