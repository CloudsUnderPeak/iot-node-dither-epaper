import gzip
import hashlib
import importlib.util
import json
import tarfile
import tempfile
import unittest
from datetime import datetime
from pathlib import Path
from unittest import mock
from zoneinfo import ZoneInfo


PROJECT = Path(__file__).resolve().parents[2]


def load_tool(name: str, relative: str):
    spec = importlib.util.spec_from_file_location(name, PROJECT / relative)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


release = load_tool(
    "build_release_under_test",
    "tools/release-build/build_release.py",
)


class ReleaseBuildTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.build = self.root / "build"
        self.latest = self.build / "latest"
        self.version = self.root / "VERSION"
        self.build.mkdir()
        (self.build / ".gitignore").write_text("*\n!.gitignore\n", encoding="utf-8")
        self.version.write_text("0.8.0\n", encoding="utf-8")
        self.patch = mock.patch.multiple(
            release,
            BUILD_ROOT=self.build,
            LATEST_ROOT=self.latest,
            WEB_OUTPUT=self.latest / "web",
            WEB_MANIFEST_PATH=self.latest / "web-manifest.json",
            WORK_ROOT=self.build / ".work",
            GENERATED_OUTPUT=self.build / ".work/esp-generated",
            EMBEDDED_WEB_HEADER=(
                self.build / ".work/esp-generated/EmbeddedWebAssets.generated.h"
            ),
            VERSION_PATH=self.version,
        )
        self.patch.start()

    def tearDown(self):
        self.patch.stop()
        self.temporary.cleanup()

    def write_valid_snapshot(self, name: str = "20260726_0428") -> Path:
        snapshot = self.build / name
        web_root = snapshot / "web"
        binary_root = snapshot / "binary"
        web_root.mkdir(parents=True)
        binary_root.mkdir()
        (web_root / "index.html.gz").write_bytes(
            gzip.compress(b"<html></html>\n", mtime=0)
        )
        web_manifest = {
            "schema": 1,
            "version": "0.8.0",
            "target": "production",
            "web": "builtin",
            "web_process": "minify-gzip",
            "file_count": 1,
            "payload_bytes": (web_root / "index.html.gz").stat().st_size,
            "web_sha256": release.tree_sha256(web_root),
        }
        release.write_json(snapshot / "web-manifest.json", web_manifest)

        layout = (
            (0x0, "bootloader.bin", b"B" * 16),
            (0x8000, "partitions.bin", b"P" * 16),
            (0xE000, "boot_app0.bin", b"A" * 16),
            (release.APP_OFFSET, "firmware.bin", b"F" * 32),
        )
        images = []
        for offset, image_name, content in layout:
            path = binary_root / image_name
            path.write_bytes(content)
            images.append(
                {
                    "offset": hex(offset),
                    "file": image_name,
                    "size": len(content),
                    "sha256": release.sha256(path),
                }
            )
        manifest = {
            "schema": release.MANIFEST_SCHEMA,
            "product": release.PRODUCT,
            "version": "0.8.0",
            "release": "v0.8.0",
            "build_id": name,
            "environment": release.SUPPORTED_ENVIRONMENT,
            "board": release.SUPPORTED_BOARD,
            "chip": release.SUPPORTED_CHIP,
            "upload_baud": release.SUPPORTED_UPLOAD_BAUD,
            "flash_mode": release.SUPPORTED_FLASH_MODE,
            "flash_frequency": release.SUPPORTED_FLASH_FREQUENCY,
            "flash_size": release.SUPPORTED_FLASH_SIZE,
            "app_partition": "app0",
            "app_offset": release.APP_OFFSET,
            "app_size": release.APP_SIZE,
            "firmware_size": 32,
            "app_available_size": release.APP_SIZE - 32,
            "frontend_delivery": "embedded",
            "frontend_file_count": 1,
            "frontend_payload_size": web_manifest["payload_bytes"],
            "web": "builtin",
            "web_process": "minify-gzip",
            "web_sha256": web_manifest["web_sha256"],
            "user_data_partition": "userdata",
            "user_data_offset": release.USER_DATA_OFFSET,
            "user_data_size": release.USER_DATA_SIZE,
            "user_data_reserve_bytes": 64 * 1024,
            "user_nvs_partition": "user_nvs",
            "user_nvs_offset": release.USER_NVS_OFFSET,
            "user_nvs_size": release.USER_NVS_SIZE,
            "images": images,
        }
        release.write_json(binary_root / "manifest.json", manifest)
        release.create_firmware_package(
            binary_root,
            snapshot / "firmware.img",
        )
        return snapshot

    def rewrite_manifest(self, snapshot: Path, manifest: dict) -> None:
        binary = snapshot / "binary"
        release.write_json(binary / "manifest.json", manifest)
        release.create_firmware_package(binary, snapshot / "firmware.img")

    def convert_snapshot_to_none(self, snapshot: Path) -> None:
        web_root = snapshot / "web"
        for path in web_root.iterdir():
            path.unlink()
        web_manifest_path = snapshot / "web-manifest.json"
        web_manifest = json.loads(web_manifest_path.read_text(encoding="utf-8"))
        web_manifest.update(
            {
                "web": "none",
                "web_process": "none",
                "file_count": 0,
                "payload_bytes": 0,
                "web_sha256": release.tree_sha256(web_root),
            }
        )
        web_manifest.pop("user_web_sha256", None)
        release.write_json(web_manifest_path, web_manifest)

        manifest = json.loads(
            (snapshot / "binary/manifest.json").read_text(encoding="utf-8")
        )
        manifest.update(
            {
                "frontend_delivery": "none",
                "frontend_file_count": 0,
                "frontend_payload_size": 0,
                "web": "none",
                "web_process": "none",
                "web_sha256": web_manifest["web_sha256"],
            }
        )
        manifest.pop("user_web_sha256", None)
        self.rewrite_manifest(snapshot, manifest)

    def test_partition_csv_parsing_and_alignment(self):
        path = self.root / "partitions.csv"
        path.write_text(
            "# Name, Type, SubType, Offset, Size\n"
            "nvs,data,nvs,0x9000,20K\n"
            "app0,app,ota_0,,1M\n"
            "userdata,data,spiffs,,64K\n",
            encoding="utf-8",
        )

        partitions = release.parse_partitions(path)

        self.assertEqual(partitions[0]["offset"], 0x9000)
        self.assertEqual(partitions[0]["size"], 20 * 1024)
        self.assertEqual(partitions[1]["offset"], 0x10000)
        self.assertEqual(partitions[1]["size"], 1024 * 1024)
        self.assertEqual(partitions[2]["offset"], 0x110000)
        self.assertEqual(partitions[2]["size"], 64 * 1024)

        path.write_text("broken,data,nvs\n", encoding="utf-8")
        with self.assertRaisesRegex(release.ReleaseError, "invalid partition"):
            release.parse_partitions(path)

    def test_embedded_records_support_raw_and_gzip(self):
        web_root = self.root / "web"
        web_root.mkdir()
        (web_root / "index.html").write_text("<html></html>", encoding="utf-8")
        (web_root / "app.js.gz").write_bytes(
            gzip.compress(b"const app = true;", mtime=0)
        )

        records = release.embedded_web_records(web_root)

        by_path = {record["request_path"]: record for record in records}
        self.assertEqual(by_path["/index.html"]["content_encoding"], "")
        self.assertEqual(by_path["/app.js"]["content_encoding"], "gzip")

        (web_root / "app.js").write_text("collision", encoding="utf-8")
        with self.assertRaisesRegex(release.ReleaseError, "collision"):
            release.embedded_web_records(web_root)

    def test_generated_header_embeds_validated_web_identity(self):
        web_root = self.latest / "web"
        web_root.mkdir(parents=True)
        (web_root / "index.html").write_text("<html></html>", encoding="utf-8")
        web_hash = release.tree_sha256(web_root)

        release.generate_embedded_web_header(
            {"web": "user", "web_sha256": web_hash}
        )

        generated = release.EMBEDDED_WEB_HEADER.read_text(encoding="utf-8")
        self.assertIn('constexpr char kWebSource[] = "user";', generated)
        self.assertIn(
            f'constexpr char kWebSha256[] = "{web_hash}";',
            generated,
        )
        with self.assertRaisesRegex(release.ReleaseError, "invalid source"):
            release.generate_embedded_web_header(
                {"web": "custom", "web_sha256": web_hash}
            )
        with self.assertRaisesRegex(release.ReleaseError, "invalid SHA-256"):
            release.generate_embedded_web_header(
                {"web": "builtin", "web_sha256": "not-a-hash"}
            )

        (web_root / "index.html").unlink()
        release.generate_embedded_web_header(
            {"web": "none", "web_sha256": release.tree_sha256(web_root)}
        )
        generated = release.EMBEDDED_WEB_HEADER.read_text(encoding="utf-8")
        self.assertIn('constexpr char kWebSource[] = "none";', generated)
        self.assertIn("constexpr const char *kWebSha256 = nullptr;", generated)
        self.assertIn("const EmbeddedWebAsset kAssets[1] = {};", generated)
        self.assertIn("constexpr size_t kAssetCount = 0;", generated)
        self.assertIn("constexpr size_t kPayloadBytes = 0;", generated)

    def test_firmware_package_is_flat_and_matches_binary(self):
        snapshot = self.write_valid_snapshot()

        manifest = release.verify_snapshot(
            snapshot,
            snapshot / "firmware.img",
        )

        self.assertEqual(manifest["schema"], release.MANIFEST_SCHEMA)
        with tarfile.open(snapshot / "firmware.img", "r:gz") as archive:
            names = archive.getnames()
            self.assertEqual(names, list(release.PACKAGE_NAMES))
            self.assertNotIn("binary/manifest.json", names)
        self.assertEqual(
            (snapshot / "firmware.img").read_bytes()[:2],
            b"\x1f\x8b",
        )
        duplicate = snapshot / "firmware-copy.img"
        release.create_firmware_package(snapshot / "binary", duplicate)
        self.assertEqual(
            (snapshot / "firmware.img").read_bytes(),
            duplicate.read_bytes(),
        )
        non_deterministic = snapshot / "firmware-metadata.img"
        with tarfile.open(non_deterministic, "w:gz") as archive:
            for name in release.PACKAGE_NAMES:
                archive.add(
                    snapshot / "binary" / name,
                    arcname=name,
                    recursive=False,
                )
        with self.assertRaisesRegex(release.ReleaseError, "metadata"):
            release.verify_package(non_deterministic, snapshot / "binary")

    def test_corruption_and_web_mismatch_are_rejected(self):
        snapshot = self.write_valid_snapshot()
        (snapshot / "binary/firmware.bin").write_bytes(b"corrupt")
        with self.assertRaisesRegex(release.ReleaseError, "size mismatch"):
            release.verify_snapshot(snapshot, snapshot / "firmware.img")

        snapshot = self.write_valid_snapshot("20260726_0429")
        (snapshot / "web/index.html.gz").write_bytes(
            gzip.compress(b"<html>changed</html>", mtime=0)
        )
        with self.assertRaisesRegex(release.ReleaseError, "no longer matches"):
            release.verify_snapshot(snapshot, snapshot / "firmware.img")

        snapshot = self.write_valid_snapshot("20260726_0430")
        extra = snapshot / "extra.bin"
        extra.write_bytes(b"extra")
        with tarfile.open(snapshot / "firmware.img", "w:gz") as archive:
            for name in release.PACKAGE_NAMES:
                archive.add(
                    snapshot / "binary" / name,
                    arcname=name,
                    recursive=False,
                )
            archive.add(extra, arcname="binary/extra.bin", recursive=False)
        with self.assertRaisesRegex(release.ReleaseError, "flat"):
            release.verify_snapshot(snapshot, snapshot / "firmware.img")

    def test_none_snapshot_is_verified_and_rejects_assets(self):
        snapshot = self.write_valid_snapshot()
        self.convert_snapshot_to_none(snapshot)

        manifest = release.verify_snapshot(snapshot, snapshot / "firmware.img")
        self.assertEqual(manifest["web"], "none")
        self.assertEqual(manifest["frontend_delivery"], "none")
        self.assertEqual(manifest["frontend_file_count"], 0)
        self.assertEqual(manifest["frontend_payload_size"], 0)

        web_root = snapshot / "web"
        (web_root / "unexpected.js").write_text("asset", encoding="utf-8")
        web_manifest_path = snapshot / "web-manifest.json"
        web_manifest = json.loads(web_manifest_path.read_text(encoding="utf-8"))
        web_manifest["file_count"] = 1
        web_manifest["payload_bytes"] = 5
        web_manifest["web_sha256"] = release.tree_sha256(web_root)
        release.write_json(web_manifest_path, web_manifest)
        binary_manifest = json.loads(
            (snapshot / "binary/manifest.json").read_text(encoding="utf-8")
        )
        binary_manifest["frontend_file_count"] = 1
        binary_manifest["frontend_payload_size"] = 5
        binary_manifest["web_sha256"] = web_manifest["web_sha256"]
        self.rewrite_manifest(snapshot, binary_manifest)

        with self.assertRaisesRegex(release.ReleaseError, "WEB=none"):
            release.verify_snapshot(snapshot, snapshot / "firmware.img")

    def test_overlap_and_user_partition_images_are_rejected(self):
        snapshot = self.write_valid_snapshot()
        manifest_path = snapshot / "binary/manifest.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["images"][1]["offset"] = "0x8"
        self.rewrite_manifest(snapshot, manifest)
        with self.assertRaisesRegex(release.ReleaseError, "overlap"):
            release.verify_snapshot(snapshot, snapshot / "firmware.img")

        snapshot = self.write_valid_snapshot("20260726_0429")
        manifest_path = snapshot / "binary/manifest.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        firmware = manifest["images"][-1]
        firmware["offset"] = hex(release.USER_DATA_OFFSET)
        self.rewrite_manifest(snapshot, manifest)
        with self.assertRaisesRegex(release.ReleaseError, "overwrite user data"):
            release.verify_snapshot(snapshot, snapshot / "firmware.img")

    def test_incompatible_board_metadata_is_rejected(self):
        snapshot = self.write_valid_snapshot()
        manifest = json.loads(
            (snapshot / "binary/manifest.json").read_text(encoding="utf-8")
        )
        manifest["chip"] = "unreviewed-chip"
        self.rewrite_manifest(snapshot, manifest)

        with self.assertRaisesRegex(release.ReleaseError, "incompatible"):
            release.verify_snapshot(snapshot, snapshot / "firmware.img")

    def test_timestamp_collision_uses_incrementing_suffix(self):
        (self.build / "20260726_0428").mkdir()
        (self.build / "20260726_0428_02").mkdir()
        now = datetime(2026, 7, 26, 4, 28, tzinfo=ZoneInfo("Asia/Taipei"))

        self.assertEqual(release.next_build_id(now), "20260726_0428_03")

    def test_clean_preserves_or_removes_snapshots_as_requested(self):
        self.write_valid_snapshot()
        self.latest.mkdir()
        (self.build / ".work").mkdir()

        release.clean_outputs(False)

        self.assertTrue((self.build / "20260726_0428").is_dir())
        self.assertFalse(self.latest.exists())
        self.assertTrue((self.build / ".gitignore").is_file())

        release.clean_outputs(True)
        self.assertFalse((self.build / "20260726_0428").exists())
        self.assertTrue((self.build / ".gitignore").is_file())

    def test_tree_hash_is_deterministic_and_content_sensitive(self):
        left = self.root / "left"
        right = self.root / "right"
        left.mkdir()
        right.mkdir()
        (left / "a.txt").write_bytes(b"A")
        (left / "b.txt").write_bytes(b"B")
        (right / "b.txt").write_bytes(b"B")
        (right / "a.txt").write_bytes(b"A")

        self.assertEqual(release.tree_sha256(left), release.tree_sha256(right))
        (right / "a.txt").write_bytes(b"changed")
        self.assertNotEqual(release.tree_sha256(left), release.tree_sha256(right))

    def test_tree_hash_uses_utf8_path_order_on_every_host(self):
        root = self.root / "case-order"
        root.mkdir()
        (root / "a.txt").write_bytes(b"lower")
        (root / "Z.txt").write_bytes(b"upper")

        digest = hashlib.sha256()
        for name, content in (("Z.txt", b"upper"), ("a.txt", b"lower")):
            relative = name.encode("utf-8")
            digest.update(len(relative).to_bytes(4, "big"))
            digest.update(relative)
            digest.update(len(content).to_bytes(8, "big"))
            digest.update(content)

        self.assertEqual(release.tree_sha256(root), digest.hexdigest())


if __name__ == "__main__":
    unittest.main()
