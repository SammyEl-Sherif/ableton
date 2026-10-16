---
name: vst
description: Create a new Animal Audio VST3 plugin end to end in this monorepo (scaffold, DSP, animal-themed UI with its own palette, tests, build, pluginval, docs, PR). Use when the user runs /vst <name or idea> or asks for a new plugin in the series. Enforces the shared-library (DRY) policy.
argument-hint: <plugin name or idea, e.g. "Otter: a tempo-synced stereo wobbler">
---

# /vst: build a new Animal Audio plugin

Request from the user (Claude Code fills this in; in Cursor it is whatever followed `/vst`): `$ARGUMENTS`

You are adding one plugin to the Animal Audio monorepo. Read `AGENTS.md` first (layout, commands,
realtime-safety rules, branding). This skill is the workflow; AGENTS.md is the reference.

Non-negotiables:

1. **DRY.** Never copy code from Duck (or any plugin). Reuse what is in `shared/`; when you need something
   that exists only in another plugin, or that you are about to write a second time, **extract it to
   `shared/` first** (see "Reuse and extraction"). Plugins must not include files from other plugins.
2. **Same layout language, different personality.** Every plugin looks like a sibling of Duck (left panel
   with brand badge, big NAME, mascot and big MIX knob; right side with division pills, a curve/visual
   panel and a preset row) but has its **own colour scheme and its own animal**. Layout lives in
   `ui::ShaperEditor`; colour and animal live in the plugin's `Theme`.
3. **Realtime-safe, click-free, state-compatible.** Follow the audio-thread rules in AGENTS.md. Parameter
   IDs and plugin codes are saved-state contract.
4. **Prove it.** Build, run ctest, run pluginval, look at the screenshot. Do not claim DAW validation you
   did not do.

## 1. Pick and confirm the concept

- Parse `$ARGUMENTS` into: **name** (one animal word, PascalCase, not already in `plugins/`) and **concept**
  (what the effect does to audio, in one sentence). If only an idea is given, choose the animal whose
  behaviour matches the effect (Duck dips, a Cheetah gates fast, a Sloth smooths slowly, a Bat flutters...).
- Interactive session: confirm name + concept in one short question if they are genuinely ambiguous.
  Non-interactive (cloud agent): decide, and write the assumption in the PR description.
- Check what exists: `ls plugins`, `grep -rn kThemeHue plugins/*/Source/ui/`, README plugin table.
  The animal and the hue must both be new.

## 2. Scaffold

```bash
git fetch origin main && git checkout -b <branch-per-repo-conventions> origin/main
python3 scripts/new_plugin.py <Name> --description "<one line>" [--hue <0-360>]
```

This gives a plugin that already builds and passes the standard suites: a `GainShaperProcessor` (curve-driven
gain, like Duck), `ShaperEditor` with a hue-derived palette, 16 placeholder presets and tests. The script
picks a hue far from the existing plugins and a unique 4-char plugin code. Run
`scripts/verify_plugin.sh <Name> --no-screenshots` once now so you start from green.

## 3. Reuse survey (before writing any DSP/UI code)

Open this table and decide, for every piece the concept needs, whether it already exists.

| Need | Already in shared/ |
| --- | --- |
| smoothing / declick | `dsp/Smoothing.h` `OnePoleSmoother` |
| tempo divisions, host-locked or free-run phase | `dsp/TempoSync.h` `Division`, `PhaseTracker`; `core/TransportReader.h` |
| editable curve model + realtime LUT | `dsp/ShapeCurve.h` `ShapeCurve`, `CurveTable` |
| curve stored in plugin state, published to audio thread | `core/CurveState.h` |
| curve to gain engine with scope data | `dsp/CurveGainShaper.h`, `dsp/CycleScope.h` |
| standard params (division, mix, fallbackBpm), state save/restore, phase/transport plumbing | `core/CurveShaperProcessor.h` (`GainShaperProcessor` for gain effects) |
| 16 stock curve shapes | `dsp/StandardPresets.h` (you supply names) |
| whole editor | `ui/ShaperEditor.h` driven by `ui/Theme.h` (`Palette` + `Motif`) |
| widgets | `BrandBadge`, `PillButton`, `ChoicePillRow`, `RotaryKnob`/`ThemedKnob`, `CurveEditor`, `CurvePresetPanel`, `Drawing.h` |
| test suites | `shared/testing`: `AA_SHAPER_PLUGIN_TESTS`, `AA_GAIN_SHAPER_PLUGIN_TESTS` |

