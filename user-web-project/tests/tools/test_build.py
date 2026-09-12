import gzip
import contextlib
import io
import importlib.util
from pathlib import Path
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
