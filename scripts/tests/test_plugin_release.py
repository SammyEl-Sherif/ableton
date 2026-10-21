"""Tests for scripts/lib/plugin_release.py and the plugin-agnostic Mac installer templates."""
from __future__ import annotations

import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

from lib import plugin_release  # noqa: E402


def version_file(slug: str) -> str:
    return (ROOT / "plugins" / slug / "VERSION").read_text(encoding="utf-8").strip()


def write_fake_plugin(root: Path, slug: str, target: str, version: str) -> None:
    plugin_dir = root / "plugins" / slug
    plugin_dir.mkdir(parents=True)
    (plugin_dir / "VERSION").write_text(f"{version}\n", encoding="utf-8")
    (plugin_dir / "CMakeLists.txt").write_text(
        f'aa_add_plugin({target}\n    PRODUCT_NAME "{target}"\n    PLUGIN_CODE Fake\n    SOURCES x.cpp)\n',
        encoding="utf-8",
    )


class PluginReleaseTests(unittest.TestCase):
    def test_every_plugin_folder_has_release_metadata(self) -> None:
        slugs = plugin_release.list_plugin_slugs()
        self.assertIn("duck", slugs)
        for slug in slugs:
            with self.subTest(slug=slug):
                info = plugin_release.get_plugin(slug)
                self.assertEqual(info.version, version_file(slug))
                self.assertNotIn(" ", info.product_name)

    def test_multiple_plugins_resolve_independently(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            write_fake_plugin(root, "duck", "Duck", "1.2.3")
            write_fake_plugin(root, "honeybadger", "HoneyBadger", "0.1.0")
            self.assertEqual(plugin_release.list_plugin_slugs(repo_root=root), ["duck", "honeybadger"])

            badger = plugin_release.get_plugin("honeybadger", repo_root=root)
            duck = plugin_release.get_plugin("duck", repo_root=root)
            self.assertEqual(badger.version, "0.1.0")
            self.assertEqual(duck.version, "1.2.3")
            self.assertEqual(badger.github_tag, "honeybadger-v0.1.0")
            self.assertEqual(badger.mac_pkg_name(), "HoneyBadger-0.1.0-mac-installer.pkg")
            self.assertEqual(badger.mac_pkg_identifier, "com.animalaudio.honeybadger.vst3")
            self.assertEqual(badger.release_root(root), root / "dist" / "releases" / "honeybadger" / "0.1.0")

    def test_display_name_splits_pascal_case(self) -> None:
        def display(product: str) -> str:
            return plugin_release.PluginReleaseInfo("x", product, product, "1.0.0").display_name

        self.assertEqual(display("Duck"), "Duck")
        self.assertEqual(display("IcePenguin"), "Ice Penguin")
        self.assertEqual(display("HoneyBadger"), "Honey Badger")

    def test_duck_package_identity_is_stable(self) -> None:
        duck = plugin_release.get_plugin("duck")
        self.assertEqual(duck.mac_pkg_identifier, "com.animalaudio.duck.vst3")
        self.assertEqual(duck.mac_pkg_name(), f"Duck-{duck.version}-mac-installer.pkg")

    def test_github_tags_are_per_slug(self) -> None:
        duck = plugin_release.get_plugin("duck")
        self.assertEqual(duck.github_tag, f"duck-v{duck.version}")

    def test_dist_paths_are_slug_scoped(self) -> None:
        duck = plugin_release.get_plugin("duck")
        self.assertEqual(duck.release_root(ROOT), ROOT / "dist" / "releases" / "duck" / duck.version)

    def test_storage_path_uses_explicit_version(self) -> None:
        duck = plugin_release.get_plugin("duck")
        self.assertEqual(duck.storage_path("x.zip", "9.9.9"), "duck/9.9.9/x.zip")


@unittest.skipUnless(shutil.which("bash") and shutil.which("sed"), "needs bash + sed")
class MacInstallerTemplateTests(unittest.TestCase):
    def test_duck_render_fills_every_placeholder(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            bundle = tmp_path / "Duck.vst3"
            bundle.mkdir()
            out = tmp_path / "out"
            subprocess.run(
                ["bash", str(ROOT / "scripts" / "build_mac_pkg.sh"), "duck", str(bundle), "9.8.7", str(out), "--render-only"],
                check=True,
                capture_output=True,
            )
            rendered = {
                p.relative_to(out).as_posix(): p.read_text(encoding="utf-8")
                for p in out.rglob("*")
                if p.is_file()
            }

        for name, text in rendered.items():
            with self.subTest(file=name):
                self.assertNotRegex(text, r"__[A-Z_]+__")

        dist = rendered["Distribution.xml"]
        self.assertIn("<title>Duck by Animal Audio</title>", dist)
        self.assertIn('<pkg-ref id="com.animalaudio.duck.vst3" version="9.8.7"', dist)
        self.assertIn("#duck-component.pkg</pkg-ref>", dist)
        self.assertIn("/Library/Audio/Plug-Ins/VST3/Duck.vst3", rendered["scripts/postinstall"])
        self.assertIn('"Duck installed"', rendered["scripts/postinstall"])
        self.assertIn("<h1>Install Duck</h1>", rendered["resources/welcome.html"])
        self.assertIn("<h1>Duck is installed</h1>", rendered["resources/conclusion.html"])


if __name__ == "__main__":
    unittest.main()
