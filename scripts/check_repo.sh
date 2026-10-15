#!/usr/bin/env bash
# Mechanical repo-hygiene checks that CI also runs:
#   - skill mirrors are in sync (.cursor/skills -> .claude/skills)
#   - plugins don't include files from other plugins or from outside their own folder tree
#   - shared code doesn't depend on a plugin
set -euo pipefail
cd "$(dirname "$0")/.."

status=0

scripts/sync_skills.sh --check || status=1

if git ls-files 'plugins/*/Source/**' 'plugins/*/tests/**' | xargs -r grep -En '#include[[:space:]]+"(\.\./|.*plugins/)' ; then
    echo "error: plugin sources must not include files outside their own folder (extract shared code into shared/)" >&2
    status=1
fi

if git ls-files 'shared/**' | xargs -r grep -En '#include[[:space:]]+"?<?(plugins/|\.\./\.\./plugins)' ; then
    echo "error: shared code must not include plugin files" >&2
    status=1
fi

exit $status
