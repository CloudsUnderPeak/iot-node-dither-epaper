#!/usr/bin/env python3
import argparse
import html
import json
import re
from pathlib import Path


TOOL_DIR = Path(__file__).resolve().parent
import sys
sys.path.insert(0, str(TOOL_DIR.parent / 'shared'))
import browser as helper


def main():
    parser = argparse.ArgumentParser(description="Validate .dither.png routing and round trip.")
    parser.add_argument("--chrome", help="Chrome or Edge executable path.")
    parser.add_argument("--timeout", type=float, default=90)
    args = parser.parse_args()
    browser = helper.browser_path(args.chrome)
    dom = helper.run_ready_browser(browser, helper.file_url(TOOL_DIR / 'index.html', browser), args.timeout)
    match = re.search(r'<pre id="project-test-json">(.*?)</pre>', dom, re.DOTALL)
    if not match:
        raise SystemExit('Project test result was not found.')
    result = json.loads(html.unescape(match.group(1)))
    if result.get("error") or not result.get("passed"):
        raise SystemExit("Project file validation failed: " + result.get("error", "unknown error"))
    app_dom = helper.run_ready_browser(browser, helper.file_url(TOOL_DIR.parent.parent / 'index.html', browser), args.timeout)
    loading_tag = re.search(r'<div\s+id="app-loading"[^>]*>', app_dom)
    if not loading_tag or 'data-state="ready"' not in loading_tag.group(0):
        state = loading_tag.group(0) if loading_tag else "missing loading element"
        message = re.search(r'id="app-loading-message"[^>]*>(.*?)</div>', app_dom, re.DOTALL)
        detail = html.unescape(message.group(1)).strip() if message else "missing message"
        raise SystemExit("Application did not reach the ready state: " + state + " / " + detail)
    if not re.search(r'class="[^"]*\bexport-action\b', app_dom):
        raise SystemExit("Application export actions were not loaded.")
    labels = [label for label in ("Download Image Project", "下載圖片專案") if label in app_dom]
    if not labels:
        raise SystemExit("Application image project action was not loaded.")
    label_position = app_dom.index(labels[0])
    button_start = app_dom.rfind("<button", 0, label_position)
    button_end = app_dom.find("</button>", label_position)
    button_markup = app_dom[button_start:button_end]
    if 'primary-button' not in button_markup or 'secondary-button' in button_markup:
        raise SystemExit("Image project action does not use the primary button style.")
    if "View Source Image" in app_dom or "檢視原圖" in app_dom:
        raise SystemExit("Unexpected source-image action was loaded.")
    print(f"Project file validation passed ({result['bytes']} bytes); app startup passed.")


if __name__ == "__main__":
    main()
