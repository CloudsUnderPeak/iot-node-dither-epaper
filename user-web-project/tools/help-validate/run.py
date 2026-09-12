#!/usr/bin/env python3
import argparse
import html
import json
import re
from pathlib import Path


TOOL_DIR = Path(__file__).resolve().parent
VALIDATION_HTML = TOOL_DIR / "index.html"
import sys
sys.path.insert(0, str(TOOL_DIR.parent / 'shared'))
import browser as helper


def extract_result(dom):
    match = re.search(r'<pre id="validation-json"[^>]*>(.*?)</pre>', dom, flags=re.DOTALL)
    if not match:
        raise SystemExit("Help validation JSON was not found in browser output.")
    return json.loads(html.unescape(match.group(1)).strip())


def main():
    parser = argparse.ArgumentParser(description="Validate Help capabilities and bilingual content.")
    parser.add_argument("--chrome", help="Chrome or Edge executable path.")
    parser.add_argument("--timeout", type=float, default=90)
    args = parser.parse_args()

    browser = helper.browser_path(args.chrome)
    dom = helper.run_ready_browser(browser, helper.file_url(VALIDATION_HTML, browser), args.timeout)

    result = extract_result(dom)
    for warning in result.get("warnings", []):
        print("Warning: " + warning)
    for error in result.get("errors", []):
        print("Error: " + error)
    if result.get("errors"):
        raise SystemExit(1)
    print("Help validation passed.")


if __name__ == "__main__":
    main()
