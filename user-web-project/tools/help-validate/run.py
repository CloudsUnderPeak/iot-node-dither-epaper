#!/usr/bin/env python3
import argparse
import html
import importlib.util
import json
import re
import subprocess
from pathlib import Path


TOOL_DIR = Path(__file__).resolve().parent
VALIDATION_HTML = TOOL_DIR / "index.html"
RENDER_HELPER = TOOL_DIR.parent / "dither-render" / "run.py"


def load_browser_helper():
    spec = importlib.util.spec_from_file_location("dither_render_helper", RENDER_HELPER)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def extract_result(dom):
    match = re.search(r'<pre id="validation-json"[^>]*>(.*?)</pre>', dom, flags=re.DOTALL)
    if not match:
        raise SystemExit("Help validation JSON was not found in browser output.")
    return json.loads(html.unescape(match.group(1)).strip())


def main():
    parser = argparse.ArgumentParser(description="Validate Help capabilities and bilingual content.")
    parser.add_argument("--chrome", help="Chrome or Edge executable path.")
    args = parser.parse_args()

    helper = load_browser_helper()
    browser = helper.browser_path(args.chrome)
    command = [
        str(browser),
        "--headless=new",
        "--allow-file-access-from-files",
        "--disable-background-networking",
        "--dump-dom",
        helper.file_url(VALIDATION_HTML),
    ]
    completed = subprocess.run(command, check=False, capture_output=True, text=True, errors="replace")
    if completed.returncode != 0:
        raise SystemExit(helper.sanitize_text(completed.stderr))

    result = extract_result(completed.stdout)
    for warning in result.get("warnings", []):
        print("Warning: " + warning)
    for error in result.get("errors", []):
        print("Error: " + error)
    if result.get("errors"):
        raise SystemExit(1)
    print("Help validation passed.")


if __name__ == "__main__":
    main()
