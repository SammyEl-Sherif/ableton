# CLAUDE.md

Read [AGENTS.md](AGENTS.md): it is the single source of truth for this repo's layout, build/test commands,
conventions, Animal Audio branding, how to add a plugin, and the audio-thread realtime-safety rules.

Quick reminders:

- Build: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
- New plugin: run the `/vst <name or idea>` skill (`.claude/skills/vst`), or `python3 scripts/new_plugin.py <PascalName> --description "..."`
- Set up on a user's machine: run the `/setup` skill (`.claude/skills/setup`), or `python3 scripts/install_plugins.py --build --clean`
  (builds + installs every plugin under `plugins/`; `--list`, `--uninstall`, `--dry-run`).
- DRY: second use of code moves it to `shared/` (dsp/core/ui/testing); never copy from another plugin. Per-plugin
  look = `Theme` (own palette + animal `Motif`), layout is shared (`ShaperEditor`).
- Skills' canonical files are `.cursor/skills/<name>/SKILL.md`; after editing one run `scripts/sync_skills.sh`.
- Audio thread: no allocation, no blocking locks, no I/O; smooth every audible gain change.
- Keep AGENTS.md updated when you change layout, commands or conventions.
