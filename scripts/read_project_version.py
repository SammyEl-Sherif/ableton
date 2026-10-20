#!/usr/bin/env python3
"""Print Duck's semver (backward compatibility).

Per-plugin versions live in plugins/<slug>/VERSION. Use read_plugin_version.py for other plugins.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

from lib.plugin_release import get_plugin  # noqa: E402


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag", action="store_true", help="Print duck-vX.Y.Z GitHub tag")
    args = parser.parse_args(argv)
    info = get_plugin("duck")
    print(info.github_tag if args.tag else info.version)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
