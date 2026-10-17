"""Tests for scripts/install_plugins.py using fake bundles (no compiler, no host). Run:
    python3 -m unittest discover -s scripts/tests -v
"""
import contextlib
import importlib.util
import io
import shutil
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

SPEC = importlib.util.spec_from_file_location("install_plugins", Path(__file__).resolve().parent.parent / "install_plugins.py")
ip = importlib.util.module_from_spec(SPEC)
sys.modules["install_plugins"] = ip
SPEC.loader.exec_module(ip)


def make_bundle(root: Path, name="Duck", config="Release", sha="abc1234", layout="linux", stamp=True) -> Path:
    bundle = root / "plugins" / name.lower() / f"{name}_artefacts" / config / "VST3" / f"{name}.vst3"
    folder = {"linux": "x86_64-linux", "mac": "MacOS", "win": "x86_64-win"}[layout]
    binary = bundle / "Contents" / folder / (f"{name}.vst3" if layout == "win" else name)
    binary.parent.mkdir(parents=True)
    payload = b"\x7fELF-junk\x00" + (f"AA-BUILD|0.1.0|{sha}|2026-10-17|".encode() if stamp else b"") + b"\x00tail"
    binary.write_bytes(payload)
    (bundle / "Contents" / "Resources").mkdir(parents=True, exist_ok=True)
    (bundle / "Contents" / "Resources" / "moduleinfo.json").write_text("{}")
    return bundle


def run_main(argv):
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        code = ip.main(argv)
    return code, out.getvalue()


class InstallPluginsTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.tmp = Path(self._tmp.name)
        self.build = self.tmp / "build"
        self.prefix = self.tmp / "vst3"
        patcher = mock.patch.object(ip, "plugin_names", return_value=["Duck", "Wobble"])
        patcher.start()
        self.addCleanup(patcher.stop)
        self.addCleanup(self._tmp.cleanup)

    def args(self, *extra):
        return ["--build-dir", str(self.build), "--prefix", str(self.prefix), *extra]

    def test_reads_stamp_from_every_bundle_layout(self):
        for layout in ("linux", "mac", "win"):
            bundle = make_bundle(self.tmp / layout, layout=layout, sha="deadbee")
            stamp = ip.read_stamp(bundle)
            self.assertEqual((stamp.version, stamp.sha, stamp.date), ("0.1.0", "deadbee", "2026-10-17"), layout)

    def test_missing_stamp_is_reported_as_none(self):
        self.assertIsNone(ip.read_stamp(make_bundle(self.tmp, stamp=False)))

    def test_prefers_release_and_finds_every_plugin(self):
        make_bundle(self.build, "Duck", "Debug")
        release = make_bundle(self.build, "Duck", "Release")
        make_bundle(self.build, "Wobble", "Release")
        found = ip.find_bundles(self.build)
        self.assertEqual([b.name for b in found], ["Duck.vst3", "Wobble.vst3"])
        self.assertIn(release, found)
        self.assertEqual([b.name for b in ip.find_bundles(self.build, {"wobble"})], ["Wobble.vst3"])

    def test_install_copies_all_plugins_and_prints_stamp(self):
        make_bundle(self.build, "Duck", sha="1111111")
        make_bundle(self.build, "Wobble", sha="1111111")
        code, out = run_main(self.args())
        self.assertEqual(code, 0, out)
        self.assertTrue((self.prefix / "Duck.vst3/Contents/Resources/moduleinfo.json").exists())
        self.assertTrue((self.prefix / "Wobble.vst3").is_dir())
        self.assertIn("0.1.0 | 1111111 | 2026-10-17", out)
        self.assertIn("Rescan", out)
        self.assertIn("Use VST3 Plug-In System Folders", out)

    def test_reinstall_replaces_the_old_copy_completely(self):
        make_bundle(self.build, sha="2222222")
        old = self.prefix / "Duck.vst3"
        (old / "Contents").mkdir(parents=True)
        (old / "Contents" / "leftover.txt").write_text("stale")
        code, out = run_main(self.args())
        self.assertEqual(code, 0, out)
        self.assertFalse((old / "Contents" / "leftover.txt").exists())
        self.assertEqual(ip.read_stamp(old).sha, "2222222")
        self.assertFalse(list(self.prefix.glob("*.aa-new")))

    def test_dry_run_changes_nothing(self):
        make_bundle(self.build)
        code, out = run_main(self.args("--dry-run"))
        self.assertEqual(code, 0)
        self.assertFalse(self.prefix.exists())
        self.assertIn("[dry-run] copy", out)

    def test_nothing_built_is_an_error(self):
        code, out = run_main(self.args())
        self.assertEqual(code, 1)
        self.assertIn("Build first", out)

    def test_warns_about_plugins_without_a_built_bundle(self):
        make_bundle(self.build, "Duck")
        _, out = run_main(self.args())
        self.assertIn("plugin 'Wobble' is in plugins/ but has no built bundle", out)

    def test_removes_stale_copies_from_other_standard_folders(self):
        user, system = self.tmp / "user", self.tmp / "system"
        for directory, sha in ((user, "oldoldo"), (system, "oldsyst")):
            make_bundle(directory.parent / f"src-{directory.name}", sha=sha)
            (directory).mkdir(parents=True)
            (directory / "Duck.vst3").mkdir()
            (directory / "Duck.vst3" / "marker").write_text(sha)
        make_bundle(self.build, sha="newnewn")
        with mock.patch.object(ip, "standard_dirs", return_value={"user": [user], "system": [system]}):
            code, out = run_main(["--build-dir", str(self.build), "--user"])
        self.assertEqual(code, 0, out)
        self.assertEqual(ip.read_stamp(user / "Duck.vst3").sha, "newnewn")
        self.assertFalse((system / "Duck.vst3").exists(), "stale copy in the other folder must go")
        self.assertIn("stale copy", out)

    def test_keep_stale_leaves_them_but_warns(self):
        user, system = self.tmp / "user", self.tmp / "system"
        (system / "Duck.vst3").mkdir(parents=True)
        make_bundle(self.build)
        with mock.patch.object(ip, "standard_dirs", return_value={"user": [user], "system": [system]}):
            code, out = run_main(["--build-dir", str(self.build), "--user", "--keep-stale"])
        self.assertEqual(code, 0)
        self.assertTrue((system / "Duck.vst3").exists())
        self.assertIn("a host may load that one instead", out)

    def test_uninstall_removes_only_animal_audio_plugins(self):
        (self.prefix / "Duck.vst3").mkdir(parents=True)
        (self.prefix / "Wobble.vst3").mkdir()
        (self.prefix / "OtherVendor.vst3").mkdir()
        code, _ = run_main(self.args("--uninstall", "--dry-run"))
        self.assertTrue((self.prefix / "Duck.vst3").exists())
        code, out = run_main(self.args("--uninstall"))
        self.assertEqual(code, 0, out)
        self.assertEqual([p.name for p in self.prefix.iterdir()], ["OtherVendor.vst3"])

    def test_list_flags_copies_from_another_commit(self):
        make_bundle(self.tmp / "src", sha="0000000")
        (self.prefix).mkdir()
        shutil.copytree(self.tmp / "src/plugins/duck/Duck_artefacts/Release/VST3/Duck.vst3", self.prefix / "Duck.vst3")
        with mock.patch.object(ip, "repo_head", return_value="9999999"):
            code, out = run_main(["--list", "--prefix", str(self.prefix)])
        self.assertEqual(code, 0)
        self.assertIn("not this checkout's commit", out)

    def test_macos_clears_quarantine_and_ad_hoc_signs(self):
        make_bundle(self.build)
        calls = []
        real_run = ip.subprocess.run

        def fake_run(command, *a, **k):
            if command and command[0] in ("xattr", "codesign"):
                calls.append(command)
                return mock.Mock(returncode=0)
            return real_run(command, *a, **k)

        with mock.patch.object(ip, "detect_os", return_value="macos"), \
                mock.patch.object(ip.subprocess, "run", side_effect=fake_run):
            code, out = run_main(self.args())
        self.assertEqual(code, 0, out)
        self.assertEqual(calls[0][:3], ["xattr", "-dr", "com.apple.quarantine"])
        self.assertEqual(calls[1][:5], ["codesign", "--force", "--deep", "--sign", "-"])

    def test_standard_dirs_per_os(self):
        self.assertTrue(str(ip.standard_dirs("macos")["user"][0]).endswith("Library/Audio/Plug-Ins/VST3"))
        self.assertEqual(str(ip.standard_dirs("macos")["system"][0]), "/Library/Audio/Plug-Ins/VST3")
        self.assertTrue(str(ip.standard_dirs("linux")["user"][0]).endswith(".vst3"))
        with mock.patch.dict(ip.os.environ, {"COMMONPROGRAMFILES": r"C:\CF", "LOCALAPPDATA": r"C:\L"}):
            win = ip.standard_dirs("windows")
        self.assertIn("VST3", str(win["system"][0]))
        self.assertIn("Programs", str(win["user"][0]))

    def test_clean_refuses_directories_outside_the_repo(self):
        outside = self.tmp / "elsewhere"
        outside.mkdir()
        with self.assertRaises(SystemExit):
            ip.build_all(ip.Runner(True), outside, True, None, False, "linux")

    def test_build_commands_cover_all_plugins_unless_restricted(self):
        runner = ip.Runner(True)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            ip.build_all(runner, ip.ROOT / "build-does-not-exist", False, None, False, "linux")
            ip.build_all(runner, ip.ROOT / "build-does-not-exist", False, {"duck"}, False, "macos")
        text = out.getvalue()
        self.assertIn("-DCMAKE_BUILD_TYPE=Release", text)
        self.assertIn("-DAA_PLUGINS=duck", text)
        self.assertEqual(text.count("-DAA_PLUGINS"), 1)


class RepoDiscoveryTest(unittest.TestCase):
    def test_every_plugin_folder_is_discovered_by_product_name(self):
        names = ip.plugin_names()
        folders = [p for p in (ip.ROOT / "plugins").glob("*/CMakeLists.txt")]
        self.assertEqual(len(names), len(folders))
        self.assertIn("Duck", names)


if __name__ == "__main__":
    unittest.main()
