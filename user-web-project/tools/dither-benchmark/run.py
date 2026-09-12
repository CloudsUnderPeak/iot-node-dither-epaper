#!/usr/bin/env python3
import argparse
import html
import json
import re
import sys
from pathlib import Path
from urllib.parse import urlencode


BENCHMARK_HTML = Path(__file__).resolve().with_name("index.html")
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'shared'))
import browser as browser_helper
file_url = browser_helper.file_url
windows_file_url = browser_helper.file_url
sanitize_text = browser_helper.sanitize_text

def chrome_path(explicit):
    return browser_helper.browser_path(explicit, 'DITHER_BENCHMARK_BROWSER')


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
    parser.add_argument("--timeout", type=float, default=180)
    args = parser.parse_args()
    browser = chrome_path(args.chrome)

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
    url = f"{windows_file_url(BENCHMARK_HTML, browser)}?{query}"
    dom = browser_helper.run_ready_browser(browser, url, timeout=args.timeout)

    result = extract_json(dom)
    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print_table(result)


if __name__ == "__main__":
    main()
