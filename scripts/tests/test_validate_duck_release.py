"""Tests for scripts/validate_duck_release.py (layout + stamp; no codesign)."""
from __future__ import annotations

import importlib.util
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
SPEC = importlib.util.spec_from_file_location("validate_duck_release", ROOT / "scripts" / "validate_duck_release.py")
validate = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules["validate_duck_release"] = validate
SPEC.loader.exec_module(validate)

SPEC_IP = importlib.util.spec_from_file_location("install_plugins", ROOT / "scripts" / "install_plugins.py")
ip = importlib.util.module_from_spec(SPEC_IP)
assert SPEC_IP.loader is not None
sys.modules["install_plugins"] = ip
SPEC_IP.loader.exec_module(ip)


def make_minimal_mac_zip(dest: Path, *, local_name: bool = False) -> None:
    bundle_name = "Duck Local.vst3" if local_name else "Duck.vst3"
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        bundle = root / bundle_name
        binary = bundle / "Contents" / "MacOS" / "Duck"
        binary.parent.mkdir(parents=True)
        binary.write_bytes(b"\x7fELF" + b"junk\x00AA-BUILD|0.1.0|abc1234|2026-10-19|\x00")
        (root / "Animal-Audio-Duck-EULA.txt").write_text("eula")
        (root / "Third-Party-Notices.txt").write_text("notices")
        with zipfile.ZipFile(dest, "w") as zf:
            for path in root.rglob("*"):
                if path.is_file():
                    zf.write(path, path.relative_to(root).as_posix())


class ValidateDuckReleaseTests(unittest.TestCase):
    def test_accepts_minimal_unsigned_mac_zip(self):
        with tempfile.TemporaryDirectory() as tmp:
            zpath = Path(tmp) / "Duck-1.0.0-mac-vst3.zip"
            make_minimal_mac_zip(zpath)
            manifest = validate.validate_zip(
                zpath,
                platform_name="mac",
                version="1.0.0",
                require_notarized=False,
                allow_unsigned=True,
                expected_sha=None,
                expected_bytes=None,
            )
            self.assertEqual(manifest.sha256, validate.sha256_file(zpath))
            self.assertEqual(manifest.storage_path, "duck/1.0.0/Duck-1.0.0-mac-vst3.zip")

    def test_rejects_local_bundle_name(self):
        with tempfile.TemporaryDirectory() as tmp:
            zpath = Path(tmp) / "bad.zip"
            make_minimal_mac_zip(zpath, local_name=True)
            with self.assertRaises(SystemExit):
                validate.validate_zip(
                    zpath,
                    platform_name="mac",
                    version="1.0.0",
                    require_notarized=False,
                    allow_unsigned=True,
                    expected_sha=None,
                    expected_bytes=None,
                )


if __name__ == "__main__":
    unittest.main()
