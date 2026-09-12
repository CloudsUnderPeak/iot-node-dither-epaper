"""Cross-project fixture agreement; each project transforms its own source."""
import hashlib
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]

class IntegrationFixtureTest(unittest.TestCase):
    def test_css_v1_agrees_with_user_source(self):
        main = (ROOT / "tests/tools/fixtures/css-semantics-v1.css").read_bytes()
        user = (ROOT / "user-web-project/tests/fixtures/css-semantics-v1.css").read_bytes()
        self.assertTrue(main)
        self.assertEqual(hashlib.sha256(main).digest(), hashlib.sha256(user).digest())
