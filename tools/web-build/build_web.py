#!/usr/bin/env python3
"""Build and verify the selected frontend under build/latest/web."""

from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import os
import re
import shutil
import sys
import tempfile
from datetime import datetime, timezone
from pathlib import Path
from urllib.parse import unquote, urlsplit


ROOT = Path(__file__).resolve().parents[2]
BUILTIN_SOURCE = ROOT / "builtin-web"
USER_SOURCE = ROOT / "user-web"
BUILD_ROOT = ROOT / "build"
LATEST_ROOT = BUILD_ROOT / "latest"
WEB_OUTPUT = LATEST_ROOT / "web"
WEB_MANIFEST = LATEST_ROOT / "web-manifest.json"
VERSION_PATH = ROOT / "VERSION"
PREVIEW_DIRECTORY = Path("assets/js/preview")
CSS_DIRECTORY = Path("assets/css")
CSS_SOURCE_FILES = (
    CSS_DIRECTORY / "tokens.css",
    CSS_DIRECTORY / "app.css",
    CSS_DIRECTORY / "responsive.css",
    PREVIEW_DIRECTORY / "styles.css",
)
PREVIEW_SCRIPTS = (
    "assets/js/preview/config.js",
    "assets/js/preview/mockApi.js",
)
MOCK_CONFIG, MOCK_ADAPTER = PREVIEW_SCRIPTS
MINIFY_SUFFIXES = {".html", ".css", ".js"}
PREVIEW_BLOCK = re.compile(
    r"\s*<!--\s*PREVIEW START:.*?-->.*?<!--\s*PREVIEW END\s*-->",
    flags=re.DOTALL,
)


class WebBuildError(RuntimeError):
    """The frontend source or processed web output is invalid."""


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Build or verify frontend output under build/latest/web."
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    build_parser = subparsers.add_parser("build", help="Build selected frontend.")
    build_parser.add_argument(
        "--target",
        choices=("production", "demo"),
        default="production",
        help="Output behavior (default: production).",
    )
    build_parser.add_argument(
        "--web",
        choices=("auto", "builtin", "user", "none"),
        default="auto",
        help="Frontend source (default: auto).",
    )
    build_parser.add_argument(
        "--process",
        dest="processing",
        choices=("auto", "minify-gzip", "none"),
        default="auto",
        help="Frontend processing mode (default: auto).",
    )

    verify_parser = subparsers.add_parser(
        "verify", help="Verify build/latest/web and its metadata."
    )
    verify_parser.add_argument(
        "--target",
        choices=("production", "demo"),
        help="Optionally require a specific output target.",
    )

    args = parser.parse_args()
    try:
        if args.command == "build":
            build_web(args.web, args.processing, args.target)
        else:
            verify_published_web(args.target)
    except (WebBuildError, OSError, UnicodeError, json.JSONDecodeError) as error:
        print(f"web-build: {error}", file=sys.stderr)
        return 1
    return 0


def build_web(
    web: str = "auto",
    processing: str = "auto",
    target: str = "production",
) -> dict:
    source_name, source = select_source(web, target)
    resolved_processing = resolve_processing(processing, source_name)
    if source is not None:
        validate_source_tree(source, source_name)
        if source_name == "builtin":
            verify_source_architecture(source)
        if resolved_processing == "minify-gzip":
            reject_precompressed_input(source)

    BUILD_ROOT.mkdir(parents=True, exist_ok=True)
    stage_name = tempfile.mkdtemp(prefix=".web-stage-", dir=BUILD_ROOT)
    stage_root = Path(stage_name)
    stage_web = stage_root / "web"
    try:
        if source is None:
            stage_web.mkdir()
        else:
            copy_source(source, stage_web, source_name, target)
            if source_name == "builtin" and target == "production":
                remove_builtin_preview(stage_web)
            if resolved_processing == "minify-gzip":
                minify_files(stage_web)
                gzip_output_files(stage_web)

        verify_output(stage_web, target, source_name, resolved_processing)
        manifest = create_web_manifest(
            stage_web,
            source,
            source_name,
            resolved_processing,
            target,
        )
        write_json(stage_root / "web-manifest.json", manifest)
        publish_latest(stage_root)
    except Exception:
        if stage_root.exists():
            shutil.rmtree(stage_root)
        raise

    source_label = (
        source_name
        if source is None
        else f"{source_name} ({display_path(source)})"
    )
    print(f"Web source: {source_label}")
    print(f"Web target: {target}")
    print(f"Web processing: {resolved_processing}")
    print(f"Web output: {display_path(WEB_OUTPUT)}")
    print(f"Web files: {manifest['file_count']}")
    print(f"Web payload: {manifest['payload_bytes']} bytes")
    return manifest


