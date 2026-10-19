#!/bin/bash
# Installs Duck.vst3 into the current user's VST3 folder (no administrator password).
cd "$(dirname "$0")"
set -euo pipefail

if [ ! -d "Duck.vst3" ]; then
  osascript -e 'display alert "Duck install failed" message "Duck.vst3 not found next to this installer. Unzip the full download first." as critical'
  exit 1
fi

dest="${HOME}/Library/Audio/Plug-Ins/VST3"
mkdir -p "$dest"
ditto Duck.vst3 "${dest}/Duck.vst3"
xattr -dr com.apple.quarantine "${dest}/Duck.vst3" 2>/dev/null || true

osascript -e 'display alert "Duck installed" message "Duck is in your user VST3 folder.\n\nIn Ableton: Preferences → Plug-ins → enable Use VST3 Plug-In System Folders, then Rescan (Option/Alt for a full rescan)."'