Skim the headers (`ls shared/*/include/animalaudio/*`) rather than trusting this table; it can be stale.
If your effect needs something the base class does not offer (another parameter, a different DSP,
a second visual), prefer extending the shared class with a hook or a template over forking it.

## 4. Theme: palette + animal (the "different colour scheme and animal")

Everything plugin-specific about looks is in `plugins/<name>/Source/ui/<Name>Theme.{h,cpp}` and the Motif.

**Palette convention** (`animalaudio::ui::Palette`, roles are fixed, values are yours):

| Role | Use | Duck example |
| --- | --- | --- |
| `background`, `panel`, `panelRaised`, `outline` | dark, low-contrast hue-tinted surfaces | warm charcoal |
| `text`, `textDim` | labels, read-outs | cream |
| `accent` | primary highlight: knob arc, selected pill, curve, title | duck yellow |
| `accentAlt` | gradient partner of the accent | bill orange |
| `onAccent` | text on top of `accent` | near-black |
| `detail` | inactive bend handles, ornaments | bill |
| `ambient` | animated background motif (ripples), kept faint | pond blue |

Rules:

- `kThemeHue` in `<Name>Theme.h` is the plugin's signature hue; `new_plugin.py` reads it to keep hues
  at least ~30 degrees apart. Keep it in sync with the palette you end up with.
- Start from `makePaletteFromHue(kThemeHue)`. Hand-tune only what looks off (write the struct out like
  Duck once the design settles). Keep dark surfaces, one saturated accent, and readable contrast
  (`text` on `panel`, `onAccent` on `accent`).
- Colours are **only** in the Theme/Motif files. No colour literals in shared code or elsewhere in the plugin.

**Animal convention** (`ui::Motif` subclass in `Source/ui/<Name>Motif.{h,cpp}`, set `theme.motif`):

- `drawMascot` (title mascot ~62 px and the small one riding the curve at the playhead), `drawKnobCap`
  (the big MIX knob; the animal's face/nose is the value pointer), `drawAmbient` (animated background,
  e.g. water for Duck, grass for a Rabbit, sound waves for a Bat), `drawDecoration` (faint ornaments).
  All are optional; the defaults are a paw, a plain cap and ripples.
- Draw with `juce::Path` and colours from the passed `Palette` only. No bitmaps, no external fonts.
- Set `theme.name` (upper-case animal) and `theme.tagline` (what it does, e.g. "VOLUME SHAPER", "STEREO WOBBLER").
- Rename the 16 presets in `<Name>Presets.cpp` with animal-flavoured names (keep the count and order).
- Do **not** move or restyle the layout per plugin. If the shared layout is the limit (e.g. you need a
  fourth control), extend `ShaperEditor` so all plugins gain it.

## 5. DSP

- Curve-driven gain effect: nothing to write beyond presets/theme. Tune if the concept differs.
- Other effects: derive from `core::CurveShaperProcessor` and implement `prepareEffect`,
  `curveTableChanged`, `renderCycle` (it hands you phase, increment and mix per block). Put JUCE-free DSP in
  `shared/dsp` if any other plugin could use it, otherwise `plugins/<name>/Source/dsp`, with numeric unit
  tests (bounds, periodicity, click-freeness, block-size independence, mix 0 = bypass).
- Extra parameters: add them in your processor with new, versioned `ParameterID`s; never rename the shared ones.
- Audio-thread rules (AGENTS.md): no allocation/locks/I/O, smooth every audible change, denormals, any block
  size, transport may be absent.

## 6. Reuse and extraction (the DRY policy)

Rule of two: **the second use of a piece of code is the moment it moves to `shared/`.** Do the move
first, as its own commit, then use it from both places.

Decision checklist (answer for every non-trivial class/function/constant you write or find in Duck):

1. **Search first.** `rg` shared/ and plugins/ for the idea, not just the name. If it exists, use it.
2. **Is this the second use?** (another plugin already has it, or you are about to write it twice) -> extract now.
3. **Is it first-use but obviously generic?** (math, a widget, plumbing with no animal in it) -> still write it
   in the right shared library with a plugin-agnostic API, because the next plugin will need it.
