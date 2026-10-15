# CLAUDE.md

Read [AGENTS.md](AGENTS.md): it is the single source of truth for this repo's layout, build/test commands,
conventions, Animal Audio branding, how to add a plugin, and the audio-thread realtime-safety rules.

Quick reminders:

- Build: `cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure`
- New plugin: `python3 scripts/new_plugin.py <PascalName> --description "..."`
- Audio thread: no allocation, no blocking locks, no I/O; smooth every audible gain change.
- Keep AGENTS.md updated when you change layout, commands or conventions.
