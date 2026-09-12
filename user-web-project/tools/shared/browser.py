"""Shared browser discovery and bounded, isolated headless execution."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time
import json
from urllib.request import urlopen
from browser_cdp import Connection
from urllib.parse import quote

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


def file_url(path, browser=None):
    text = path.resolve().as_posix()
    match = re.match(r"^/mnt/([a-zA-Z])/(.*)$", text)
    if match and (browser is None or str(browser).endswith('.exe')):
        drive = match.group(1).upper()
        rest = quote(match.group(2), safe="/:")
        return f"file:///{drive}:/{rest}"
    return path.resolve().as_uri()


def posix_path_from_windows_path(value):
    if os.name == 'nt':
        return Path(value)
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
        timeout=15,
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


def browser_path(explicit=None, env_name=BROWSER_ENV):
    if explicit:
        return Path(explicit)
    env_browser = os.environ.get(env_name) or os.environ.get("DITHER_BROWSER")
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



def argument_path(path, browser):
    if str(browser).endswith('.exe') and os.name != 'nt':
        return subprocess.check_output(['wslpath', '-w', str(path)], text=True, timeout=10).strip()
    return str(path)


def run_ready_browser(browser, url, timeout=90):
    """Wait for an explicit app/test DOM result with real clocks, then close owned Chrome."""
    root = Path(__file__).resolve().parents[2]
    temporary = root / 'tmp'
    temporary.mkdir(exist_ok=True)
    profile = Path(tempfile.mkdtemp(prefix='browser-profile-', dir=temporary))
    process = None
    connection = None
    deadline = time.monotonic() + timeout
    try:
        process = subprocess.Popen([str(browser), '--headless=new', '--no-first-run', '--no-default-browser-check',
            '--disable-background-networking', '--allow-file-access-from-files', '--disable-renderer-backgrounding',
            '--disable-background-timer-throttling', '--remote-debugging-port=0',
            '--user-data-dir=' + argument_path(profile, browser), 'about:blank'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        port_file = profile / 'DevToolsActivePort'
        while not port_file.exists():
            if process.poll() is not None:
                raise RuntimeError('Chrome exited before DevTools: ' + sanitize_text(process.communicate()[1].decode(errors='replace')))
            if time.monotonic() >= deadline:
                raise TimeoutError('DevTools startup deadline exceeded')
            time.sleep(0.05)
        port = int(port_file.read_text().splitlines()[0])
        if str(browser).endswith('.exe') and os.name != 'nt':
            powershell = shutil.which('powershell.exe')
            if not powershell:
                raise RuntimeError('WSL Chrome readiness requires powershell.exe in PATH.')
            bridge = Path(__file__).with_name('browser-cdp.ps1')
            result = subprocess.run([powershell, '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                argument_path(bridge, browser), '-Port', str(port), '-TargetUrl', url,
                '-TimeoutSeconds', str(max(1, int(deadline - time.monotonic())))],
                capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=max(1, deadline - time.monotonic()) + 5)
            if result.returncode:
                raise RuntimeError('Windows DevTools bridge failed: ' + sanitize_text(result.stderr))
            return result.stdout

        with urlopen('http://127.0.0.1:' + str(port) + '/json/list', timeout=max(1, deadline - time.monotonic())) as response:
            pages = json.load(response)
        target = next(page for page in pages if page['type'] == 'page')
        connection = Connection(target['webSocketDebuggerUrl'], max(1, deadline - time.monotonic()))
        # Register before navigation so readiness observes the new document only.
        connection.call('Page.enable')
        connection.call('Page.navigate', {'url': url})
        while True:
            result = connection.call('Runtime.evaluate', {'expression': "document.URL !== 'about:blank' && document.readyState !== 'loading'", 'returnByValue': True})
            if result.get('result', {}).get('value'):
                break
            if time.monotonic() >= deadline:
                raise TimeoutError('Navigation deadline exceeded')
            time.sleep(0.05)
        expression = Path(__file__).with_name('browser-readiness.js').read_text()
        while True:
            result = connection.call('Runtime.evaluate', {'expression': expression, 'returnByValue': True})
            if result.get('exceptionDetails'):
                raise RuntimeError(str(result['exceptionDetails']))
            if result.get('result', {}).get('value'):
                break
            if time.monotonic() >= deadline:
                raise TimeoutError('DOM readiness deadline exceeded')
            time.sleep(0.02)
        result = connection.call('Runtime.evaluate', {'expression': 'document.documentElement.outerHTML', 'returnByValue': True})
        return result['result']['value']
    finally:
        if connection:
            try:
                connection.send({'id': 999999, 'method': 'Browser.close'})
            except OSError:
                pass
            connection.close()
        if process and process.poll() is None and str(browser).endswith('.exe') and os.name != 'nt':
            # Close only this invocation's exact profile; never touch a user's browser.
            powershell = shutil.which('powershell.exe')
            if powershell:
                subprocess.run([powershell, '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                    argument_path(Path(__file__).with_name('browser-cleanup.ps1'), browser),
                    '-Profile', argument_path(profile, browser)], capture_output=True, timeout=10)
        if process:
            try:
                process.communicate(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.communicate(timeout=10)
        shutil.rmtree(profile, ignore_errors=True)