4. **Is it data or personality?** (colours, names, presets' names, mascot drawing, tagline, a plugin's own DSP
   maths) -> stays in the plugin, behind the Theme/Motif/constructor-argument seam.
5. **Where does it go?** JUCE-free -> `shared/dsp`; JUCE but no GUI -> `shared/core`; GUI -> `shared/ui`;
   test helpers -> `shared/testing`; build logic -> `cmake/`; repetitive file scaffolding -> `templates/plugin`.
6. **How is it parameterised?** Prefer, in order: values from `Theme`/`Palette`, constructor/function arguments,
   a virtual hook (`Motif`, processor hooks), a template parameter. Never `#ifdef PLUGIN_X` and never a copy with edits.
7. **Does it change another plugin?** It must not: after extracting, Duck's tests pass unchanged and its
   headless screenshot looks the same (regenerate and compare `docs/images/duck-ui*.png`). Saved-state
   identifiers (parameter IDs, ValueTree identifiers) stay byte-identical.
8. **Move the tests with the code** (to the shared library's `tests/` or into `shared/testing` suites), and add
   a test for the new seam.
9. **Update the docs in the same PR**: AGENTS.md layout section, and the skill's reuse table if the inventory changed.
10. **Don't over-extract.** Two things that merely look alike but must diverge stay separate. No speculative
    options nobody uses; no abstraction for a single caller unless item 3 applies.

Red flags in review: a file that is mostly a Duck file with names changed; `#include` paths that leave the
plugin folder (other than `animalaudio/...`); colour literals outside Theme/Motif files; the same helper
in two plugins; shared code mentioning a plugin or an animal by name.

`scripts/check_repo.sh` enforces the mechanical parts (no cross-plugin includes, skill mirrors in sync).

## 7. Tests

- Keep the macro lines in `tests/<Name>Tests.cpp` (`AA_SHAPER_PLUGIN_TESTS`, `AA_REAL_MOUSE_PLUGIN_TESTS`, and
  `AA_GAIN_SHAPER_PLUGIN_TESTS` while the processor is a `GainShaperProcessor`). They cover parameters, state
  round-trip, division selector (hit-test, click, automation), preset panel, curve mouse editing, a headless render,
  and real mouse events through a native window at several UI scales (run ctest under `xvfb-run -a` on Linux).
- Add tests for everything plugin-specific: your DSP (JUCE-free where possible), extra parameters and
  their state round-trip, anything you changed in the shared libs (tests belong next to the shared code).
- A regression found in one plugin becomes a shared suite test, so every plugin gets it.

## 8. Verify

```bash
scripts/format.sh                                  # CI runs clang-format --dry-run --Werror
PLUGINVAL=/path/to/pluginval scripts/verify_plugin.sh <Name>
```

`verify_plugin.sh` configures, builds `<Name>_VST3` + tests, runs ctest (`dsp`, `core`, `<name>`), writes the
headless screenshots to `docs/images/<name>-ui*.png` and runs pluginval at strictness 10 (download from
https://github.com/Tracktion/pluginval/releases if missing; on Linux it needs `xvfb-run`).
Then **look at** `docs/images/<name>-ui.png`: distinct palette from the other plugins, animal drawn,
nothing clipped, text readable. Also run the whole ctest suite so Duck and the shared tests stay green, and
`scripts/check_repo.sh`.

## 9. Docs

- README: the plugin-table row (the script adds a stub; replace the status with what you really verified),
  a short section like Duck's with the screenshot, and the layout/test count if it changed.
- AGENTS.md: add the plugin to the table, a short "<Name> specifics" section if it has its own DSP or
  parameters, and any new shared piece in the layout section.
- Remove screenshots you generated for variants you do not reference.

## 10. Commit, push, PR

- Commit per logical change: (a) shared extractions, (b) the plugin, (c) docs/screenshots.
- Push the branch, open the PR to `main` (draft is fine). Description: what the plugin does, animal + palette
  hue, what was extracted to shared/ (and which plugins it touched), exactly what was verified (ctest count,
  pluginval level, screenshot) and what was not (macOS/Windows, real DAW), assumptions you made.
- Final report to the user, briefly: PR link, what was built, what was shared/extracted, verification, caveats.