def select_source(web: str, target: str) -> tuple[str, Path | None]:
    if target == "demo":
        if web in ("user", "none"):
            raise WebBuildError("demo target only supports WEB=builtin")
        require_valid_index(BUILTIN_SOURCE, "builtin-web")
        return "builtin", BUILTIN_SOURCE

    if web == "none":
        return "none", None
    if web == "builtin":
        require_valid_index(BUILTIN_SOURCE, "builtin-web")
        return "builtin", BUILTIN_SOURCE
    if web == "user":
        require_valid_index(USER_SOURCE, "user-web")
        return "user", USER_SOURCE
    if web != "auto":
        raise WebBuildError(f"unsupported WEB selection: {web}")

    user_files = source_files(USER_SOURCE)
    if not user_files:
        require_valid_index(BUILTIN_SOURCE, "builtin-web")
        return "builtin", BUILTIN_SOURCE
    if has_valid_index(USER_SOURCE):
        return "user", USER_SOURCE
    raise WebBuildError(
        "user-web contains files but has no index.html or index.html.gz; "
        "clear it or provide a complete frontend"
    )


def resolve_processing(processing: str, source_name: str) -> str:
    if source_name == "none":
        if processing in ("auto", "none"):
            return "none"
        raise WebBuildError("WEB=none only supports WEB_PROCESS=none")
    if processing == "auto":
        return "minify-gzip" if source_name == "builtin" else "none"
    if processing not in ("minify-gzip", "none"):
        raise WebBuildError(f"unsupported WEB_PROCESS selection: {processing}")
    return processing


def source_files(root: Path) -> list[Path]:
    if not root.is_dir():
        return []
    return sorted(
        path
        for path in root.rglob("*")
        if path.is_file() and path.relative_to(root) != Path(".gitignore")
    )


def has_valid_index(root: Path) -> bool:
    return (root / "index.html").is_file() or (root / "index.html.gz").is_file()


def require_valid_index(root: Path, label: str) -> None:
    if not root.is_dir():
        raise WebBuildError(f"frontend source is missing: {label}")
    if not has_valid_index(root):
        raise WebBuildError(f"{label} must contain index.html or index.html.gz")


def validate_source_tree(root: Path, source_name: str) -> None:
    require_valid_index(root, f"{source_name}-web")
    if (root / "index.html").is_file() and (root / "index.html.gz").is_file():
        raise WebBuildError("frontend has both index.html and index.html.gz")
    for path in root.rglob("*"):
        if path.is_symlink():
            raise WebBuildError(
                f"frontend source contains a symbolic link: {display_path(path)}"
            )
        if not path.is_dir() and not path.is_file():
            raise WebBuildError(
                f"frontend source contains an unsupported entry: {display_path(path)}"
            )
    validate_logical_paths(root)


def validate_logical_paths(root: Path) -> dict[str, Path]:
    logical_paths: dict[str, Path] = {}
    for path in source_files(root):
        relative = path.relative_to(root).as_posix()
        logical = relative.removesuffix(".gz") if relative.endswith(".gz") else relative
        if logical in logical_paths:
            previous = logical_paths[logical].relative_to(root).as_posix()
            raise WebBuildError(
                f"frontend has raw/gzip collision for {logical}: "
                f"{previous} and {relative}"
            )
        if relative.endswith(".gz"):
            try:
                gzip.decompress(path.read_bytes())
            except (EOFError, OSError) as error:
                raise WebBuildError(
                    f"frontend contains invalid gzip data: {display_path(path)}"
                ) from error
        logical_paths[logical] = path
    return logical_paths


def reject_precompressed_input(source: Path) -> None:
    compressed = next(
        (path for path in source_files(source) if path.suffix.lower() == ".gz"),
        None,
    )
    if compressed is not None:
        raise WebBuildError(
            "WEB_PROCESS=minify-gzip requires raw input; "
            f"precompressed file found: {display_path(compressed)}"
        )


def copy_source(
    source: Path,
    output: Path,
    source_name: str,
    target: str,
) -> None:
    output.mkdir(parents=True)
    for path in sorted(source.rglob("*")):
        relative = path.relative_to(source)
        if relative == Path(".gitignore"):
            continue
        if (
            source_name == "builtin"
            and target == "production"
            and (relative == PREVIEW_DIRECTORY or PREVIEW_DIRECTORY in relative.parents)
        ):
            continue
        destination = output / relative
        if path.is_dir():
            destination.mkdir(parents=True, exist_ok=True)
        elif path.is_file():
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, destination)


