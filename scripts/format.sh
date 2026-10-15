#!/usr/bin/env bash
# Format all C++ sources with the repo's .clang-format. Use --check to verify without modifying.
set -euo pipefail
cd "$(dirname "$0")/.."
files=$(git ls-files '*.h' '*.cpp' ; git ls-files --others --exclude-standard '*.h' '*.cpp')
if [ "${1:-}" = "--check" ]; then
    echo "$files" | xargs clang-format --dry-run --Werror
else
    echo "$files" | xargs clang-format -i
fi
