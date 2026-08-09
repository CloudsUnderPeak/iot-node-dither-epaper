#!/usr/bin/env python3
import argparse
import html
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path
from urllib.parse import quote, urlencode


BENCHMARK_HTML = Path(__file__).resolve().with_name("index.html")
BROWSER_ENV = "DITHER_BENCHMARK_BROWSER"
BROWSER_COMMANDS = [
    "google-chrome",
    "chromium",
    "chromium-browser",
    "chrome",
    "msedge",
    "chrome.exe",
    "msedge.exe",
]


def windows_file_url(path):
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


def chrome_path(explicit):
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
        r'<pre id="benchmark-json"[^>]*>(.*?)</pre>',
        dom,
        flags=re.DOTALL,
    )
    if not match:
        raise SystemExit("Benchmark JSON was not found in browser output.")
    payload = html.unescape(match.group(1)).strip()
    if not payload:
        raise SystemExit("Benchmark JSON was empty.")
    result = json.loads(payload)
    if "error" in result:
        raise SystemExit("Benchmark failed: " + sanitize_text(result["error"]))
    return result


def print_table(result):
    rows = result["rows"]
    headers = ["Algorithm", "Mapping", "Avg ms", "Min ms", "Max ms", "Runs", "Pixels/ms", "Backend", "Checksum"]
    table = []
    for row in rows:
        table.append([
            row["algorithm"],
            row["mapping"],
            f'{row["avg"]:.2f}',
            f'{row["min"]:.2f}',
            f'{row["max"]:.2f}',
            str(row["runs"]),
            f'{row["pixelsPerMs"]:.0f}',
            row.get("backend", ""),
            row.get("checksum", ""),
        ])
    widths = [
        max(len(headers[index]), *(len(row[index]) for row in table))
        for index in range(len(headers))
    ]
    print("  ".join(headers[index].ljust(widths[index]) for index in range(len(headers))))
    print("  ".join("-" * width for width in widths))
    for row in table:
        print("  ".join(row[index].ljust(widths[index]) for index in range(len(headers))))


def main():
    parser = argparse.ArgumentParser(description="Run the internal dither benchmark in headless Chrome.")
    parser.add_argument("--chrome", help="Chrome or Edge executable path.")
    parser.add_argument("--width", type=int, default=800)
    parser.add_argument("--height", type=int, default=480)
    parser.add_argument("--iterations", type=int, default=5)
    parser.add_argument("--warmup", type=int, default=1)
    parser.add_argument("--image", default="gradient", choices=["gradient", "noise", "bands"])
    parser.add_argument("--palette", default="e6", choices=["e6", "gameboy", "sixteen"])
    parser.add_argument("--mapping", default="all", choices=["all", "nearest-color", "pair-mix", "tri-mix"])
    parser.add_argument("--algorithm", default="all")
    parser.add_argument("--backend", default="auto", choices=["auto", "cpu", "gpu"])
    parser.add_argument("--json", action="store_true", help="Print raw JSON instead of a table.")
    args = parser.parse_args()

    query = urlencode({
        "autorun": "1",
        "width": args.width,
        "height": args.height,
        "iterations": args.iterations,
        "warmup": args.warmup,
        "image": args.image,
        "palette": args.palette,
        "mapping": args.mapping,
        "algorithm": args.algorithm,
        "backend": args.backend,
    })
    url = f"{windows_file_url(BENCHMARK_HTML)}?{query}"
    browser = chrome_path(args.chrome)
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
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print_table(result)


if __name__ == "__main__":
    main()
