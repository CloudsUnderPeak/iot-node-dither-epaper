#!/usr/bin/env python3
import argparse
import base64
import html
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path
from urllib.parse import quote, urlencode


RENDER_HTML = Path(__file__).resolve().with_name("index.html")
BROWSER_ENV = "DITHER_RENDER_BROWSER"
BROWSER_COMMANDS = [
    "google-chrome",
    "chromium",
    "chromium-browser",
    "chrome",
    "msedge",
    "chrome.exe",
    "msedge.exe",
]


def file_url(path):
    text = path.resolve().as_posix()
    match = re.match(r"^/mnt/([a-zA-Z])/(.*)$", text)
    if match:
        drive = match.group(1).upper()
        rest = quote(match.group(2), safe="/:")
        return f"file:///{drive}:/{rest}"
    return path.resolve().as_uri()


def posix_path_from_windows_path(value):
    match = re.match(r"^([a-zA-Z]):[\\/](.*)$", value.strip())
    if not match:
        return Path(value)
    drive = match.group(1).lower()
    rest = match.group(2).replace("\\", "/")
    return Path(f"/mnt/{drive}/{rest}")


def powershell_first_line(powershell, command):
    completed = subprocess.run(
        [powershell, "-NoProfile", "-Command", command],
        check=False,
        capture_output=True,
        text=True,
        errors="replace",
    )
    if completed.returncode != 0:
        return ""
    return next((line.strip() for line in completed.stdout.splitlines() if line.strip()), "")


def powershell_browser_path():
    powershell = shutil.which("powershell.exe")
    if not powershell:
        return None
    command = (
        "$names=@('chrome.exe','msedge.exe');"
        "foreach($name in $names){"
        "$cmd=Get-Command $name -ErrorAction SilentlyContinue;"
        "if($cmd){$cmd.Source; break}"
        "}"
    )
    first_line = powershell_first_line(powershell, command)
    if not first_line:
        registry_command = (
            "$names=@('chrome.exe','msedge.exe');"
            "foreach($name in $names){"
            "$paths=@("
            '"HKCU:\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\$name",'
            '"HKLM:\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\$name"'
            ");"
            "foreach($path in $paths){"
            "$value=Get-ItemPropertyValue -Path $path -Name '(default)' -ErrorAction SilentlyContinue;"
            "if($value){$value; break}"
            "}"
            "if($value){break}"
            "}"
        )
        first_line = powershell_first_line(powershell, registry_command)
    return posix_path_from_windows_path(first_line) if first_line else None


def browser_path(explicit):
    if explicit:
        return Path(explicit)
    env_browser = os.environ.get(BROWSER_ENV)
    if env_browser:
        return Path(env_browser)
    for command in BROWSER_COMMANDS:
        found = shutil.which(command)
        if found:
            return Path(found)
    discovered = powershell_browser_path()
    if discovered:
        return discovered
    raise SystemExit(f"Browser was not found. Pass --chrome or set {BROWSER_ENV}.")


def sanitize_text(value):
    text = str(value)
    text = re.sub(r"file:///[^\s\"'<>]+", "<local-file>", text)
    text = re.sub(r"[A-Za-z]:[\\/][^\s\"'<>]+", "<local-file>", text)
    text = re.sub(r"/mnt/[A-Za-z]/[^\s\"'<>]+", "<local-file>", text)
    return text


def extract_json(dom):
    match = re.search(
        r'<pre id="render-json"[^>]*>(.*?)</pre>',
        dom,
        flags=re.DOTALL,
    )
    if not match:
        raise SystemExit("Render JSON was not found in browser output.")
    payload = html.unescape(match.group(1)).strip()
    if not payload:
        raise SystemExit("Render JSON was empty.")
    result = json.loads(payload)
    if "error" in result:
        raise SystemExit("Render failed: " + sanitize_text(result["error"]))
    return result


def write_png(result, output):
    data_url = result.get("dataUrl", "")
    prefix = "data:image/png;base64,"
    if not data_url.startswith(prefix):
        raise SystemExit("Render result did not contain a PNG data URL.")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(base64.b64decode(data_url[len(prefix):]))


def main():
    parser = argparse.ArgumentParser(description="Render a dithered PNG using project dither scripts.")
    parser.add_argument("--chrome", help="Chrome or Edge executable path.")
    parser.add_argument("--input", help="Optional input image path. If omitted, a synthetic source is used.")
    parser.add_argument("--output", default="dither-output.png", help="Output PNG path.")
    parser.add_argument("--source", default="gradient", choices=["gradient", "noise", "bands"])
    parser.add_argument("--width", type=int, default=800)
    parser.add_argument("--height", type=int, default=480)
    parser.add_argument("--palette", default="e6", choices=["e6", "gameboy", "sixteen"])
    parser.add_argument("--colors", help="Custom comma-separated hex palette, for example #000000,#ffffff.")
    parser.add_argument("--mapping", default="nearest-color", choices=["nearest-color", "pair-mix", "tri-mix"])
    parser.add_argument("--algorithm", default="bayer-8")
    parser.add_argument("--color-distance", default="euclidean-bt709")
    parser.add_argument("--backend", default="auto", choices=["auto", "cpu", "gpu"])
    parser.add_argument("--error-strength", type=int, default=100)
    parser.add_argument("--serpentine", action="store_true")
    parser.add_argument("--json", action="store_true", help="Print render metadata as JSON.")
    args = parser.parse_args()

    query = {
        "autorun": "1",
        "source": args.source,
        "width": args.width,
        "height": args.height,
        "palette": args.palette,
        "mapping": args.mapping,
        "algorithm": args.algorithm,
        "colorDistance": args.color_distance,
        "backend": args.backend,
        "errorStrength": args.error_strength,
        "serpentine": "1" if args.serpentine else "0",
    }
    if args.input:
        query["input"] = file_url(Path(args.input))
    if args.colors:
        query["colors"] = args.colors

    url = f"{file_url(RENDER_HTML)}?{urlencode(query)}"
    browser = browser_path(args.chrome)
    command = [
        str(browser),
        "--headless=new",
        "--allow-file-access-from-files",
        "--disable-background-networking",
        "--dump-dom",
        url,
    ]
    completed = subprocess.run(command, check=False, capture_output=True, text=True, errors="replace")
    if completed.returncode != 0:
        sys.stderr.write(sanitize_text(completed.stderr))
        raise SystemExit(completed.returncode)

    result = extract_json(completed.stdout)
    write_png(result, Path(args.output))

    metadata = {
        "output": args.output,
        "width": result["width"],
        "height": result["height"],
        "algorithm": result["algorithm"],
        "mapping": result["mapping"],
        "palette": result["palette"],
        "backend": result["backend"],
        "duration": result["duration"],
        "checksum": result["checksum"],
    }
    if args.json:
        print(json.dumps(metadata, indent=2))
    else:
        print(
            "Rendered {output} ({width}x{height}) algorithm={algorithm} "
            "mapping={mapping} backend={backend} checksum={checksum}".format(**metadata)
        )


if __name__ == "__main__":
    main()
