"""Fresh release/demo bytes served over temporary loopback HTTP, never deployed."""
import gzip
import hashlib
import html
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import mimetypes
from pathlib import Path
import re
import subprocess
import sys
import threading
from urllib.parse import urlsplit, unquote

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/shared'))
import browser as helper


def run(browser, timeout):
    logs = ROOT / 'tmp/verification/production'
    logs.mkdir(parents=True, exist_ok=True)
    for mode in ['production', 'demo']:
        output = logs / mode
        command = [sys.executable, str(ROOT / 'tools/build/run.py'), '--output', str(output)]
        if mode == 'demo':
            command.append('--demo')
        built = subprocess.run(command, capture_output=True, text=True, check=True, timeout=180)
        print(built.stdout)
        match = re.search(r'^Build output: (.+)$', built.stdout, re.M)
        artifact = Path(match.group(1))
        if not artifact.is_absolute():
            artifact = ROOT / artifact
        compressed = mode == 'production'

        def read(relative):
            path = artifact / (relative + ('.gz' if compressed else ''))
            data = path.read_bytes()
            return gzip.decompress(data) if compressed else data

        index = read('index.html').decode()
        assert ('device-mock.js' in index) == (mode == 'demo'), 'mock script mode mismatch'
        assert (artifact / 'src/device/device-mock.js').exists() == (mode == 'demo')
        assert not (artifact / 'src/device/device-mock.js.gz').exists()
        assert not list(artifact.rglob('*.gz')) if mode == 'demo' else all(p.suffix == '.gz' for p in artifact.rglob('*') if p.is_file())
        entries = re.findall(r'<script[^>]+src="([^"]+)"', index)
        entries = entries[:entries.index('src/pages/dither-editor/entry.js') + 1]
        # The probe imports real release scripts, not source or a mock algorithm.
        probe = '<!doctype html><meta charset="utf-8"><base href="/"><pre id="probe-result">WAIT</pre>'
        probe += ''.join('<link rel="stylesheet" href="' + path + '">' for path in re.findall(r'<link[^>]+href="([^"]+[.]css)"', index))
        probe += ''.join('<script defer src="' + path + '"></script>' for path in entries if path != 'src/device/device-mock.js')
        probe += '<script defer src="/__probe.js"></script>'
        probe_js = (ROOT / 'tests/browser/production.js').read_bytes()
        seen = []

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *_):
                pass

            def handle(self):
                try:
                    super().handle()
                except (BrokenPipeError, ConnectionResetError):
                    pass  # Browser.close cancels optional favicon/background resources.

            def do_GET(self):
                path = unquote(urlsplit(self.path).path)
                seen.append(path)
                encoding = False
                status = 200
                if path == '/__probe.html':
                    data, content_type = probe.encode(), 'text/html'
                elif path == '/__probe.js':
                    data, content_type = probe_js, 'text/javascript'
                elif path.startswith('/api/'):
                    status = 401 if path == '/api/auth/session' else 200
                    data = json.dumps({'success': status == 200, 'data': {'code': 'unauthorized'} if status == 401 else {}}).encode()
                    content_type = 'application/json'
                else:
                    relative = path.lstrip('/') or 'index.html'
                    target = (artifact / (relative + ('.gz' if compressed else ''))).resolve()
                    if not target.is_relative_to(artifact.resolve()) or not target.is_file():
                        self.send_error(404)
                        return
                    data = target.read_bytes()
                    encoding = compressed
                    content_type = mimetypes.guess_type(relative)[0] or 'application/octet-stream'
                self.send_response(status)
                self.send_header('Content-Type', content_type)
                self.send_header('Content-Length', str(len(data)))
                if encoding:
                    self.send_header('Content-Encoding', 'gzip')
                self.end_headers()
                self.wfile.write(data)

        server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        base = 'http://127.0.0.1:' + str(server.server_port)
        try:
            dom = helper.run_ready_browser(browser, base + '/', timeout)
            (logs / (mode + '-startup.html')).write_text(dom)
            loading = re.search(r'<div\s+id="app-loading"[^>]*>', dom)
            if not loading or 'data-state="ready"' not in loading.group(0):
                raise RuntimeError(mode + ' app did not reach ready state; see ignored startup DOM.')
            dom = helper.run_ready_browser(browser, base + '/__probe.html', timeout)
            (logs / (mode + '-probe.html')).write_text(dom)
            match = re.search(r'<pre id="probe-result">(.*?)</pre>', dom, re.S)
            if not match or html.unescape(match.group(1)) == 'WAIT':
                raise RuntimeError(mode + ' HTTP probe did not finish.')
            report = json.loads(html.unescape(match.group(1)))
            if not report.get('passed'):
                raise RuntimeError(helper.sanitize_text(str(report)))
            assert '/src/pages/dither-editor/worker/dither-worker.js' in seen, 'Worker URL was not served'
            assert '/api/auth/session' in seen, 'relative API request was not served'
            report.update(mode=mode, artifact=str(artifact.relative_to(ROOT)), index_sha256=hashlib.sha256(read('index.html')).hexdigest())
            (logs / (mode + '.json')).write_text(json.dumps(report, indent=2) + '\n')
            print(json.dumps(report, indent=2))
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=5)
