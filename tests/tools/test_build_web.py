import gzip
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock


PROJECT = Path(__file__).resolve().parents[2]


def load_tool(name: str, relative: str):
    spec = importlib.util.spec_from_file_location(name, PROJECT / relative)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


web = load_tool("build_web_under_test", "tools/web-build/build_web.py")


class WebBuildTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.builtin = self.root / "builtin-web"
        self.user = self.root / "user-web"
        self.build = self.root / "build"
        self.latest = self.build / "latest"
        self.version = self.root / "VERSION"
        self.version.write_text("0.8.0\n", encoding="utf-8")
        self.user.mkdir()
        (self.user / ".gitignore").write_text("*\n!.gitignore\n", encoding="utf-8")
        self.patch = mock.patch.multiple(
            web,
            ROOT=self.root,
            BUILTIN_SOURCE=self.builtin,
            USER_SOURCE=self.user,
            BUILD_ROOT=self.build,
            LATEST_ROOT=self.latest,
            WEB_OUTPUT=self.latest / "web",
            WEB_MANIFEST=self.latest / "web-manifest.json",
            VERSION_PATH=self.version,
        )
        self.patch.start()
        self.architecture_patch = mock.patch.object(
            web,
            "verify_source_architecture",
        )
        self.architecture_patch.start()

    def tearDown(self):
        self.architecture_patch.stop()
        self.patch.stop()
        self.temporary.cleanup()

    def write_builtin(self, preview_blocks: int = 1):
        self.builtin.mkdir(parents=True)
        block = (
            "<!-- PREVIEW START: test -->"
            '<script src="assets/js/preview/config.js"></script>'
            '<script src="assets/js/preview/mockApi.js"></script>'
            "<!-- PREVIEW END -->"
        )
        (self.builtin / "index.html").write_text(
            "<html><body>"
            + block * preview_blocks
            + '<script src="assets/js/namespace.js"></script>'
            + '<script src="assets/js/app.js"></script>'
            + "</body></html>",
            encoding="utf-8",
        )
        app = self.builtin / "assets/js/app.js"
        app.parent.mkdir(parents=True)
        app.write_text("const ready = true;\n", encoding="utf-8")
        (self.builtin / "assets/js/namespace.js").write_text(
            "window.DeviceConsole = {};\n",
            encoding="utf-8",
        )
        preview = self.builtin / web.PREVIEW_DIRECTORY
        preview.mkdir(parents=True)
        (preview / "config.js").write_text("preview config", encoding="utf-8")
        (preview / "mockApi.js").write_text("preview mock", encoding="utf-8")

    def test_auto_uses_builtin_and_default_processing(self):
        self.write_builtin()

        manifest = web.build_web()

        self.assertEqual(manifest["web"], "builtin")
        self.assertEqual(manifest["web_process"], "minify-gzip")
        self.assertTrue((self.latest / "web/index.html.gz").is_file())
        self.assertFalse((self.latest / "web/index.html").exists())
        index = gzip.decompress(
            (self.latest / "web/index.html.gz").read_bytes()
        ).decode("utf-8")
        self.assertNotIn("PREVIEW START", index)
        self.assertFalse((self.latest / "binary").exists())
        self.assertFalse((self.latest / "firmware.img").exists())

    def test_builtin_can_disable_minify_and_gzip(self):
        self.write_builtin()

        manifest = web.build_web("builtin", "none")

        self.assertEqual(manifest["web_process"], "none")
        self.assertTrue((self.latest / "web/index.html").is_file())
        index = (self.latest / "web/index.html").read_text(encoding="utf-8")
        self.assertNotIn("PREVIEW START", index)
        self.assertFalse((self.latest / "web/assets/js/preview").exists())

    def test_user_auto_preserves_raw_files_and_external_resources(self):
        self.write_builtin()
        (self.user / "index.html").write_text(
            '<link href="https://cdn.example/app.css">'
            '<script src="app.js"></script>',
            encoding="utf-8",
        )
        (self.user / "app.js").write_text("const value = 1;\n", encoding="utf-8")

        manifest = web.build_web()

        self.assertEqual(manifest["web"], "user")
        self.assertEqual(manifest["web_process"], "none")
        self.assertIn("user_web_sha256", manifest)
        self.assertTrue((self.latest / "web/index.html").is_file())
        self.assertTrue((self.latest / "web/app.js").is_file())

    def test_auto_rejects_nonempty_incomplete_user_web(self):
        self.write_builtin()
        (self.user / "asset.js").write_text("incomplete", encoding="utf-8")

        with self.assertRaisesRegex(web.WebBuildError, "has no index"):
            web.build_web()

    def test_none_publishes_and_verifies_empty_production_output(self):
        manifest = web.build_web("none")

        self.assertEqual(manifest["web"], "none")
        self.assertEqual(manifest["web_process"], "none")
        self.assertEqual(manifest["file_count"], 0)
        self.assertEqual(manifest["payload_bytes"], 0)
        self.assertEqual(manifest["web_sha256"], web.tree_sha256(self.latest / "web"))
        self.assertNotIn("user_web_sha256", manifest)
        self.assertEqual(list((self.latest / "web").iterdir()), [])
        self.assertEqual(web.verify_published_web("production"), manifest)

    def test_none_rejects_processing_demo_and_output_files(self):
        with self.assertRaisesRegex(web.WebBuildError, "only supports WEB_PROCESS=none"):
            web.build_web("none", "minify-gzip")
        with self.assertRaisesRegex(web.WebBuildError, "only supports WEB=builtin"):
            web.build_web("none", "auto", "demo")

        web.build_web("none")
        (self.latest / "web/asset.js").write_text("unexpected", encoding="utf-8")
        with self.assertRaisesRegex(web.WebBuildError, "must not contain files"):
            web.verify_published_web()

    def test_raw_and_gzip_collision_is_rejected(self):
        (self.user / "index.html").write_text("<html></html>", encoding="utf-8")
        (self.user / "index.html.gz").write_bytes(
            gzip.compress(b"<html></html>", mtime=0)
        )

        with self.assertRaisesRegex(web.WebBuildError, "both index"):
            web.build_web("user", "none")

    def test_minify_gzip_rejects_precompressed_user_input(self):
        (self.user / "index.html.gz").write_bytes(
            gzip.compress(b"<html></html>", mtime=0)
        )

        with self.assertRaisesRegex(web.WebBuildError, "requires raw input"):
            web.build_web("user", "minify-gzip")

    def test_user_can_opt_in_to_minify_gzip(self):
        (self.user / "index.html").write_text(
            '<script src="app.js"></script>',
            encoding="utf-8",
        )
        (self.user / "app.js").write_text(
            "const value = 1; // comment\n",
            encoding="utf-8",
        )

        manifest = web.build_web("user", "minify-gzip")

        self.assertEqual(manifest["web_process"], "minify-gzip")
        self.assertTrue((self.latest / "web/index.html.gz").is_file())
        self.assertTrue((self.latest / "web/app.js.gz").is_file())

    def test_user_precompressed_input_is_preserved_with_none(self):
        compressed = gzip.compress(b"<html></html>", mtime=0)
        (self.user / "index.html.gz").write_bytes(compressed)

        manifest = web.build_web("user", "none")

        self.assertEqual(manifest["web_process"], "none")
        self.assertEqual(
            (self.latest / "web/index.html.gz").read_bytes(),
            compressed,
        )

    def test_import_user_web_atomically_replaces_generated_tree(self):
        source = self.root / "user-web-project/build/latest"
        source.mkdir(parents=True)
        (source / "index.html.gz").write_bytes(
            gzip.compress(b'<script src="app.js"></script>', mtime=0)
        )
        (source / "app.js.gz").write_bytes(
            gzip.compress(b"const ready = true;\n", mtime=0)
        )
        (self.user / "stale.js.gz").write_bytes(gzip.compress(b"stale", mtime=0))

        web.import_user_web(str(source))

        self.assertTrue((self.user / ".gitignore").is_file())
        self.assertTrue((self.user / "index.html.gz").is_file())
        self.assertTrue((self.user / "app.js.gz").is_file())
        self.assertFalse((self.user / "stale.js.gz").exists())
        self.assertEqual(list((self.root / "tmp").iterdir()), [])

    def test_import_rejects_raw_file_without_replacing_previous_tree(self):
        source = self.root / "user-web-project/build/latest"
        source.mkdir(parents=True)
        (source / "index.html.gz").write_bytes(
            gzip.compress(b'<script src="app.js"></script>', mtime=0)
        )
        (source / "app.js").write_text("const ready = true;\n", encoding="utf-8")
        previous = gzip.compress(b"previous", mtime=0)
        (self.user / "index.html.gz").write_bytes(previous)

        with self.assertRaisesRegex(web.WebBuildError, "gzip-only"):
            web.import_user_web(str(source))

        self.assertEqual((self.user / "index.html.gz").read_bytes(), previous)
        self.assertFalse((self.root / "tmp").exists())

    def test_clean_user_web_preserves_only_gitignore(self):
        (self.user / "index.html.gz").write_bytes(
            gzip.compress(b"<html></html>", mtime=0)
        )
        nested = self.user / "assets/app.js.gz"
        nested.parent.mkdir()
        nested.write_bytes(gzip.compress(b"const ready = true;", mtime=0))

        web.clean_user_web()

        self.assertEqual(
            [path.name for path in self.user.iterdir()],
            [".gitignore"],
        )
        self.assertEqual(list((self.root / "tmp").iterdir()), [])

    def test_demo_is_builtin_and_can_be_unprocessed(self):
        self.write_builtin()

        manifest = web.build_web("auto", "none", "demo")

        self.assertEqual(manifest["target"], "demo")
        self.assertEqual(manifest["web"], "builtin")
        self.assertTrue((self.latest / "web/index.html").is_file())
        self.assertTrue(
            (self.latest / "web/assets/js/preview/mockApi.js").is_file()
        )
        web.verify_published_web("demo")

    def test_demo_default_processing_is_minify_gzip(self):
        self.write_builtin()

        manifest = web.build_web("builtin", "auto", "demo")

        self.assertEqual(manifest["web_process"], "minify-gzip")
        self.assertTrue((self.latest / "web/index.html.gz").is_file())
        self.assertTrue(
            (self.latest / "web/assets/js/preview/mockApi.js.gz").is_file()
        )

    def test_preview_block_must_exist_exactly_once(self):
        self.write_builtin(preview_blocks=0)
        with self.assertRaisesRegex(web.WebBuildError, "exactly one preview"):
            web.build_web("builtin")

    def test_web_manifest_detects_output_changes(self):
        self.write_builtin()
        web.build_web()
        manifest = json.loads(
            (self.latest / "web-manifest.json").read_text(encoding="utf-8")
        )
        self.assertEqual(manifest["version"], "0.8.0")

        (self.latest / "web/index.html.gz").write_bytes(
            gzip.compress(b"<html>changed</html>", mtime=0)
        )
        with self.assertRaisesRegex(web.WebBuildError, "no longer matches"):
            web.verify_published_web()

    def test_js_minifier_preserves_literal_boundaries(self):
        source = r'''
            const url = "https://example.test/a//b"; // remove this
            const css = '/* literal */';
            const template = `line // literal ${"/* nested literal */"}`;
            const matcher = /https?:\/\/[^/]+\/a\/\/*/g;
            /* remove this block */
            const quotient = 8 / 2;
        '''
        result = web.minify_js(source)

        self.assertIn('"https://example.test/a//b"', result)
        self.assertIn("'/* literal */'", result)
        self.assertIn('`line // literal ${"/* nested literal */"}`', result)
        self.assertIn(r"/https?:\/\/[^/]+\/a\/\/*/g", result)
        self.assertIn("8 / 2", result)
        self.assertNotIn("remove this", result)

    def test_gzip_output_is_deterministic(self):
        left = self.root / "left"
        right = self.root / "right"
        for directory in (left, right):
            directory.mkdir()
            (directory / "asset.js").write_bytes(b"const value = 1;\n")

        web.gzip_output_files(left)
        web.gzip_output_files(right)

        self.assertEqual(
            (left / "asset.js.gz").read_bytes(),
            (right / "asset.js.gz").read_bytes(),
        )


if __name__ == "__main__":
    unittest.main()
