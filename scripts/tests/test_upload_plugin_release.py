"""Tests for scripts/upload_plugin_release.py (dry run, no network)."""
from __future__ import annotations

import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

import upload_plugin_release  # noqa: E402


class UploadPluginReleaseTests(unittest.TestCase):
    def test_existing_object_is_removed_before_copy_so_reruns_can_overwrite(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            (tmp_path / "web" / "supabase").mkdir(parents=True)
            artifact = tmp_path / "Duck-1.0.1-mac-installer.pkg"
            artifact.write_bytes(b"pkg")
            manifest = tmp_path / "manifest-mac.json"
            manifest.write_text(
                json.dumps(
                    {
                        "plugin_slug": "duck",
                        "version": "1.0.1",
                        "platform": "mac",
                        "file_name": artifact.name,
                        "storage_path": f"duck/1.0.1/{artifact.name}",
                        "sha256": "abc",
                        "byte_size": 3,
                    }
                ),
                encoding="utf-8",
            )

            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                code = upload_plugin_release.main(
                    [
                        "--manifest",
                        str(manifest),
                        "--zip",
                        str(artifact),
                        "--web-repo",
                        str(tmp_path / "web"),
                        "--dry-run",
                    ]
                )

        self.assertEqual(code, 0)
        commands = [line for line in out.getvalue().splitlines() if line.startswith("$ npx supabase storage")]
        dst = f"ss:///plugin-releases/duck/1.0.1/{artifact.name}"
        self.assertEqual(len(commands), 2)
        self.assertIn(" storage rm ", commands[0])
        self.assertTrue(commands[0].endswith(dst))
        self.assertIn(" storage cp ", commands[1])
        self.assertTrue(commands[1].endswith(dst))


if __name__ == "__main__":
    unittest.main()
