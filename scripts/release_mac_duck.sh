#!/usr/bin/env bash
# Duck macOS sign/notarize (wrapper). Prefer: ./scripts/release_mac_plugin.sh duck
exec "$(dirname "$0")/release_mac_plugin.sh" duck
