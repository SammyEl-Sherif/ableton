#!/usr/bin/env python3
"""Backward-compatible wrapper — prefer upload_plugin_release.py."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

from upload_plugin_release import main  # noqa: E402

if __name__ == "__main__":
    raise SystemExit(main())
