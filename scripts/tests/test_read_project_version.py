"""Tests for scripts/read_project_version.py."""
from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
SPEC = importlib.util.spec_from_file_location(
    "read_project_version", ROOT / "scripts" / "read_project_version.py"
)
read_version_mod = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(read_version_mod)


class ReadProjectVersionTests(unittest.TestCase):
    def test_reads_version_from_cmake(self) -> None:
        version = read_version_mod.read_project_version(ROOT / "CMakeLists.txt")
        parts = version.split(".")
        self.assertEqual(len(parts), 3)
        for part in parts:
            self.assertTrue(part.isdigit())

    def test_parses_minimal_cmake_snippet(self) -> None:
        with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as tmp:
            tmp.write('project(AnimalAudio VERSION 2.3.4 LANGUAGES CXX)\n')
            path = Path(tmp.name)
        self.addCleanup(path.unlink, missing_ok=True)
        self.assertEqual(read_version_mod.read_project_version(path), "2.3.4")


if __name__ == "__main__":
    unittest.main()