def remove_builtin_preview(output: Path) -> None:
    index_path = output / "index.html"
    if not index_path.is_file():
        raise WebBuildError("builtin production transform requires raw index.html")
    index = index_path.read_text(encoding="utf-8")
    index, replacements = PREVIEW_BLOCK.subn("", index)
    if replacements != 1:
        raise WebBuildError("expected exactly one preview block in builtin index.html")
    write_utf8(index_path, index)


def verify_output(
    output: Path,
    target: str,
    source_name: str,
    processing: str,
) -> dict[str, Path]:
    if not output.is_dir():
        raise WebBuildError(f"web output is missing: {display_path(output)}")
    for path in output.rglob("*"):
        if path.is_symlink():
            raise WebBuildError(
                f"web output contains a symbolic link: {display_path(path)}"
            )
    if source_name == "none":
        if target != "production":
            raise WebBuildError("WEB=none only supports the production target")
        if processing != "none":
            raise WebBuildError("WEB=none only supports WEB_PROCESS=none")
        if source_files(output):
            raise WebBuildError("WEB=none output must not contain files")
        return {}

    logical_paths = validate_logical_paths(output)
    if "index.html" not in logical_paths:
        raise WebBuildError("web output is missing logical index.html")

    if processing == "minify-gzip":
        raw = next(
            (
                path
                for path in source_files(output)
                if path.suffix.lower() != ".gz"
            ),
            None,
        )
        if raw is not None:
            raise WebBuildError(
                f"minify-gzip output contains a raw file: {display_path(raw)}"
            )

    index = read_logical_text(logical_paths["index.html"])
    if source_name == "builtin" and target == "production":
        if (output / PREVIEW_DIRECTORY).exists():
            raise WebBuildError("builtin preview directory leaked into production web")
        if any(script in index for script in PREVIEW_SCRIPTS) or "PREVIEW START" in index:
            raise WebBuildError("builtin preview entry leaked into production index.html")
    if target == "demo":
        verify_demo_entries(output, logical_paths, index)

    verify_index_references(index, logical_paths, allow_external=source_name == "user")
    return logical_paths


def verify_demo_entries(
    output: Path,
    logical_paths: dict[str, Path],
    index: str,
) -> None:
    config_position = index.find(f'src="{MOCK_CONFIG}"')
    adapter_position = index.find(f'src="{MOCK_ADAPTER}"')
    app_position = index.find('src="assets/js/namespace.js"')
    if min(config_position, adapter_position, app_position) < 0:
        raise WebBuildError("demo index is missing preview or application script entries")
    if not config_position < adapter_position < app_position:
        raise WebBuildError(
            "preview config and mock adapter must load before the application"
        )
    for preview_file in PREVIEW_SCRIPTS:
        if preview_file not in logical_paths:
            raise WebBuildError(f"demo preview file is missing: {preview_file}")


def verify_index_references(
    index: str,
    logical_paths: dict[str, Path],
    *,
    allow_external: bool,
) -> None:
    for _, value in re.findall(
        r"\b(src|href)=[\"']([^\"']+)[\"']",
        index,
        flags=re.IGNORECASE,
    ):
        if not value or value.startswith(("#", "data:", "blob:")):
            continue
        parsed = urlsplit(value)
        if parsed.scheme or parsed.netloc:
            if allow_external:
                continue
            raise WebBuildError(f"builtin frontend must remain offline: {value}")
        logical = unquote(parsed.path).lstrip("/")
        if not logical:
            continue
        parts = Path(logical).parts
        if ".." in parts or "." in parts:
            raise WebBuildError(f"unsafe frontend reference: {value}")
        if logical not in logical_paths:
            raise WebBuildError(f"referenced web asset is missing: {value}")


def read_logical_text(path: Path) -> str:
    content = path.read_bytes()
    if path.suffix.lower() == ".gz":
        content = gzip.decompress(content)
    return content.decode("utf-8")


def create_web_manifest(
    output: Path,
    source: Path | None,
    source_name: str,
    processing: str,
    target: str,
) -> dict:
    files = source_files(output)
    manifest = {
        "schema": 1,
        "version": read_version(),
        "target": target,
        "web": source_name,
        "web_process": processing,
        "file_count": len(files),
        "payload_bytes": sum(path.stat().st_size for path in files),
        "web_sha256": tree_sha256(output),
        "generated_at": datetime.now(timezone.utc).isoformat(),
    }
    if source_name == "user" and source is not None:
        manifest["user_web_sha256"] = tree_sha256(source, ignore_gitignore=True)
    return manifest


