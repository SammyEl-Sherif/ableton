#!/usr/bin/env python3
"""Upload a validated plugin release to Supabase Storage and publish it in plugin_releases.

    python3 scripts/upload_plugin_release.py --manifest dist/releases/duck/1.0.0/manifest-mac.json

The active row for (plugin_slug, platform) is updated in place; a plugin's first release inserts it, so new plugins
need no hand-written SQL.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_WEB_REPO = Path(os.environ.get("AA_WEB_REPO", str(ROOT.parent / "animal-audio-web")))


@dataclass
class Manifest:
    plugin_slug: str
    version: str
    platform: str
    file_name: str
    storage_path: str
    sha256: str
    byte_size: int

    @staticmethod
    def load(path: Path) -> "Manifest":
        data = json.loads(path.read_text(encoding="utf-8"))
        return Manifest(**data)

    def check(self) -> None:
        """Values are interpolated into SQL, so only accept the shapes the release pipeline produces."""
        rules = {
            "plugin_slug": r"[a-z0-9]+",
            "version": r"[0-9]+\.[0-9]+\.[0-9]+",
            "platform": r"mac|win",
            "file_name": r"[A-Za-z0-9._-]+",
            "storage_path": r"[a-z0-9]+/[0-9]+\.[0-9]+\.[0-9]+/[A-Za-z0-9._-]+",
            "sha256": r"[0-9a-fA-F]{64}",
        }
        for field, pattern in rules.items():
            value = getattr(self, field)
            if not isinstance(value, str) or not re.fullmatch(pattern, value):
                raise SystemExit(f"manifest {field} {value!r} is not a valid release value")
        if not isinstance(self.byte_size, int) or self.byte_size <= 0:
            raise SystemExit(f"manifest byte_size {self.byte_size!r} must be a positive integer")


def resolve_executable(name: str) -> str:
    # On Windows, npx is npx.cmd; CreateProcess does not apply PATHEXT to bare names.
    return shutil.which(name) or name


def redact(cmd: list[str]) -> list[str]:
    out = list(cmd)
    for i, arg in enumerate(out[:-1]):
        if arg in ("--token", "--password"):
            out[i + 1] = "***"
    return out


def run(cmd: list[str], *, cwd: Path, dry_run: bool) -> None:
    print("$ " + " ".join(redact(cmd)))
    if dry_run:
        return
    subprocess.run([resolve_executable(cmd[0]), *cmd[1:]], cwd=cwd, check=True)


def ensure_supabase_linked(web_repo: Path, *, dry_run: bool) -> None:
    token = os.environ.get("AA_SUPABASE_ACCESS_TOKEN") or os.environ.get("SUPABASE_ACCESS_TOKEN")
    if not token or dry_run:
        return
    ref = os.environ.get("AA_SUPABASE_PROJECT_REF", "huivhdradgroykfvtclx")
    run(["npx", "supabase", "login", "--token", token], cwd=web_repo, dry_run=False)
    link = ["npx", "supabase", "link", "--project-ref", ref, "--yes"]
    db_password = os.environ.get("AA_SUPABASE_DB_PASSWORD")
    if db_password:
        link.extend(["--password", db_password])
    run(link, cwd=web_repo, dry_run=False)


def sql_publish(m: Manifest) -> str:
    m.check()
    return (
        "with updated as ("
        "update public.plugin_releases "
        f"set version = '{m.version}', storage_path = '{m.storage_path}', file_name = '{m.file_name}', "
        f"sha256 = '{m.sha256}', byte_size = {m.byte_size}, published_at = now() "
        f"where plugin_slug = '{m.plugin_slug}' and platform = '{m.platform}' and is_active = true "
        "returning id) "
        "insert into public.plugin_releases "
        "(plugin_slug, version, platform, storage_path, file_name, sha256, byte_size, is_active) "
        f"select '{m.plugin_slug}', '{m.version}', '{m.platform}', '{m.storage_path}', '{m.file_name}', "
        f"'{m.sha256}', {m.byte_size}, true "
        "where not exists (select 1 from updated);"
    )


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--manifest", type=Path, required=True, help="JSON from validate_plugin_release.py")
    p.add_argument("--zip", type=Path, help="Artifact path (default: dist/releases/<slug>/<version>/<file>)")
    p.add_argument("--web-repo", type=Path, default=None, help="animal-audio-web root")
    p.add_argument("--dry-run", action="store_true")
    return p.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    web_repo = (args.web_repo or DEFAULT_WEB_REPO).expanduser().resolve()
    if not (web_repo / "supabase").is_dir():
        raise SystemExit(f"animal-audio-web not found at {web_repo} (set AA_WEB_REPO)")

    ensure_supabase_linked(web_repo, dry_run=args.dry_run)

    manifest = Manifest.load(args.manifest.expanduser().resolve())
    if not manifest.sha256:
        raise SystemExit("manifest sha256 is empty; run validate_plugin_release.py --write-manifest first")
    manifest.check()

    artifact_path = (
        args.zip.expanduser().resolve()
        if args.zip
        else ROOT / "dist" / "releases" / manifest.plugin_slug / manifest.version / manifest.file_name
    )
    if not artifact_path.is_file():
        raise SystemExit(f"artifact not found: {artifact_path}")

    content_type = "application/zip"
    if artifact_path.suffix == ".pkg":
        content_type = "application/octet-stream"

    storage_dst = f"ss:///plugin-releases/{manifest.storage_path}"
    # `storage cp` cannot overwrite (409 KeyAlreadyExists), and re-running a release for the same version
    # must work (rebuilt/re-signed artifact). `storage rm` exits 0 when the object does not exist.
    run(
        ["npx", "supabase", "storage", "rm", "--experimental", "--linked", "--yes", storage_dst],
        cwd=web_repo,
        dry_run=args.dry_run,
    )
    run(
        [
            "npx",
            "supabase",
            "storage",
            "cp",
            "--experimental",
            "--linked",
            "--content-type",
            content_type,
            "--yes",
            str(artifact_path),
            storage_dst,
        ],
        cwd=web_repo,
        dry_run=args.dry_run,
    )

    query = sql_publish(manifest)
    run(["npx", "supabase", "db", "query", "--linked", query], cwd=web_repo, dry_run=args.dry_run)

    print(f"\nUploaded {manifest.file_name} -> plugin-releases/{manifest.storage_path}")
    print(f"Published plugin_releases ({manifest.plugin_slug}, {manifest.platform}): "
          f"sha256={manifest.sha256}, byte_size={manifest.byte_size}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
