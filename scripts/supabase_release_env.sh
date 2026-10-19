#!/usr/bin/env bash
# Log in and link Supabase CLI for release uploads (CI or fresh machines).
# Requires: AA_SUPABASE_ACCESS_TOKEN (or SUPABASE_ACCESS_TOKEN)
# Optional: AA_SUPABASE_PROJECT_REF (default huivhdradgroykfvtclx), AA_SUPABASE_DB_PASSWORD
set -euo pipefail
web_repo="${1:?animal-audio-web path}"
token="${AA_SUPABASE_ACCESS_TOKEN:-${SUPABASE_ACCESS_TOKEN:-}}"
ref="${AA_SUPABASE_PROJECT_REF:-huivhdradgroykfvtclx}"
if [ -z "$token" ]; then
  echo "No Supabase access token in env; assuming project is already linked in $web_repo" >&2
  exit 0
fi
cd "$web_repo"
npx supabase login --token "$token"
link=(npx supabase link --project-ref "$ref" --yes)
if [ -n "${AA_SUPABASE_DB_PASSWORD:-}" ]; then
  link+=(--password "$AA_SUPABASE_DB_PASSWORD")
fi
"${link[@]}"
