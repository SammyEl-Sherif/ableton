"""Tests for scripts/lib/plugin_release.py."""
from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

from lib import plugin_release  # noqa: E402


class PluginReleaseTests(unittest.TestCase):
    def test_duck_and_icepenguin_have_independent_versions(self) -> None:
        duck = plugin_release.get_plugin("duck")
        ice = plugin_release.get_plugin("icepenguin")
        self.assertEqual(duck.version, "1.0.0")
        self.assertEqual(ice.version, "0.1.0")
        self.assertNotEqual(duck.version, ice.version)

    def test_github_tags_are_per_slug(self) -> None:
        duck = plugin_release.get_plugin("duck")
        self.assertEqual(duck.github_tag, "duck-v1.0.0")

    def test_dist_paths_are_slug_scoped(self) -> None:
        duck = plugin_release.get_plugin("duck")
        self.assertIn("duck/1.0.0", str(duck.release_root()))


if __name__ == "__main__":
    unittest.main()
