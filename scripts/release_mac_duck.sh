#!/usr/bin/env bash
# Sign, notarize, staple, and zip Duck.vst3 for distribution on animal.audio.
#
# Prerequisites: docs/APPLE_DEVELOPER_SETUP.md
#
#   export AA_CODESIGN_IDENTITY="Developer ID Application: …"
#   export AA_INSTALLER_CODESIGN_IDENTITY="Developer ID Installer: …"
#   export AA_NOTARY_KEY="$HOME/.appstoreconnect/AuthKey_XXXX.p8"
#   export AA_NOTARY_KEY_ID="..."
#   export AA_NOTARY_ISSUER="..."
#   ./scripts/release_mac_duck.sh
#
# Optional: AA_DUCK_VERSION=1.0.0  AA_BUILD_DIR=build  AA_SKIP_BUILD=1
set -euo pipefail
repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"
# shellcheck source=lib/mac_signing_identities.sh
source "$repo_root/scripts/lib/mac_signing_identities.sh"

version="${AA_DUCK_VERSION:-$(python3 scripts/read_project_version.py)}"
build_dir="${AA_BUILD_DIR:-build}"
bundle_rel="plugins/duck/Duck_artefacts/Release/VST3/Duck.vst3"
bundle="${build_dir}/${bundle_rel}"
zip_name="Duck-${version}-mac-vst3.zip"
pkg_name="Duck-${version}-mac-installer.pkg"
dist_dir="${AA_DIST_DIR:-${repo_root}/dist/releases/${version}}"

identity="${AA_CODESIGN_IDENTITY:-}"
notary_key="${AA_NOTARY_KEY:-}"
notary_key_id="${AA_NOTARY_KEY_ID:-}"
notary_issuer="${AA_NOTARY_ISSUER:-}"

require_env() {
  if [ -z "${!1:-}" ]; then
    echo "Missing env: $1 (see docs/APPLE_DEVELOPER_SETUP.md)" >&2
    exit 1
  fi
}

require_env AA_CODESIGN_IDENTITY
require_env AA_NOTARY_KEY
require_env AA_NOTARY_KEY_ID
require_env AA_NOTARY_ISSUER
mac_assert_application_identity "$identity"

if [ "${AA_SKIP_BUILD:-0}" != "1" ]; then
  echo "== configure (Release)"
  generator=()
  command -v ninja >/dev/null && generator=(-G Ninja)
  cmake -S . -B "$build_dir" "${generator[@]}" -DCMAKE_BUILD_TYPE=Release -DAA_PLUGINS=duck -DAA_LOCAL_BUILD=OFF
  echo "== build Duck_VST3"
  cmake --build "$build_dir" --target Duck_VST3 --config Release --parallel
fi

if [ ! -d "$bundle" ]; then
  echo "VST3 bundle not found: $bundle" >&2
  exit 1
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
cp -R "$bundle" "$work/Duck.vst3"
bundle="$work/Duck.vst3"

echo "== codesign"
codesign --force --deep --options runtime --timestamp \
  --sign "$identity" \
  "$bundle"
codesign --verify --deep --strict --verbose=2 "$bundle"

submit_zip="$work/submit.zip"
echo "== zip for notarization"
ditto -c -k --keepParent "$bundle" "$submit_zip"

echo "== notarytool submit (wait for Accepted)"
xcrun notarytool submit "$submit_zip" \
  --key "$notary_key" \
  --key-id "$notary_key_id" \
  --issuer "$notary_issuer" \
  --wait

echo "== staple"
xcrun stapler staple "$bundle"

echo "== spctl verify"
spctl -a -vv -t install "$bundle"

mkdir -p "$dist_dir"

echo "== macOS .pkg installer (primary Mac download)"
scripts/build_mac_duck_pkg.sh "$bundle" "$version" "$dist_dir"
out_pkg="${dist_dir}/${pkg_name}"

if [ "${AA_BUILD_MAC_ZIP:-0}" = "1" ]; then
  cp docs/EULA.md "$work/Animal-Audio-Duck-EULA.txt"
  cp docs/THIRD_PARTY_NOTICES.md "$work/Third-Party-Notices.txt"
  out_zip="${dist_dir}/${zip_name}"
  (
    cd "$work"
    zip -r -X "$out_zip" Duck.vst3 Animal-Audio-Duck-EULA.txt Third-Party-Notices.txt
  )
  shasum -a 256 "$out_zip"
  echo "Optional zip: $out_zip"
fi

echo ""
echo "Primary Mac release (upload this): $out_pkg"
echo "Supabase Storage: duck/${version}/${pkg_name}"