def verify_published_web(expected_target: str | None = None) -> dict:
    if not WEB_MANIFEST.is_file():
        raise WebBuildError(
            "build/latest/web-manifest.json is missing; run make web or make demo"
        )
    manifest = json.loads(WEB_MANIFEST.read_text(encoding="utf-8"))
    if manifest.get("schema") != 1:
        raise WebBuildError("unsupported web manifest schema")
    target = manifest.get("target")
    source_name = manifest.get("web")
    processing = manifest.get("web_process")
    if target not in ("production", "demo"):
        raise WebBuildError("web manifest has an invalid target")
    if source_name not in ("builtin", "user", "none"):
        raise WebBuildError("web manifest has an invalid source")
    if processing not in ("minify-gzip", "none"):
        raise WebBuildError("web manifest has an invalid processing mode")
    if expected_target is not None and target != expected_target:
        raise WebBuildError(
            f"build/latest/web is {target}, expected {expected_target}"
        )

    files = source_files(WEB_OUTPUT)
    verify_output(WEB_OUTPUT, target, source_name, processing)
    if manifest.get("file_count") != len(files):
        raise WebBuildError("web file count no longer matches web-manifest.json")
    payload = sum(path.stat().st_size for path in files)
    if manifest.get("payload_bytes") != payload:
        raise WebBuildError("web payload size no longer matches web-manifest.json")
    if manifest.get("web_sha256") != tree_sha256(WEB_OUTPUT):
        raise WebBuildError("build/latest/web no longer matches web-manifest.json")
    print(f"Verified web files: {len(files)} ({target}, {source_name}, {processing})")
    return manifest


def publish_latest(stage_root: Path) -> None:
    BUILD_ROOT.mkdir(parents=True, exist_ok=True)
    backup = BUILD_ROOT / ".latest-backup"
    if backup.exists():
        shutil.rmtree(backup)
    had_latest = LATEST_ROOT.exists()
    if had_latest:
        os.replace(LATEST_ROOT, backup)
    try:
        os.replace(stage_root, LATEST_ROOT)
    except Exception:
        if had_latest and backup.exists():
            os.replace(backup, LATEST_ROOT)
        raise
    if backup.exists():
        shutil.rmtree(backup)


def read_version() -> str:
    if not VERSION_PATH.is_file():
        raise WebBuildError("VERSION is missing")
    version = VERSION_PATH.read_text(encoding="utf-8").strip()
    if not re.fullmatch(r"\d+\.\d+\.\d+", version):
        raise WebBuildError(f"VERSION must use MAJOR.MINOR.PATCH, got: {version}")
    return version


def write_json(path: Path, value: dict) -> None:
    with path.open("w", encoding="utf-8", newline="\n") as output:
        output.write(json.dumps(value, indent=2, sort_keys=True) + "\n")


def tree_sha256(root: Path, *, ignore_gitignore: bool = False) -> str:
    digest = hashlib.sha256()
    for path in sorted(item for item in root.rglob("*") if item.is_file()):
        relative_path = path.relative_to(root)
        if ignore_gitignore and relative_path == Path(".gitignore"):
            continue
        relative = relative_path.as_posix().encode("utf-8")
        digest.update(len(relative).to_bytes(4, "big"))
        digest.update(relative)
        digest.update(path.stat().st_size.to_bytes(8, "big"))
        with path.open("rb") as source:
            for chunk in iter(lambda: source.read(64 * 1024), b""):
                digest.update(chunk)
    return digest.hexdigest()


