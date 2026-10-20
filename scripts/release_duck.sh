#!/usr/bin/env bash
# Duck macOS release (wrapper). Prefer: ./scripts/release_plugin.sh duck
exec "$(dirname "$0")/release_plugin.sh" duck "$@"
