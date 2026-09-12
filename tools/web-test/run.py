#!/usr/bin/env python3
"""Run source frontend contracts in a local headless Chrome or Edge."""

from __future__ import annotations

import argparse
import functools
import html
import http.server
import os
import re
import shutil
import subprocess
import tempfile
import threading
from pathlib import Path
from urllib.parse import quote

from css_semantics import build_css_harness
import epdimg_contract


ROOT = Path(__file__).resolve().parents[2]
BUILTIN_INDEX = ROOT / "builtin-web/index.html"
TEST_ROOT = ROOT / "tests/web"
OUTPUT_ROOT = ROOT / "tmp/web-tests"
BROWSER_ENVIRONMENT = "DEVICE_CONSOLE_BROWSER"
BROWSER_COMMANDS = (
    "google-chrome",
    "chromium",
    "chromium-browser",
    "chrome",
    "msedge",
    "chrome.exe",
    "msedge.exe",
)
WINDOWS_BROWSER_PATHS = (
    Path("/mnt/c/Program Files/Google/Chrome/Application/chrome.exe"),
    Path("/mnt/c/Program Files (x86)/Microsoft/Edge/Application/msedge.exe"),
)


class WebTestError(RuntimeError):
    """The browser test runner or a frontend test failed."""


def file_url(path: Path) -> str:
    text = path.resolve().as_posix()
    match = re.match(r"^/mnt/([a-zA-Z])/(.*)$", text)
    if match:
        drive = match.group(1).upper()
        rest = quote(match.group(2), safe="/:")
        return f"file:///{drive}:/{rest}"
    return path.resolve().as_uri()


def browser_argument_path(path: Path, browser: Path) -> str:
    text = path.resolve().as_posix()
    if browser.suffix.lower() != ".exe":
        return text
    match = re.match(r"^/mnt/([a-zA-Z])/(.*)$", text)
    if not match:
        return text
    return f"{match.group(1).upper()}:/{match.group(2)}"


def find_browser(explicit: str | None) -> Path:
    requested = explicit or os.environ.get(BROWSER_ENVIRONMENT)
    if requested:
        path = Path(requested)
        if path.is_file():
            return path
        found = shutil.which(requested)
        if found:
            return Path(found)
        raise WebTestError(f"browser is missing: {requested}")
    for command in BROWSER_COMMANDS:
        found = shutil.which(command)
        if found:
            return Path(found)
    for path in WINDOWS_BROWSER_PATHS:
        if path.is_file():
            return path
    raise WebTestError(
        f"Chrome or Edge was not found; pass --browser or set {BROWSER_ENVIRONMENT}"
    )


def build_application_harness() -> Path:
    source = BUILTIN_INDEX.read_text(encoding="utf-8")
    if source.count("</head>") != 1 or source.count("</body>") != 1:
        raise WebTestError("builtin-web/index.html has an unsupported document shape")
    source = re.sub(
        r'(?P<prefix>\b(?:src|href)=")(?P<path>assets/[^"]+)',
        lambda match: (
            match.group("prefix")
            + file_url(BUILTIN_INDEX.parent / match.group("path"))
        ),
        source,
    )
    diagnostics = (
        "<script>"
        "window.__webTestErrors=[];"
        "window.addEventListener('error',function(event){"
        "window.__webTestErrors.push(event.message||'script error');"
        "});"
        "window.addEventListener('unhandledrejection',function(event){"
        "window.__webTestErrors.push(String(event.reason&&event.reason.message"
        "||event.reason||'unhandled rejection'));"
        "});"
        "</script>"
    )
    source = source.replace(
        "</head>",
        f"    {diagnostics}\n  </head>",
    )
    test_script = file_url(TEST_ROOT / "AppTest.js")
    source = source.replace(
        "</body>",
        '    <pre id="result">WAIT</pre>\n'
        f'    <script src="{test_script}"></script>\n'
        "  </body>",
    )
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    harness = OUTPUT_ROOT / "AppTest.html"
    harness.write_text(source, encoding="utf-8", newline="\n")
    return harness


def build_http_application_harness() -> Path:
    source = BUILTIN_INDEX.read_text(encoding="utf-8")
    if source.count("</head>") != 1 or source.count("</body>") != 1:
        raise WebTestError("builtin-web/index.html has an unsupported document shape")
    source = re.sub(
        r'(?P<prefix>\b(?:src|href)=")(?P<path>assets/[^"]+)',
        lambda match: match.group("prefix") + "/builtin-web/" + match.group("path"),
        source,
    )
    diagnostics = (
        "<script>"
        "window.__webTestErrors=[];"
        "window.addEventListener('error',function(event){"
        "window.__webTestErrors.push(event.message||'script error');"
        "});"
        "window.addEventListener('unhandledrejection',function(event){"
        "window.__webTestErrors.push(String(event.reason&&event.reason.message"
        "||event.reason||'unhandled rejection'));"
        "});"
        "</script>"
    )
    source = source.replace("</head>", f"    {diagnostics}\n  </head>")
    source = source.replace(
        "</body>",
        '    <pre id="result">WAIT</pre>\n'
        '    <script src="/tests/web/AppTest.js"></script>\n'
        "  </body>",
    )
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    harness = OUTPUT_ROOT / "AppHttpTest.html"
    harness.write_text(source, encoding="utf-8", newline="\n")
    return harness


class QuietRequestHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, format: str, *args: object) -> None:
        pass


def run_http_application_test(browser: Path, profile: Path, harness: Path) -> None:
    handler = functools.partial(QuietRequestHandler, directory=ROOT)
    with http.server.ThreadingHTTPServer(("127.0.0.1", 0), handler) as server:
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            port = server.server_address[1]
            relative = harness.relative_to(ROOT).as_posix()
            run_page(
                browser,
                profile,
                "Application lifecycle (HTTP)",
                f"http://127.0.0.1:{port}/{relative}?mock=1",
            )
        finally:
            server.shutdown()
            thread.join(timeout=5)


def extract_result(dom: str, label: str) -> None:
    match = re.search(
        r'<pre id="result"[^>]*>(.*?)</pre>',
        dom,
        flags=re.DOTALL,
    )
    if not match:
        raise WebTestError(f"{label}: result marker was not found")
    result = html.unescape(re.sub(r"<[^>]+>", "", match.group(1))).strip()
    if result != "PASS":
        raise WebTestError(f"{label}: {result or 'empty result'}")


def run_page(
    browser: Path,
    profile: Path,
    label: str,
    url: str,
    window_size: tuple[int, int] = (1280, 900),
) -> str:
    window_width, window_height = window_size
    command = [
        str(browser),
        "--headless=new",
        "--allow-file-access-from-files",
        "--disable-background-networking",
        "--disable-component-update",
        "--disable-default-apps",
        "--disable-sync",
        "--no-first-run",
        f"--window-size={window_width},{window_height}",
        f"--user-data-dir={browser_argument_path(profile, browser)}",
        "--virtual-time-budget=30000",
        "--dump-dom",
        url,
    ]
    if os.environ.get("CI"):
        command[1:1] = ["--no-sandbox", "--disable-dev-shm-usage"]
    completed = subprocess.run(
        command,
        check=False,
        capture_output=True,
        text=True,
        errors="replace",
        timeout=60,
    )
    if completed.returncode != 0:
        details = completed.stderr.strip()
        suffix = f": {details[-2000:]}" if details else ""
        raise WebTestError(
            f"{label}: browser exited with {completed.returncode}{suffix}"
        )
    try:
        extract_result(completed.stdout, label)
    except WebTestError:
        safe_label = re.sub(r"[^a-z0-9]+", "-", label.lower()).strip("-")
        (OUTPUT_ROOT / f"{safe_label}.dom.html").write_text(
            completed.stdout,
            encoding="utf-8",
            newline="\n",
        )
        raise
    print(f"{label}: PASS")
    return completed.stdout


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run Device Console headless browser tests."
    )
    parser.add_argument("--browser", help="Chrome or Edge executable path.")
    args = parser.parse_args()

    try:
        browser = find_browser(args.browser)
        harness = build_application_harness()
        http_harness = build_http_application_harness()
        css_harness = build_css_harness(ROOT, OUTPUT_ROOT)
        OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(
            prefix="profile-",
            dir=OUTPUT_ROOT,
        ) as profile_name:
            profile = Path(profile_name)
            run_page(
                browser,
                profile,
                "ResourceStore",
                file_url(TEST_ROOT / "ResourceStoreTest.html"),
            )
            run_page(
                browser,
                profile,
                "API client (formal)",
                file_url(TEST_ROOT / "ApiClientBoundaryTest.html") + "?mock=0",
            )
            run_page(
                browser,
                profile,
                "API client (mock)",
                file_url(TEST_ROOT / "ApiClientBoundaryTest.html") + "?mock=1",
            )
            run_page(
                browser,
                profile,
                "Application lifecycle (file)",
                file_url(harness) + "?mock=1",
                window_size=(1680, 900),
            )
            run_http_application_test(browser, profile, http_harness)
            run_page(browser, profile, "CSS production semantics", file_url(css_harness))
            run_page(browser, profile, "API deadlines", file_url(TEST_ROOT / "ApiDeadlineTest.html"))
            contract = epdimg_contract.build_harness(ROOT, OUTPUT_ROOT)
            dom = run_page(browser, profile, "User EPDIMG encoder", file_url(contract))
            epdimg_contract.validate_bytes(ROOT, OUTPUT_ROOT, dom)
    except (
        OSError,
        subprocess.TimeoutExpired,
        subprocess.CalledProcessError,
        ValueError,
        WebTestError,
    ) as error:
        print(f"web-test: {error}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
