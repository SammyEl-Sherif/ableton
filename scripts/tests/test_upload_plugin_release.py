"""Tests for scripts/upload_plugin_release.py (dry run / SQL generation only; no network)."""
from __future__ import annotations

import contextlib
import io
import json
import sys
import tempfile
import unittest
from dataclasses import replace
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

import upload_plugin_release  # noqa: E402


def manifest(**overrides: object) -> upload_plugin_release.Manifest:
    base = upload_plugin_release.Manifest(
        plugin_slug="honeybadger",
        version="0.1.0",
        platform="mac",
        file_name="HoneyBadger-0.1.0-mac-installer.pkg",
        storage_path="honeybadger/0.1.0/HoneyBadger-0.1.0-mac-installer.pkg",
        sha256="a" * 64,
        byte_size=4_194_304,
    )
    return replace(base, **overrides)


class UploadPluginReleaseTests(unittest.TestCase):
    def test_existing_object_is_removed_before_copy_so_reruns_can_overwrite(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            (tmp_path / "web" / "supabase").mkdir(parents=True)
            artifact = tmp_path / "Duck-1.0.1-mac-installer.pkg"
            artifact.write_bytes(b"pkg")
            manifest_path = tmp_path / "manifest-mac.json"
            manifest_path.write_text(
                json.dumps(
                    {
                        "plugin_slug": "duck",
                        "version": "1.0.1",
                        "platform": "mac",
                        "file_name": artifact.name,
                        "storage_path": f"duck/1.0.1/{artifact.name}",
                        "sha256": "ab" * 32,
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
                        str(manifest_path),
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


class PublishSqlTests(unittest.TestCase):
    def test_updates_the_active_row_and_inserts_on_first_release(self) -> None:
        sql = upload_plugin_release.sql_publish(manifest())
        self.assertIn("update public.plugin_releases", sql)
        self.assertIn("where plugin_slug = 'honeybadger' and platform = 'mac' and is_active = true", sql)
        self.assertIn("insert into public.plugin_releases", sql)
        self.assertIn("where not exists (select 1 from updated)", sql)
        self.assertIn("'honeybadger/0.1.0/HoneyBadger-0.1.0-mac-installer.pkg'", sql)
        self.assertTrue(sql.rstrip().endswith(";"))
        self.assertEqual(sql.count(";"), 1)

    def test_rejects_values_that_are_not_pipeline_output(self) -> None:
        bad = [
            {"plugin_slug": "duck'; drop table x; --"},
            {"plugin_slug": "Duck"},
            {"version": "1.0"},
            {"platform": "linux"},
            {"file_name": "Duck 1.0.pkg"},
            {"storage_path": "../duck/1.0.0/x.pkg"},
            {"sha256": "abc"},
            {"byte_size": 0},
        ]
        for override in bad:
            with self.subTest(override=override), self.assertRaises(SystemExit):
                upload_plugin_release.sql_publish(manifest(**override))


if __name__ == "__main__":
    unittest.main()
