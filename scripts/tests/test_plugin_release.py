"""Tests for scripts/lib/plugin_release.py."""
from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

from lib import plugin_release  # noqa: E402


def version_file(slug: str) -> str:
    return (ROOT / "plugins" / slug / "VERSION").read_text(encoding="utf-8").strip()


class PluginReleaseTests(unittest.TestCase):
    def test_versions_come_from_each_plugins_version_file(self) -> None:
        duck = plugin_release.get_plugin("duck")
        ice = plugin_release.get_plugin("icepenguin")
        self.assertEqual(duck.version, version_file("duck"))
        self.assertEqual(ice.version, version_file("icepenguin"))

    def test_github_tags_are_per_slug(self) -> None:
        duck = plugin_release.get_plugin("duck")
        self.assertEqual(duck.github_tag, f"duck-v{duck.version}")

    def test_dist_paths_are_slug_scoped(self) -> None:
        duck = plugin_release.get_plugin("duck")
        self.assertEqual(duck.release_root(ROOT), ROOT / "dist" / "releases" / "duck" / duck.version)

    def test_storage_path_uses_explicit_version(self) -> None:
        duck = plugin_release.get_plugin("duck")
        self.assertEqual(duck.storage_path("x.zip", "9.9.9"), "duck/9.9.9/x.zip")


if __name__ == "__main__":
    unittest.main()
