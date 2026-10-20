"""Tests for scripts/read_project_version.py (Duck semver alias)."""
from __future__ import annotations

import importlib.util
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent


class ReadProjectVersionTests(unittest.TestCase):
    def test_prints_duck_version(self) -> None:
        out = subprocess.check_output([sys.executable, "scripts/read_project_version.py"], cwd=ROOT, text=True).strip()
        self.assertRegex(out, r"^[0-9]+\.[0-9]+\.[0-9]+$")

    def test_tag_is_duck_prefixed(self) -> None:
        out = subprocess.check_output(
            [sys.executable, "scripts/read_project_version.py", "--tag"], cwd=ROOT, text=True
        ).strip()
        self.assertTrue(out.startswith("duck-v"))


if __name__ == "__main__":
    unittest.main()
