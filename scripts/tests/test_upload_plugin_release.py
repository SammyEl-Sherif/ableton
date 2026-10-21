"""Tests for scripts/upload_plugin_release.py (SQL generation only; no network)."""
from __future__ import annotations

import importlib.util
import sys
import unittest
from dataclasses import replace
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
_spec = importlib.util.spec_from_file_location("upload_plugin_release", ROOT / "scripts" / "upload_plugin_release.py")
upload = importlib.util.module_from_spec(_spec)
assert _spec.loader is not None
sys.modules["upload_plugin_release"] = upload
_spec.loader.exec_module(upload)


def manifest(**overrides: object) -> "upload.Manifest":
    base = upload.Manifest(
        plugin_slug="honeybadger",
        version="0.1.0",
        platform="mac",
        file_name="HoneyBadger-0.1.0-mac-installer.pkg",
        storage_path="honeybadger/0.1.0/HoneyBadger-0.1.0-mac-installer.pkg",
        sha256="a" * 64,
        byte_size=4_194_304,
    )
    return replace(base, **overrides)


class PublishSqlTests(unittest.TestCase):
    def test_updates_the_active_row_and_inserts_on_first_release(self) -> None:
        sql = upload.sql_publish(manifest())
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
                upload.sql_publish(manifest(**override))


if __name__ == "__main__":
    unittest.main()
