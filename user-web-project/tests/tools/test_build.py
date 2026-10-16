import gzip
import contextlib
import io
import importlib.util
from pathlib import Path
import re
import tempfile
import unittest
from unittest.mock import MagicMock, patch
import sys

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('release_build', ROOT / 'tools/build/run.py')
build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build)
sys.path.insert(0, str(ROOT / 'tools/shared'))
import browser


class BuildTests(unittest.TestCase):
    def test_css_lexical_islands(self):
        source = '.a/**/.b { --s: "a  b"; bottom: calc(100% + 8px); content: "/*x*/"; }'
        result = build.minify_css(source)
        for fragment in ['.a/**/.b', '"a  b"', 'calc(100% + 8px)', '"/*x*/"']:
            self.assertIn(fragment, result)

    def test_gzip_preserves_bytes(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = b'body { bottom: calc(100% + 8px); }'
            (root / 'style.css').write_bytes(source)
            build.gzip_output_files(root)
            self.assertFalse((root / 'style.css').exists())
            self.assertEqual(gzip.decompress((root / 'style.css.gz').read_bytes()), source)

    def test_preview_strip(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'index.html'
            path.write_text('before<!-- PREVIEW START -->mock<!-- PREVIEW END -->after')
            build.strip_preview_block(path)
            self.assertEqual(path.read_text(), 'beforeafter')

    def test_release_processing_flags(self):
        for flags, minified, compressed in [(['--no-minify', '--no-gzip'], False, False), (['--no-gzip'], True, False), (['--no-minify'], False, True)]:
            with self.subTest(flags=flags), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                (root / 'assets').mkdir()
                (root / 'index.html').write_text('<body><!-- PREVIEW START -->mock<!-- PREVIEW END --></body>')
                source = 'p {\n  bottom: calc(100% + 8px);\n}\n'
                (root / 'assets/style.css').write_text(source)
                with patch.object(build, 'ROOT', root), patch.object(build, 'COPY_PATHS', ('index.html', 'assets')), patch.object(build, 'EXCLUDE_PATHS', set()), patch.object(sys, 'argv', ['build', '--output', str(root / 'output')] + flags), contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(build.main(), 0)
                path = root / 'output/latest/assets' / ('style.css.gz' if compressed else 'style.css')
                actual = gzip.decompress(path.read_bytes()).decode() if compressed else path.read_text()
                self.assertEqual(actual, build.minify_css(source) if minified else source)

    def bundle_fixture(self, root, overrides=None):
        files = {
            'index.html': '<head>\n    <link rel="icon" href="assets/icons/app/favicon.ico">\n'
                          '    <link rel="stylesheet" href="assets/styles/a.css">\n'
                          '    <link rel="stylesheet" href="assets/styles/b.css">\n</head><body>\n'
                          '    <span data-svg-icon="assets/icons/x.svg"></span>\n'
                          '    <script defer src="src/namespace.js"></script>\n'
                          '    <script defer src="src/pages/help/entry.js"></script>\n</body>',
            'assets/styles/a.css': 'a{}',
            'assets/styles/b.css': 'b{}',
            'assets/icons/x.svg': '<svg xmlns="http://www.w3.org/2000/svg"><path d="M0 0"/></svg>',
            'assets/icons/app/favicon.ico': 'ico',
            'src/namespace.js': '(function () { window.DitherApp = {}; })(window);',
            'src/pages/help/entry.js': '(function (app) { app.entry = 1; })(window.DitherApp);',
            'src/pages/help/page.js': '(function (app) { app.help = 1; })(window.DitherApp);',
            'src/pages/a/page.js': '(function (app) { app.a = 1; })(window.DitherApp);',
            'src/vendor/lib.js': '(function () { this.Lib = 1; }).call(this);',
            build.WORKER_SCRIPT: "self.window = self;\nimportScripts(\n    '../../../namespace.js'\n);\nself.onmessage = null;\n",
        }
        files.update(overrides or {})
        for relative, text in files.items():
            (root / relative).parent.mkdir(parents=True, exist_ok=True)
            (root / relative).write_text(text, encoding='utf-8')

    def test_bundle_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.bundle_fixture(root)
            self.assertEqual(build.bundle_output(root), 8)
            index = (root / 'index.html').read_text(encoding='utf-8')
            self.assertEqual(re.findall(r'<link rel="stylesheet" href="([^"]+)"', index), ['bundle/app.css'])
            self.assertEqual(re.findall(r'<script[^>]+src="([^"]+)"', index), ['bundle/app.js'])
            self.assertIn('assets/icons/app/favicon.ico', index)
            app_script = (root / 'bundle/app.js').read_text(encoding='utf-8')
            for fragment in ['"src/namespace.js":true', '"src/pages/a/page.js":function(){', '"src/vendor/lib.js":function(){',
                             '"src/pages/help/page.js": "bundle/help.js"', '"assets/icons/x.svg": "data:image/svg+xml;charset=utf-8,%3Csvg']:
                self.assertIn(fragment, app_script)
            # Registry is defined before entry scripts run; entries keep index.html order.
            self.assertLess(app_script.index('app.a = 1'), app_script.index('window.DitherApp = {}'))
            self.assertLess(app_script.index('window.DitherApp = {}'), app_script.index('app.entry = 1'))
            self.assertIn('scripts["src/pages/help/page.js"]=function(){', (root / 'bundle/help.js').read_text(encoding='utf-8'))
            self.assertEqual((root / 'bundle/app.css').read_text(encoding='utf-8'), 'a{}\nb{}')
            worker = (root / build.WORKER_SCRIPT).read_text(encoding='utf-8')
            self.assertIn('window.DitherApp = {}', worker)
            self.assertNotIn('importScripts', worker)
            for removed in ['src/namespace.js', 'src/pages/a', 'src/pages/help', 'src/vendor', 'assets/styles', 'assets/icons/x.svg']:
                self.assertFalse((root / removed).exists(), removed)
            self.assertTrue((root / 'assets/icons/app/favicon.ico').exists())

    def test_bundle_rejects_unsafe_modules(self):
        for overrides in [{'src/pages/a/page.js': 'var leaked = 1;'},
                          {'index.html': '<script src="src/namespace.js"></script>'}]:
            with self.subTest(overrides=list(overrides)), tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                self.bundle_fixture(root, overrides)
                with self.assertRaises(SystemExit):
                    build.bundle_output(root)

    def test_native_file_url(self):
        path = ROOT / 'index.html'
        self.assertEqual(browser.file_url(path, '/usr/bin/chromium'), path.as_uri())

    def test_browser_failure_and_timeout_cleanup(self):
        before = set((ROOT / 'tmp').glob('browser-profile-*'))
        failed = MagicMock()
        failed.poll.return_value = 7
        failed.communicate.return_value = (b'', b'failed startup')
        with patch.object(browser.subprocess, 'Popen', return_value=failed):
            with self.assertRaises(RuntimeError):
                browser.run_ready_browser('/fake/chrome', 'about:blank', 1)
        pending = MagicMock()
        pending.poll.return_value = None
        pending.communicate.return_value = (b'', b'')
        with patch.object(browser.subprocess, 'Popen', return_value=pending), patch.object(browser.time, 'monotonic', side_effect=[0, 2]):
            with self.assertRaises(TimeoutError):
                browser.run_ready_browser('/fake/chrome', 'about:blank', 1)
        self.assertEqual(set((ROOT / 'tmp').glob('browser-profile-*')), before)



if __name__ == '__main__':
    unittest.main()
