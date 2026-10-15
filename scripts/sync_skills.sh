#!/usr/bin/env bash
# `.cursor/skills/<name>/SKILL.md` is canonical; Claude Code reads `.claude/skills/<name>/SKILL.md`.
# Copies (no symlinks, so Windows checkouts work). `--check` fails if a mirror is out of date.
set -euo pipefail
cd "$(dirname "$0")/.."

status=0
for source in .cursor/skills/*/SKILL.md; do
    name="$(basename "$(dirname "$source")")"
    mirror=".claude/skills/$name/SKILL.md"
    if [ "${1:-}" = "--check" ]; then
        if ! cmp -s "$source" "$mirror"; then
            echo "out of sync: $mirror (run scripts/sync_skills.sh)" >&2
            status=1
        fi
    else
        mkdir -p "$(dirname "$mirror")"
        cp "$source" "$mirror"
        echo "synced $mirror"
    fi
done
exit $status
