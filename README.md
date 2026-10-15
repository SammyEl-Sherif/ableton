<h1 align="center">Animal Audio</h1>
<p align="center"><b>A series of VST3 plugins with juice, for Ableton Live and any other VST3 host.</b></p>

<p align="center">
  <img src="docs/images/duck-ui.png" alt="Duck by Animal Audio" width="760">
</p>

Every plugin in the series is built from the same toolbox: JUCE + CMake, shared DSP/UI libraries,
realtime-safe processing, and a bold, playful look with a bit of animal personality.

## The series

<!-- PLUGINS:START -->
| Plugin | What it does | Source | Status |
| --- | --- | --- | --- |
| **Duck** | Tempo-synced volume ducking with a draggable curve editor, 16 preset shapes and a big MIX knob | `plugins/duck` | Alpha: builds as VST3, 50 tests pass, passes pluginval level 10 on Linux; not yet tried in a real DAW |
<!-- PLUGINS:END -->

More animals are on the way. Adding one takes a single command, see [Adding a plugin](#adding-a-plugin).

## Duck

A volume shaper in the spirit of the classic sidechain-free "kick ducking" tools: it dips your signal
on every beat and lets it swim back up along a curve you draw. No sidechain routing, no kick needed.

- **Time divisions** `1/8`, `1/4`, `1/2`, `1/1`: one duck per note division, locked to the host's
  playhead (PPQ) and tempo.
- **Free-run fallback**: when the transport is stopped, or a host reports no position, Duck keeps
  going at the host BPM (or the *Fallback BPM* slider) so you can always hear what you are drawing.
  The status pill shows `HOST SYNC` or `FREE RUN`.
- **MIX** (the big rubber-duck knob): dry/wet of the *ducking effect*. 0% is a bit-exact bypass,
  100% applies the full curve.
- **Editable curve**: drag nodes, drag the diamond handles to bend a segment, double-click to add
  or remove nodes (up to 16). `FLIP` mirrors the curve in time, `INVERT` turns it upside down.
- **16 preset shapes** in two rows, plus prev/next buttons. A rubber duck rides the curve at the
  playhead, and the post-duck signal level is drawn behind it.
- **Click-free**: all gain changes (including hard-edged curves, mix and curve edits) go through
  a ~0.7 ms declick smoother. The audio thread never allocates, locks or does I/O.
- **State**: parameters and the curve are saved with your Live set / preset (APVTS ValueTree).

<p align="center">
  <img src="docs/images/duck-ui-pond-swell.png" alt="Duck with the Pond Swell preset at 1/8" width="620">
</p>

> Both screenshots are rendered by the UI code itself, headlessly, in the test suite
> (`AA_SCREENSHOT_DIR=docs/images ./build/plugins/duck/DuckTests "Editor renders headlessly"`).

## Build

Requirements: CMake 3.22+, a C++17 compiler, and Ninja (optional, recommended). JUCE and Catch2 are
downloaded by CMake at configure time (pinned in `cmake/FetchDependencies.cmake`).

**Linux** (Debian/Ubuntu)

```bash
sudo apt-get install ninja-build libasound2-dev libfreetype-dev libfontconfig1-dev libx11-dev \
  libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev libgl1-mesa-dev libcurl4-openssl-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

**macOS**: Xcode command line tools, then the same commands (`brew install ninja` if you like Ninja).

**Windows**: Visual Studio 2022 with the C++ workload, then from a Developer PowerShell:

```powershell
cmake -S . -B build
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Build just one plugin: `cmake --build build --target Duck_VST3`. Restrict what is configured with
`-DAA_PLUGINS=duck`.

## Install

The built bundle is `build/plugins/<name>/<Name>_artefacts/<config>/VST3/<Name>.vst3`. Copy it to:

| OS | VST3 folder |
| --- | --- |
| Windows | `C:\Program Files\Common Files\VST3\` |
| macOS | `~/Library/Audio/Plug-Ins/VST3/` |
| Linux | `~/.vst3/` |

Or configure with `-DAA_COPY_PLUGINS_AFTER_BUILD=ON` to have the build copy it for you. In Ableton Live,
enable *Preferences > Plug-Ins > Use VST3 Plug-In System Folders* and rescan. Duck is an audio effect:
drop it on any audio track.

## Repository layout

```
CMakeLists.txt          top level: fetches JUCE/Catch2, adds shared/ and every plugins/*
cmake/                  FetchDependencies.cmake, AnimalAudioPlugin.cmake (aa_add_plugin)
shared/dsp/             animalaudio::dsp  header-only C++17 DSP (smoothing, tempo sync, curves) + tests
shared/ui/              animalaudio::ui   brand badge, pill buttons, rotary knob, palette
plugins/<name>/         one folder per plugin: CMakeLists.txt, Source/, tests/
templates/plugin/       starter used by scripts/new_plugin.py
scripts/                new_plugin.py, format.sh
docs/images/            README screenshots
AGENTS.md               guide for AI agents and contributors (CLAUDE.md and .cursor/rules point here)
```

## Adding a plugin

```bash
python3 scripts/new_plugin.py Wobble --description "Tempo-synced stereo wobbler"
cmake -S . -B build
cmake --build build --target Wobble_VST3
```

This copies `templates/plugin` to `plugins/wobble`, picks a unique plugin code, and adds a row to the
table above. See [AGENTS.md](AGENTS.md) for conventions and the realtime-safety rules.

## Status and caveats

- Duck has been built as a real VST3 and exercised by unit/integration tests (DSP, tempo math, state
  round-trip, mouse editing, headless UI render) on Linux. The Linux VST3 also passes pluginval at strictness 10. It has not yet been loaded in Ableton Live
  or validated with Steinberg's own `validator`; macOS and Windows builds are configured in CI but
  unverified until the first workflow run.
- The repository has no open-source license yet (see [LICENSE](LICENSE)). JUCE and the VST3 SDK are
  AGPL/GPL or commercially licensed, so decide on licensing before distributing binaries.
- Duck is an original design inspired by the general idea of volume-shaper ducking tools. It does not
  use any third-party branding or assets.

<p align="center"><sub>Animal Audio</sub></p>
