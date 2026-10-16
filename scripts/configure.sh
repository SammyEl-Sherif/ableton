#!/usr/bin/env bash
# Configure the local build tree (Ninja + Release when available).
# On macOS, VST3 bundles copy to ~/Library/Audio/Plug-Ins/VST3/ after each plugin build.
set -euo pipefail
cd "$(dirname "$0")/.."

build_dir="${AA_BUILD_DIR:-build}"
generator=()
command -v ninja >/dev/null && generator=(-G Ninja)

copy_flag=()
if [[ "$(uname -s)" == Darwin ]]; then
    copy_flag=(-DAA_COPY_PLUGINS_AFTER_BUILD=ON)
fi

cmake -S . -B "$build_dir" "${generator[@]}" -DCMAKE_BUILD_TYPE=Release "${copy_flag[@]}" "$@"
