#!/usr/bin/env python3
"""Print a plugin's semver from plugins/<slug>/VERSION.

    python3 scripts/read_plugin_version.py duck
    python3 scripts/read_plugin_version.py duck --tag   # duck-v1.0.0
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

from lib.plugin_release import get_plugin, list_plugin_slugs  # noqa: E402


def main(argv: list[str] | None = None) -> int:
    slugs = list_plugin_slugs()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("slug", help=f"Plugin folder name ({', '.join(slugs) or 'none'})")
    parser.add_argument("--tag", action="store_true", help="Print GitHub release tag (slug-vX.Y.Z)")
    args = parser.parse_args(argv)
    if args.slug not in slugs:
        raise SystemExit(f"unknown plugin {args.slug!r}; available: {', '.join(slugs)}")
    info = get_plugin(args.slug)
    print(info.github_tag if args.tag else info.version)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
