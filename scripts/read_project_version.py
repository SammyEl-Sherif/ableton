#!/usr/bin/env python3
"""Print the Animal Audio semver from CMakeLists.txt (release source of truth).

    python3 scripts/read_project_version.py
    python3 scripts/read_project_version.py --tag   # v1.0.0
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CMAKE = ROOT / "CMakeLists.txt"
_VERSION_RE = re.compile(
    r"project\s*\(\s*AnimalAudio\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)",
    re.MULTILINE,
)


def read_project_version(cmake_path: Path = CMAKE) -> str:
    text = cmake_path.read_text(encoding="utf-8")
    match = _VERSION_RE.search(text)
    if not match:
        raise ValueError(f"Could not parse project(AnimalAudio VERSION …) in {cmake_path}")
    return match.group(1)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag", action="store_true", help="Print v-prefixed tag name (e.g. v1.0.0)")
    parser.add_argument("--cmake", type=Path, default=CMAKE, help="Path to CMakeLists.txt")
    args = parser.parse_args(argv)
    version = read_project_version(args.cmake.expanduser().resolve())
    if args.tag:
        print(f"v{version}")
    else:
        print(version)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ValueError as exc:
        print(exc, file=sys.stderr)
        raise SystemExit(1) from exc
