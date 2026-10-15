#!/usr/bin/env bash
# Build, test and validate one plugin the way CI does, and write its README screenshots.
#
#   scripts/verify_plugin.sh Wobble [--no-screenshots]
#
# Needs: cmake, a C++17 compiler, Ninja (optional), xvfb-run (Linux, for headless UI tests) and, for the
# VST3 validation step, pluginval on PATH or in $PLUGINVAL (see .github/workflows/build.yml).
set -euo pipefail
cd "$(dirname "$0")/.."

name="${1:?usage: scripts/verify_plugin.sh <PluginName> [--no-screenshots]}"
lower="$(echo "$name" | tr '[:upper:]' '[:lower:]')"
screenshots=1
[ "${2:-}" = "--no-screenshots" ] && screenshots=0

build_dir="${AA_BUILD_DIR:-build}"
generator=()
command -v ninja >/dev/null && generator=(-G Ninja)

run() { if command -v xvfb-run >/dev/null && [ -z "${DISPLAY:-}" ]; then xvfb-run -a "$@"; else "$@"; fi; }

echo "== configure"
cmake -S . -B "$build_dir" "${generator[@]}" -DCMAKE_BUILD_TYPE=Release -DAA_BUILD_TESTS=ON

echo "== build ${name}"
cmake --build "$build_dir" --target "${name}_VST3" "${name}Tests" animalaudio_dsp_tests animalaudio_core_tests

echo "== ctest (shared + ${lower})"
run ctest --test-dir "$build_dir" --output-on-failure -R "^(dsp|core|${lower})\\."

if [ "$screenshots" = 1 ]; then
    echo "== screenshots -> docs/images"
    AA_SCREENSHOT_DIR="$PWD/docs/images" run "$build_dir/plugins/${lower}/${name}Tests" "${lower}: editor renders headlessly"
fi

echo "== pluginval (strictness 10)"
bundle="$(find "$build_dir" -type d -name "${name}.vst3" -prune | head -n 1)"
validator="${PLUGINVAL:-$(command -v pluginval || true)}"
if [ -z "$bundle" ]; then
    echo "VST3 bundle for ${name} not found" >&2
    exit 1
elif [ -z "$validator" ]; then
    echo "pluginval not found (set PLUGINVAL=/path/to/pluginval); skipping. Bundle: $bundle" >&2
else
    run "$validator" --strictness-level 10 --validate-in-process --timeout-ms 120000 "$bundle"
fi

echo "== ${name}: OK"
