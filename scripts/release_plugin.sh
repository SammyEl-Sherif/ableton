#!/usr/bin/env bash
# End-to-end plugin release for one product (macOS today; Windows: release_win_plugin.ps1).
#
#   source ~/.appstoreconnect/notary.env
#   ./scripts/release_plugin.sh duck --upload
#   ./scripts/release_plugin.sh duck --validate-only --artifact dist/releases/duck/1.0.0/Duck-1.0.0-mac-installer.pkg
set -euo pipefail
repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"

plugin="${1:?usage: release_plugin.sh <slug> [options]}"
shift

skip_verify=0
skip_upload=1
validate_only=0
allow_unsigned=0
validate_artifact=""

while [ $# -gt 0 ]; do
  case "$1" in
    --skip-verify) skip_verify=1; shift ;;
    --skip-upload) skip_upload=1; shift ;;
    --upload) skip_upload=0; shift ;;
    --validate-only) validate_only=1; shift ;;
    --allow-unsigned) allow_unsigned=1; shift ;;
    --version)
      export AA_PLUGIN_VERSION="$2"
      shift 2
      ;;
    --artifact|--zip|--pkg)
      validate_artifact="$2"
      validate_only=1
      shift 2
      ;;
    -h|--help)
      sed -n '2,8p' "$0"
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      exit 1
      ;;
  esac
done

eval "$(PLUGIN="$plugin" python3 - <<'PY'
import os, shlex, sys
sys.path.insert(0, "scripts")
from lib.plugin_release import get_plugin
p = get_plugin(os.environ["PLUGIN"])
fields = {
    "SLUG": p.slug,
    "VERSION": p.version,
    "CMAKE_TARGET": p.cmake_target,
    "PKG_NAME": p.mac_pkg_name(),
    "DIST_DIR": str(p.release_root()),
}
for k, v in fields.items():
    print(f"{k}={shlex.quote(v)}")
PY
)"

version="${AA_PLUGIN_VERSION:-$VERSION}"
export AA_PLUGIN_VERSION="$version"
slug="$SLUG"
cmake_target="$CMAKE_TARGET"
dist_dir="$DIST_DIR"
pkg_name="$PKG_NAME"

mac_pkg="${dist_dir}/${pkg_name}"
manifest_mac="${dist_dir}/manifest-mac.json"

if [ "$validate_only" = "0" ]; then
  if [ "$(uname -s)" != "Darwin" ]; then
    echo "Full macOS release requires Darwin (sign + notarize)." >&2
    exit 1
  fi

  if [ "$skip_verify" = "0" ]; then
    echo "== verify ${cmake_target} (tests + pluginval)"
    scripts/verify_plugin.sh "$cmake_target" --release
  fi

  echo "== macOS release (${slug})"
  AA_PLUGIN_VERSION="$version" scripts/release_mac_plugin.sh "$slug"
fi

validate_artifact="${validate_artifact:-$mac_pkg}"
if [ ! -f "$validate_artifact" ]; then
  echo "Release artifact not found: $validate_artifact" >&2
  exit 1
fi

echo "== validate release artifact"
validate_args=(--plugin "$slug" --platform mac --artifact "$validate_artifact" --version "$version" --require-notarized --write-manifest "$manifest_mac")
if [ "$allow_unsigned" = "1" ]; then
  validate_args=(--plugin "$slug" --platform mac --artifact "$validate_artifact" --version "$version" --allow-unsigned --write-manifest "$manifest_mac")
fi
python3 scripts/validate_plugin_release.py "${validate_args[@]}"

if [ "$skip_upload" = "0" ]; then
  echo "== upload to Supabase"
  python3 scripts/upload_plugin_release.py --manifest "$manifest_mac" --zip "$validate_artifact"
fi

echo ""
echo "Release pipeline finished (${slug} ${version})."
echo "  artifact:  $validate_artifact"
echo "  manifest:  $manifest_mac"
