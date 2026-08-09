import importlib.util
import json
import struct
import sys
import unittest
import urllib.error
from pathlib import Path
from unittest import mock


PROJECT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "epaper_tool_under_test", PROJECT / "tools/epaper/epaper_tool.py"
)
epaper = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = epaper
assert SPEC.loader is not None
SPEC.loader.exec_module(epaper)


class FakeResponse:
    def __init__(self, status, body):
        self.status = status
        self.body = body

    def __enter__(self):
        return self

    def __exit__(self, *_args):
        return False

    def read(self):
        return self.body

    def close(self):
        pass


class EpaperToolTest(unittest.TestCase):
    def test_auto_rotate_turns_portrait_clockwise_for_landscape_panel(self):
        class FakeImageModule:
            class Transpose:
                ROTATE_270 = "clockwise"

        portrait = mock.Mock(width=480, height=800)
        rotated = mock.sentinel.rotated
        portrait.transpose.return_value = rotated
        self.assertIs(
            epaper._orient_for_panel(portrait, FakeImageModule, True), rotated
        )
        portrait.transpose.assert_called_once_with("clockwise")

        landscape = mock.Mock(width=800, height=480)
        self.assertIs(
            epaper._orient_for_panel(landscape, FakeImageModule, True), landscape
        )
        landscape.transpose.assert_not_called()

    def test_auto_rotate_can_be_disabled(self):
        parser = epaper.build_parser()
        enabled = parser.parse_args(["convert", "portrait.png"])
        disabled = parser.parse_args(
            ["convert", "portrait.png", "--no-auto-rotate"]
        )
        self.assertTrue(enabled.auto_rotate)
        self.assertFalse(disabled.auto_rotate)

    def test_build_epdimg_header_crc_and_palette_bytes(self):
        frame = bytes([0x01, 0x23, 0x56]) * (epaper.FRAME_BYTES // 3)
        converted = epaper.build_epdimg(frame, generation=123456789)
        self.assertEqual(len(converted.payload), epaper.UPLOAD_BYTES)
        header = struct.unpack("<8sIIIIIIQ", converted.payload[: epaper.HEADER_BYTES])
        self.assertEqual(
            header,
            (
                epaper.MAGIC,
                1,
                40,
                800,
                480,
                192000,
                converted.crc32,
                123456789,
            ),
        )
        self.assertEqual(converted.payload[epaper.HEADER_BYTES :], frame)

    def test_pack_codes_rejects_wrong_size_and_invalid_color(self):
        with self.assertRaises(epaper.ToolError):
            epaper.pack_codes([1, 1])
        codes = bytearray([1]) * (epaper.WIDTH * epaper.HEIGHT)
        codes[4] = 4
        with self.assertRaises(epaper.ToolError):
            epaper.pack_codes(codes)

    def test_normalize_base_url_handles_scheme_port_and_validation(self):
        self.assertEqual(
            epaper.normalize_base_url("192.168.4.1", 80, False),
            "http://192.168.4.1",
        )
        self.assertEqual(
            epaper.normalize_base_url("http://device.local:8080/", 80, False),
            "http://device.local:8080",
        )
        with self.assertRaises(epaper.ToolError):
            epaper.normalize_base_url("192.168.4.1", 0, False)

    @mock.patch.object(epaper.urllib.request, "urlopen")
    def test_client_upload_uses_raw_contract(self, urlopen):
        urlopen.return_value = FakeResponse(
            202,
            json.dumps(
                {"success": True, "data": {"state": "queued"}, "message": "ok"}
            ).encode(),
        )
        client = epaper.EpaperApiClient("http://192.168.4.1")
        document = client.upload(bytes(epaper.UPLOAD_BYTES))
        request = urlopen.call_args.args[0]
        self.assertEqual(document["data"]["state"], "queued")
        self.assertEqual(request.full_url, "http://192.168.4.1/api/epaper/image")
        self.assertEqual(request.method, "POST")
        self.assertEqual(request.get_header("Content-type"), "application/octet-stream")
        self.assertEqual(request.get_header("Content-length"), str(epaper.UPLOAD_BYTES))

    @mock.patch.object(epaper.urllib.request, "urlopen")
    def test_client_reads_error_code_from_envelope_data(self, urlopen):
        body = json.dumps(
            {
                "success": False,
                "data": {"code": "epaper_busy", "retry_after_seconds": 120},
                "message": "e-paper is busy",
            }
        ).encode()
        urlopen.side_effect = urllib.error.HTTPError(
            "http://192.168.4.1/api/epaper/image/white",
            409,
            "Conflict",
            {},
            FakeResponse(409, body),
        )
        client = epaper.EpaperApiClient("http://192.168.4.1")
        with self.assertRaises(epaper.ApiError) as caught:
            client.action("white")
        self.assertEqual(caught.exception.code, "epaper_busy")
        self.assertEqual(caught.exception.data["retry_after_seconds"], 120)

    def test_wait_reports_asynchronous_draw_failure_immediately(self):
        client = epaper.EpaperApiClient("http://192.168.4.1")
        client.status = mock.Mock(
            return_value={
                "success": True,
                "data": {
                    "state": "idle",
                    "last_operation": {
                        "result": "failed",
                        "error_code": "frame_read_failed",
                    },
                },
            }
        )
        with self.assertRaisesRegex(epaper.ToolError, "frame_read_failed"):
            client.wait_for_draw(10, interval=0)
        client.status.assert_called_once_with()


if __name__ == "__main__":
    unittest.main()
