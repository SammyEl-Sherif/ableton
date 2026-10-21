---
name: setup
description: Build and install every plugin on this machine - check prerequisites, clean Release build of all plugins under plugins/, install the VST3 bundles (replacing stale copies, macOS quarantine and codesign), print build stamps, then guide the Ableton rescan. Use for /setup, a fresh clone, or when Ableton loads an old or missing build.
argument-hint: '[optional: plugin names, --system, --universal, --dry-run]'
---

# /setup: build and install every Animal Audio plugin on this machine

Request from the user (Claude Code fills this in; in Cursor it is whatever followed `/setup`): `$ARGUMENTS`

Goal: someone who just cloned the repo ends with every plugin under `plugins/` freshly built, installed where
their host looks, and a clear "rescan and test" instruction. The deterministic work is done by
**`scripts/install_plugins.py`** (cross-platform, standard library only; also usable without an agent). Your job is to
run it in the right order, read its output, fix what fails, and talk the user through the host steps. Do not
re-implement what the script does with ad-hoc commands, and do not hardcode plugin names: plugins are
auto-discovered from `plugins/`, so future plugins are included automatically.

Run from the repo root. Use `python3` (macOS/Linux) or `python` / `py -3` (Windows).

## 0. Where am I

- Confirm you are in the repo (`AGENTS.md`, `plugins/`, `scripts/install_plugins.py` exist). `git status` and
  `git log -1 --oneline`: note the commit. If the checkout is behind `origin`, say so and offer `git pull`
  (an old checkout is the most common reason for an "old build").
- Detect the OS (`uname -s`, or `$env:OS` on Windows). Parse `$ARGUMENTS`: plugin names mean `--only a,b`; keep
  `--system`, `--universal`, `--dry-run` for the script. If the user wants to preview, add `--dry-run` everywhere.

## 1. Prerequisites

```bash
python3 scripts/install_plugins.py --prereqs
```

It checks CMake >= 3.22, git, Ninja (optional), and per OS: macOS Xcode Command Line Tools; Windows Visual Studio
2022 Build Tools with the C++ workload; Linux a compiler plus the X11/ALSA/freetype/fontconfig/GL dev packages.
It prints the fix for anything missing. To install them:

```bash
python3 scripts/install_plugins.py --install-prereqs --yes     # brew (macOS), winget (Windows), apt (Debian/Ubuntu)
```

Caveats to tell the user before running it: it installs software, may need `sudo`/admin, the Xcode CLT installer
and the Visual Studio installer are interactive and slow, and Homebrew/winget must already exist. Ask first if the
user did not already agree. After it, re-run `--prereqs` until nothing is MISSING. On other Linux distros install
the equivalent packages by hand (list in README).

## 2. Clean Release build of ALL plugins, then install

```bash
python3 scripts/install_plugins.py --build --clean
```

- `--build` configures Release (Ninja when present; the Visual Studio generator on Windows), builds every plugin
  under `plugins/` with tests off, and installs every produced `.vst3`. `--clean` deletes `build/` first so no stale
  object files can produce an old binary. First configure downloads JUCE (network needed once).
- macOS defaults to the machine's own architecture. Add `--universal` if the user's Live runs under Rosetta
  (Intel Live, or Live started in Rosetta mode): an arm64-only plugin will not load there.
- Install folders (override with `--system` / `--user` / `--prefix DIR`):
  - macOS `~/Library/Audio/Plug-Ins/VST3` (machine-wide: `/Library/Audio/Plug-Ins/VST3`, needs `sudo`);
  - Windows `%COMMONPROGRAMFILES%\VST3` when the terminal is elevated, otherwise `%LOCALAPPDATA%\Programs\Common\VST3`
    (hosts scan both; for the system folder re-run in an Administrator terminal with `--system`);
  - Linux `~/.vst3`.
- The script overwrites the existing copy of each plugin, **deletes stale copies of the same plugins from the
  other standard VST3 folders** (a host may load those instead), clears the macOS quarantine attribute and applies an
  ad-hoc `codesign` so Ableton will load a locally built bundle. Use `--keep-stale` to leave other folders alone.
  Removing from a system folder may need `sudo` / Administrator: the script prints the exact hint instead of failing.
- If it fails, read the compiler output, fix the cause (missing package, wrong CMake generator in an old `build/`:
  `--clean` fixes that), and re-run. Do not paper over errors.
- Build failure that is not environment related: it is a repo bug. Say so and offer to fix it.

## 3. Report what was installed

The script ends by printing each installed path and its **build stamp** (`version | git SHA | date`, read straight
from the installed binary; `+dirty` means built with uncommitted changes). Show the user that list verbatim. It
also warns if a bundle's SHA is not the checkout's commit. `python3 scripts/install_plugins.py --list` shows every
copy on the machine at any time, which is the quickest way to find a stale or duplicate install.

The same stamp is visible in the plugin UI: hover the ANIMAL AUDIO badge, or read the small text at the bottom of the
left panel. That is how the user confirms the host loaded the build you just installed.

## 4. Tell the user what to do in Ableton Live (say this explicitly)

1. Open **Live > Preferences > Plug-ins** (Windows: Options > Preferences) and make sure
   **Use VST3 Plug-In System Folders** is **on**.
2. Click **Rescan**. If a plugin does not appear, or still looks like an old build, hold **Alt** (Windows) /
   **Option** (macOS) while clicking Rescan for a full rescan. If it still shows the old build, quit and reopen Live.
3. Find the plugins under **Plug-Ins > VST3** (Animal Audio), drop one on an audio track and test it.
4. Check the build stamp in the plugin matches what the script printed.
5. Tracks that used an older build keep their saved state, but the UI/DSP come from the newly loaded binary; if
   a project still behaves like the old build, delete the device and add it again.

macOS only: if Live says the plugin cannot be loaded or is damaged, run
`xattr -dr com.apple.quarantine <path printed above>` and `codesign --force --deep --sign - <path>` (the script already
does this), then rescan. Local builds are not notarized; signed + notarized builds for animal.audio come only from the maintainer
`/release` skill (docs/RELEASE.md).

Next, if the user wants to make a plugin: the `/vst <name or idea>` skill (lifecycle: AGENTS.md "Plugin lifecycle").

## 5. Other operations

| Goal | Command |
| --- | --- |
| Install what is already built (no rebuild) | `python3 scripts/install_plugins.py` |
| Rebuild + reinstall after `git pull` | `python3 scripts/install_plugins.py --build --clean` |
| Show installed copies and build stamps | `python3 scripts/install_plugins.py --list` |
| Remove every Animal Audio plugin | `python3 scripts/install_plugins.py --uninstall` (then rescan) |
| Preview anything | add `--dry-run` |
| One plugin only | `--only duck` |

## 6. Keeping this skill honest

- The canonical file is `.cursor/skills/setup/SKILL.md`; after editing run `scripts/sync_skills.sh` (CI checks the
  `.claude/skills/setup/SKILL.md` mirror).
- Behaviour belongs in `scripts/install_plugins.py` (tests: `scripts/tests/test_install_plugins.py`, run by
  `scripts/check_repo.sh` and CI), not in this prose. If you change what a step does, change the script and its tests.
- State plainly what you verified. Building and installing can be verified by running the script; loading in
  Ableton cannot, so never claim the plugin works in Live: ask the user to do step 4 and report back.
