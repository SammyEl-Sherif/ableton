#!/usr/bin/env bash
# End-to-end Duck release: verify → build/sign/notarize → validate .pkg → optional Supabase upload.
#
#   source ~/.appstoreconnect/notary.env
#   ./scripts/release_duck.sh --upload
#   ./scripts/release_duck.sh --validate-only --artifact dist/releases/1.0.0/Duck-1.0.0-mac-installer.pkg
set -euo pipefail
repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"

version="${AA_DUCK_VERSION:-1.0.0}"
skip_verify=0
skip_upload=1
validate_only=0
allow_unsigned=0
platform="mac"
validate_artifact=""

while [ $# -gt 0 ]; do
  case "$1" in
    --version)
      version="$2"
      shift 2
      ;;
    --skip-verify) skip_verify=1; shift ;;
    --skip-upload) skip_upload=1; shift ;;
    --upload) skip_upload=0; shift ;;
    --validate-only) validate_only=1; shift ;;
    --allow-unsigned) allow_unsigned=1; shift ;;
    --platform)
      platform="$2"
      shift 2
      ;;
    --artifact|--zip|--pkg)
      validate_artifact="$2"
      validate_only=1
      shift 2
      ;;
    -h|--help)
      sed -n '2,12p' "$0"
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      exit 1
      ;;
  esac
done

export AA_DUCK_VERSION="$version"
dist_dir="${repo_root}/dist/releases/${version}"
mac_pkg="${dist_dir}/Duck-${version}-mac-installer.pkg"
manifest_mac="${dist_dir}/manifest-mac.json"

if [ "$platform" != "mac" ]; then
  echo "Only --platform mac is automated here; Windows: package_win_duck.ps1 + validate --platform win" >&2
  exit 1
fi

if [ "$validate_only" = "0" ]; then
  if [ "$(uname -s)" != "Darwin" ]; then
    echo "Full macOS release requires Darwin (sign + notarize)." >&2
    exit 1
  fi

  if [ "$skip_verify" = "0" ]; then
    echo "== verify Duck (tests + pluginval)"
    scripts/verify_plugin.sh Duck --release
  fi

  echo "== macOS release (VST3 + .pkg installer)"
  scripts/release_mac_duck.sh
fi

validate_artifact="${validate_artifact:-$mac_pkg}"
if [ ! -f "$validate_artifact" ]; then
  echo "Release artifact not found: $validate_artifact" >&2
  exit 1
fi

echo "== validate release artifact"
validate_args=(--platform mac --artifact "$validate_artifact" --version "$version" --require-notarized --write-manifest "$manifest_mac")
if [ "$allow_unsigned" = "1" ]; then
  validate_args=(--platform mac --artifact "$validate_artifact" --version "$version" --allow-unsigned --write-manifest "$manifest_mac")
fi
python3 scripts/validate_duck_release.py "${validate_args[@]}"

if [ "$skip_upload" = "0" ]; then
  echo "== upload to Supabase"
  python3 scripts/upload_duck_release.py --manifest "$manifest_mac" --zip "$validate_artifact"
fi

echo ""
echo "Release pipeline finished."
echo "  installer: $validate_artifact"
echo "  manifest:  $manifest_mac"
