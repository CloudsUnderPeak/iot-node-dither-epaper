#!/usr/bin/env python3
"""Deterministic real-module browser regression runner (no server required)."""
import argparse
import base64
import html
import gzip
import importlib.util
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/shared'))
import browser as helper


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--chrome')
    parser.add_argument('--assets', type=Path)
    parser.add_argument('--modules-only', action='store_true')
    parser.add_argument('--production', action='store_true')
    parser.add_argument('--timeout', type=float, default=90)
    args = parser.parse_args()
    browser = helper.browser_path(args.chrome)
    if args.production:
        from production import run
        run(browser, args.timeout)
        return
    if not args.modules_only:
        subprocess.run([sys.executable, '-m', 'unittest', 'discover', '-s', str(ROOT / 'tests/tools'), '-v'], check=True, timeout=args.timeout)
        for tool in ['help-validate', 'project-file-test']:
            subprocess.run([sys.executable, str(ROOT / 'tools' / tool / 'run.py'), '--chrome', str(browser), '--timeout', str(args.timeout)], check=True, timeout=args.timeout * 2 + 10)
    temporary = ROOT / 'tmp'
    temporary.mkdir(exist_ok=True)
    directory = Path(tempfile.mkdtemp(prefix='regression-', dir=temporary))
    try:
        build_spec = importlib.util.spec_from_file_location('user_build', ROOT / 'tools/build/run.py')
        build = importlib.util.module_from_spec(build_spec)
        build_spec.loader.exec_module(build)
        fixture = (ROOT / 'tests/fixtures/css-semantics-v1.css').read_text()
        sources = {str(p.relative_to(ROOT / 'src')): p.read_text() for p in (ROOT / 'src').rglob('*.js')}
        page = directory / 'index.html'
        entry_paths = re.findall(r'<script defer src="(src/[^"]+)"', (ROOT / 'index.html').read_text())
        scripts = 'var testProjectPng=' + json.dumps(base64.b64encode((ROOT / 'tests/fixtures/project-v1.dither.png').read_bytes()).decode()) + ';'
        scripts += 'var testProjectGolden=' + (ROOT / 'tests/fixtures/project-v1.json').read_text() + ';'
        scripts += 'var testColorGolden=' + (ROOT / 'tests/fixtures/color-v1.json').read_text() + ';'
        scripts += 'var testCss=' + json.dumps([fixture, build.minify_css(fixture)]) + ';'
        asset_css = None
        if args.assets:
            output = args.assets.resolve()
            assert not (output / 'src/device/device-mock.js.gz').exists(), 'Production contains mock'
            index = gzip.decompress((output / 'index.html.gz').read_bytes()).decode()
            assert 'device-mock.js' not in index and 'PREVIEW START' not in index
            asset_css = [(ROOT / 'assets/styles/components.css').read_text(), gzip.decompress((output / 'assets/styles/components.css.gz').read_bytes()).decode()]
        scripts += 'var testAssetCss=' + json.dumps(asset_css) + ';'
        scripts += 'var testEntryPaths=' + json.dumps(entry_paths) + ';'
        scripts += (ROOT / 'tests/browser/harness.js').read_text()
        scripts += '\n' + (ROOT / 'tests/browser/regression.js').read_text()
        page.write_text('<!doctype html><meta charset="utf-8"><base href="../../"><pre id="result">WAIT</pre><script>var testSources='
                        + json.dumps(sources).replace('</', '<\\/') + ';' + scripts + '</script>')
        dom = helper.run_ready_browser(browser, helper.file_url(page, browser), args.timeout)
        match = re.search(r'<pre id="result">(.*?)</pre>', dom, re.S)
        if not match or html.unescape(match.group(1)) == 'WAIT':
            raise SystemExit('Browser suite did not finish.')
        report = json.loads(html.unescape(match.group(1)))
        print(helper.sanitize_text(json.dumps(report, indent=2)))
        if report.get('error') or not report.get('passed'):
            raise SystemExit(1)
    except subprocess.TimeoutExpired:
        raise SystemExit('Browser suite exceeded wall-clock timeout.')
    finally:
        shutil.rmtree(directory, ignore_errors=True)


if __name__ == '__main__':
    main()