def verify_source_architecture(source_root: Path = BUILTIN_SOURCE) -> None:
    index_path = source_root / "index.html"
    if not index_path.is_file():
        raise WebBuildError("builtin frontend is missing builtin-web/index.html")

    forbidden_html = {
        "inline style block": r"<style(?:\s|>)",
        "inline style attribute": r"\sstyle\s*=",
        "inline event handler": r"\son[a-z]+\s*=",
        "inline script block": r"<script(?![^>]*\bsrc\s*=)[^>]*>",
    }
    index_source = index_path.read_text(encoding="utf-8")
    for label, pattern in forbidden_html.items():
        if re.search(pattern, index_source, flags=re.IGNORECASE):
            raise WebBuildError(f"builtin-web/index.html contains forbidden {label}")
    if "@include" in index_source:
        raise WebBuildError("builtin-web/index.html must be directly browser-readable")
    required_entries = (
        '<div id="app"',
        "assets/js/namespace.js",
        "assets/js/app/shell.js",
        "assets/js/app/router.js",
        "assets/js/main.js",
    )
    if not all(marker in index_source for marker in required_entries):
        raise WebBuildError("builtin-web/index.html is missing the application bootstrap")
    if any(marker in index_source for marker in ("appView", "page-network", "loginDialog")):
        raise WebBuildError(
            "builtin-web/index.html must not contain shell, page, or dialog implementation"
        )

    html_sources = {path.resolve() for path in source_root.rglob("*.html")}
    unexpected_html = sorted(html_sources - {index_path.resolve()})
    if unexpected_html:
        raise WebBuildError(
            "unexpected HTML source; keep structure in builtin-web/index.html: "
            f"{display_path(unexpected_html[0])}"
        )

    expected_css = {(source_root / relative).resolve() for relative in CSS_SOURCE_FILES}
    actual_css = {path.resolve() for path in source_root.rglob("*.css")}
    missing_css = sorted(expected_css - actual_css)
    if missing_css:
        raise WebBuildError(f"CSS source is missing: {display_path(missing_css[0])}")
    unexpected_css = sorted(actual_css - expected_css)
    if unexpected_css:
        raise WebBuildError(
            "unexpected CSS source; keep rules in the direct stylesheets: "
            f"{display_path(unexpected_css[0])}"
        )

    for path in sorted(source_root.rglob("*.css")):
        css = path.read_text(encoding="utf-8")
        if re.search(r"\{[^\n{}]+\}", css):
            raise WebBuildError(
                "CSS rule declarations must be written one per line: "
                f"{display_path(path)}"
            )
        if re.search(r"@import\b", css, flags=re.IGNORECASE):
            raise WebBuildError(f"runtime CSS imports are not allowed: {display_path(path)}")
        if "@include" in css:
            raise WebBuildError(
                f"build-time CSS includes are not allowed: {display_path(path)}"
            )

    preview_entry = source_root / MOCK_ADAPTER
    if not preview_entry.is_file():
        raise WebBuildError(f"preview adapter is missing: {display_path(preview_entry)}")
    if "@include" in preview_entry.read_text(encoding="utf-8"):
        raise WebBuildError("preview adapter must be directly browser-readable")

    locale_root = (source_root / "assets/js/i18n").resolve()
    for path in sorted((source_root / "assets/js").rglob("*.js")):
        script = path.read_text(encoding="utf-8")
        if re.search(r"\b(?:innerHTML|outerHTML|insertAdjacentHTML)\b", script):
            raise WebBuildError(
                "frontend structure must use DOM factories, not HTML sinks: "
                f"{display_path(path)}"
            )
        if locale_root in path.resolve().parents:
            continue
        for line_number, line in enumerate(script.splitlines(), start=1):
            svg_path_data = re.match(r'^\s*\["path", \{"(?:d|fill)":', line)
            if len(line) > 140 and not svg_path_data:
                raise WebBuildError(
                    "JavaScript source line exceeds 140 characters: "
                    f"{display_path(path)}:{line_number}"
                )

    module_root = source_root / "assets/js/modules"
    for path in sorted(module_root.rglob("*.js")):
        script = path.read_text(encoding="utf-8")
        relative = display_path(path)
        if re.search(r"\bfetch\s*\(", script):
            raise WebBuildError(f"frontend module bypasses API client: {relative}")
        if re.search(r"\blocalStorage\b", script):
            raise WebBuildError(f"frontend module bypasses state layer: {relative}")
        if re.search(r"['\"]\/api\/", script):
            raise WebBuildError(f"frontend module hard-codes a REST path: {relative}")


def minify_files(output_dir: Path) -> int:
    count = 0
    for path in sorted(output_dir.rglob("*")):
        if not path.is_file() or path.suffix.lower() not in MINIFY_SUFFIXES:
            continue
        text = path.read_text(encoding="utf-8")
        suffix = path.suffix.lower()
        if suffix == ".html":
            text = minify_html(text)
        elif suffix == ".css":
            text = minify_css(text)
        else:
            text = minify_js(text)
        write_utf8(path, text)
        count += 1
    return count


