#!/usr/bin/env python3
"""Backward-compatible wrapper — prefer validate_plugin_release.py --plugin duck."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

from validate_plugin_release import main  # noqa: E402


def _argv_with_duck() -> list[str]:
    argv = sys.argv[1:]
    if "--plugin" not in argv:
        return ["--plugin", "duck", *argv]
    return argv


if __name__ == "__main__":
    raise SystemExit(main(_argv_with_duck()))
