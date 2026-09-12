#!/usr/bin/env python3
import argparse
import base64
import html
import json
import re
import sys
from pathlib import Path
from urllib.parse import urlencode


RENDER_HTML = Path(__file__).resolve().with_name("index.html")
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'shared'))
import browser as browser_helper
file_url = browser_helper.file_url
windows_file_url = browser_helper.file_url
sanitize_text = browser_helper.sanitize_text

def browser_path(explicit):
    return browser_helper.browser_path(explicit, 'DITHER_RENDER_BROWSER')


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
    parser.add_argument("--timeout", type=float, default=180)
    args = parser.parse_args()
    browser = browser_path(args.chrome)

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
        query["input"] = file_url(Path(args.input), browser)
    if args.colors:
        query["colors"] = args.colors

    url = f"{file_url(RENDER_HTML, browser)}?{urlencode(query)}"
    dom = browser_helper.run_ready_browser(browser, url, timeout=args.timeout)

    result = extract_json(dom)
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