def gzip_output_files(output_dir: Path) -> int:
    count = 0
    for path in sorted(item for item in output_dir.rglob("*") if item.is_file()):
        compressed_path = path.with_name(f"{path.name}.gz")
        compressed_path.write_bytes(
            gzip.compress(path.read_bytes(), compresslevel=9, mtime=0)
        )
        path.unlink()
        count += 1
    return count


def write_utf8(path: Path, text: str) -> None:
    with path.open("w", encoding="utf-8", newline="\n") as output:
        output.write(text)


def minify_html(text: str) -> str:
    text = re.sub(r"<!--(?!\[if).*?-->", "", text, flags=re.DOTALL)
    text = re.sub(r">\s+<", "><", text)
    text = re.sub(r"\s{2,}", " ", text)
    return text.strip() + "\n"


def minify_css(text: str) -> str:
    text = strip_js_css_comments(text)
    text = re.sub(r"\s+", " ", text)
    text = re.sub(r"\s*([{}:;,>+~])\s*", r"\1", text)
    text = re.sub(r";}", "}", text)
    return text.strip() + "\n"


def minify_js(text: str) -> str:
    text = strip_js_css_comments(text)
    lines = [line.strip() for line in text.splitlines()]
    result = ""
    for line in (line for line in lines if line):
        needs_separator = (
            bool(result)
            and result[-1] not in "{([,;:"
            and line[0] not in "})],;:."
        )
        result += (" " if needs_separator else "") + line
    return compact_js_whitespace(result) + "\n"


def compact_js_whitespace(text: str) -> str:
    punctuation = set("{}()[],;:=")
    result: list[str] = []
    index = 0
    quote = ""
    escape = False
    while index < len(text):
        char = text[index]
        if quote:
            result.append(char)
            if escape:
                escape = False
            elif char == "\\":
                escape = True
            elif char == quote:
                quote = ""
            index += 1
            continue
        if char in ("'", '"', "`"):
            quote = char
            result.append(char)
            index += 1
            continue
        if char == "/" and starts_js_regex(result):
            index = append_js_regex(text, index, result)
            continue
        if char.isspace():
            next_index = index + 1
            while next_index < len(text) and text[next_index].isspace():
                next_index += 1
            previous = result[-1] if result else ""
            following = text[next_index] if next_index < len(text) else ""
            if previous not in punctuation and following not in punctuation:
                result.append(" ")
            index = next_index
            continue
        result.append(char)
        index += 1
    return "".join(result)


def starts_js_regex(result: list[str]) -> bool:
    previous = "".join(result).rstrip()
    if not previous:
        return True
    if previous[-1] in "([{:,;=!?&|":
        return True
    match = re.search(r"([A-Za-z_$][A-Za-z0-9_$]*)$", previous)
    return bool(match and match.group(1) in {
        "await", "case", "delete", "do", "else", "in", "instanceof",
        "new", "of", "return", "throw", "typeof", "void", "yield",
    })


def append_js_regex(text: str, index: int, result: list[str]) -> int:
    escaped = False
    character_class = False
    result.append(text[index])
    index += 1
    while index < len(text):
        char = text[index]
        result.append(char)
        if escaped:
            escaped = False
        elif char == "\\":
            escaped = True
        elif char == "[":
            character_class = True
        elif char == "]":
            character_class = False
        elif char == "/" and not character_class:
            index += 1
            while index < len(text) and text[index].isalpha():
                result.append(text[index])
                index += 1
            return index
        index += 1
    return index


def strip_js_css_comments(text: str) -> str:
    result: list[str] = []
    index = 0
    quote = ""
    escape = False
    while index < len(text):
        char = text[index]
        next_char = text[index + 1] if index + 1 < len(text) else ""
        if quote:
            result.append(char)
            if escape:
                escape = False
            elif char == "\\":
                escape = True
            elif char == quote:
                quote = ""
            index += 1
            continue
        if char in ("'", '"', "`"):
            quote = char
            result.append(char)
            index += 1
            continue
        if char == "/" and next_char == "*":
            index += 2
            while index + 1 < len(text) and text[index : index + 2] != "*/":
                index += 1
            index += 2
            continue
        if char == "/" and next_char == "/":
            index += 2
            while index < len(text) and text[index] not in "\r\n":
                index += 1
            continue
        if char == "/" and starts_js_regex(result):
            index = append_js_regex(text, index, result)
            continue
        result.append(char)
        index += 1
    return "".join(result)


def display_path(path: Path) -> str:
    try:
        return str(path.relative_to(ROOT))
    except ValueError:
        return str(path)


if __name__ == "__main__":
    raise SystemExit(main())
