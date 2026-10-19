#!/usr/bin/env python3
"""Upload a validated Duck release zip to Supabase Storage and update plugin_releases.

Requires the animal-audio-web repo linked to Supabase (`npx supabase link`).

    python3 scripts/upload_duck_release.py --manifest dist/releases/1.0.0/manifest-mac.json
    python3 scripts/upload_duck_release.py --manifest dist/releases/1.0.0/manifest-mac.json --zip dist/.../Duck-1.0.0-mac-installer.pkg

Environment:
    AA_WEB_REPO  Path to animal-audio-web checkout (default: sibling ../animal-audio-web)
"""
from __future__ import annotations

import argparse
import json
import os
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


def run(cmd: list[str], *, cwd: Path, dry_run: bool) -> None:
    print("$ " + " ".join(cmd))
    if dry_run:
        return
    subprocess.run(cmd, cwd=cwd, check=True)


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


def sql_update(m: Manifest) -> str:
    return (
        "update public.plugin_releases "
        f"set sha256 = '{m.sha256}', byte_size = {m.byte_size}, published_at = now() "
        f"where plugin_slug = '{m.plugin_slug}' and platform = '{m.platform}' and is_active = true;"
    )


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--manifest", type=Path, help="JSON from validate_duck_release.py --write-manifest")
    p.add_argument("--platform", choices=("mac", "win"))
    p.add_argument("--zip", type=Path)
    p.add_argument("--version", default="1.0.0")
    p.add_argument("--web-repo", type=Path, default=None, help="animal-audio-web root")
    p.add_argument("--dry-run", action="store_true")
    return p.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    web_repo = (args.web_repo or DEFAULT_WEB_REPO).expanduser().resolve()
    if not (web_repo / "supabase").is_dir():
        raise SystemExit(f"animal-audio-web not found at {web_repo} (set AA_WEB_REPO)")

    ensure_supabase_linked(web_repo, dry_run=args.dry_run)

    if args.manifest:
        manifest = Manifest.load(args.manifest.expanduser().resolve())
        if not manifest.sha256:
            raise SystemExit("manifest sha256 is empty; run validate_duck_release.py --write-manifest first")
        zip_path = args.zip.expanduser().resolve() if args.zip else ROOT / "dist" / "releases" / manifest.version / manifest.file_name
        if not zip_path.is_file():
            raise SystemExit(f"zip not found: {zip_path}")
    else:
        raise SystemExit("pass --manifest from the validate step (or run scripts/release_duck.sh --upload)")

    storage_dst = f"ss:///plugin-releases/{manifest.storage_path}"
    run(
        [
            "npx",
            "supabase",
            "storage",
            "cp",
            "--experimental",
            "--linked",
            "--content-type",
            "application/zip",
            "--yes",
            str(zip_path),
            storage_dst,
        ],
        cwd=web_repo,
        dry_run=args.dry_run,
    )

    query = sql_update(manifest)
    run(["npx", "supabase", "db", "query", "--linked", query], cwd=web_repo, dry_run=args.dry_run)

    print(f"\nUploaded {manifest.file_name} -> plugin-releases/{manifest.storage_path}")
    print(f"Updated plugin_releases ({manifest.platform}): sha256={manifest.sha256}, byte_size={manifest.byte_size}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
