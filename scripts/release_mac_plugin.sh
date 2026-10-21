#!/usr/bin/env bash
# Sign, notarize, staple, and package one plugin for macOS distribution.
#
#   ./scripts/release_mac_plugin.sh duck
#
# Primary artifact for every plugin is a notarized .pkg (see build_mac_pkg.sh).
# Optional: AA_BUILD_MAC_ZIP=1 for a legacy zip alongside the pkg.
set -euo pipefail
plugin="${1:?usage: release_mac_plugin.sh <slug> (e.g. duck)}"
repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"
# shellcheck source=lib/mac_signing_identities.sh
source "$repo_root/scripts/lib/mac_signing_identities.sh"

eval "$(PLUGIN="$plugin" python3 - <<'PY'
import os, shlex, sys
sys.path.insert(0, "scripts")
from lib.plugin_release import get_plugin
p = get_plugin(os.environ["PLUGIN"])
fields = {
    "VERSION": p.version,
    "CMAKE_TARGET": p.cmake_target,
    "PRODUCT": p.product_name,
    "BUNDLE_NAME": p.bundle_name,
    "SLUG": p.slug,
    "PKG_NAME": p.mac_pkg_name(),
    "ZIP_NAME": p.mac_zip_name(),
    "DIST_DIR": str(p.release_root()),
    "BUNDLE_REL": p.bundle_build_rel(),
    "EULA_NAME": p.eula_file_name,
}
for k, v in fields.items():
    print(f"{k}={shlex.quote(v)}")
PY
)"

build_dir="${AA_BUILD_DIR:-build}"
bundle="${build_dir}/${BUNDLE_REL}"
dist_dir="${AA_DIST_DIR:-$DIST_DIR}"
version="${AA_RELEASE_VERSION:-$VERSION}"
version="${AA_PLUGIN_VERSION:-$version}"

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
  echo "== configure (Release, plugin=${SLUG})"
  generator=()
  command -v ninja >/dev/null && generator=(-G Ninja)
  cmake -S . -B "$build_dir" "${generator[@]}" -DCMAKE_BUILD_TYPE=Release -DAA_PLUGINS="${SLUG}" -DAA_LOCAL_BUILD=OFF
  echo "== build ${CMAKE_TARGET}_VST3"
  cmake --build "$build_dir" --target "${CMAKE_TARGET}_VST3" --config Release --parallel
fi

if [ ! -d "$bundle" ]; then
  echo "VST3 bundle not found: $bundle" >&2
  exit 1
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
cp -R "$bundle" "$work/${BUNDLE_NAME}"
bundle="$work/${BUNDLE_NAME}"

echo "== codesign"
codesign --force --deep --options runtime --timestamp --sign "$identity" "$bundle"
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
scripts/build_mac_pkg.sh "$SLUG" "$bundle" "$version" "$dist_dir"
out_pkg="${dist_dir}/${PKG_NAME}"
echo ""
echo "Primary Mac release (upload this): $out_pkg"
echo "Supabase Storage: ${SLUG}/${version}/${PKG_NAME}"

if [ "${AA_BUILD_MAC_ZIP:-0}" = "1" ]; then
  eula_name="$EULA_NAME"
  cp docs/EULA.md "$work/${eula_name}"
  cp docs/THIRD_PARTY_NOTICES.md "$work/Third-Party-Notices.txt"
  out_zip="${dist_dir}/${ZIP_NAME}"
  (
    cd "$work"
    zip -r -X "$out_zip" "${BUNDLE_NAME}" "${eula_name}" Third-Party-Notices.txt
  )
  shasum -a 256 "$out_zip"
  echo "Mac zip: $out_zip"
fi
