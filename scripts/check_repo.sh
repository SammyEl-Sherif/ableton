#!/usr/bin/env bash
# Mechanical repo-hygiene checks that CI also runs:
#   - skill mirrors are in sync (.cursor/skills -> .claude/skills)
#   - plugins don't include files from other plugins or from outside their own folder tree
#   - shared code doesn't depend on a plugin
#   - scripts parse (bash -n, py_compile, PowerShell parser when pwsh exists) and the install-script tests pass
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

for script in scripts/*.sh; do
    bash -n "$script" || { echo "error: syntax error in $script" >&2; status=1; }
done

python_bin="$(command -v python3 || command -v python || true)"
if [ -n "$python_bin" ]; then
    for script in scripts/*.py scripts/lib/*.py scripts/tests/*.py; do
        "$python_bin" -m py_compile "$script" || { echo "error: syntax error in $script" >&2; status=1; }
    done
    "$python_bin" -m unittest discover -s scripts/tests || status=1
else
    echo "error: python is required for the script checks" >&2
    status=1
fi

if command -v pwsh >/dev/null; then
    for script in scripts/*.ps1 scripts/installers/win/*.ps1; do
        pwsh -NoProfile -Command "\$e = \$null; [void][System.Management.Automation.Language.Parser]::ParseFile('$script', [ref]\$null, [ref]\$e); if (\$e) { \$e | Out-String | Write-Host; exit 1 }" \
            || { echo "error: PowerShell syntax error in $script" >&2; status=1; }
    done
else
    echo "note: pwsh not found, skipping PowerShell syntax checks (CI has it)"
fi

find scripts -name __pycache__ -type d -prune -exec rm -rf {} + 2>/dev/null || true

exit $status
