#!/usr/bin/env bash
# Build a signed, notarized macOS installer (.pkg) for one plugin — standard Installer.app flow:
# Welcome → License → choose install location → admin password → install → conclusion + alert.
#
# Usage (stapled <Product>.vst3):
#   ./scripts/build_mac_pkg.sh duck /path/to/Duck.vst3 1.0.0 /path/to/dist
#   ./scripts/build_mac_pkg.sh duck /path/to/Duck.vst3 1.0.0 /tmp/out --render-only   # installer resources only, no signing
#
# Installer resources in scripts/installers/mac/ are templates (__DISPLAY_NAME__, __BUNDLE__, __SLUG__, __PKG_ID__,
# __VERSION__, __INSTALL_KBYTES__) filled from plugins/<slug>/ (see scripts/lib/plugin_release.py).
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
plugin="${1:?usage: build_mac_pkg.sh <slug> <stapled bundle> <version> <dist dir> [--render-only]}"
bundle="${2:?stapled <Product>.vst3 path}"
version="${3:?version e.g. 1.0.0}"
dist_dir="${4:?output directory}"
render_only=0
[ "${5:-}" = "--render-only" ] && render_only=1

eval "$(PLUGIN="$plugin" REPO_ROOT="$repo_root" python3 - <<'PY'
import os, shlex, sys
sys.path.insert(0, os.path.join(os.environ["REPO_ROOT"], "scripts"))
from lib.plugin_release import get_plugin
p = get_plugin(os.environ["PLUGIN"])
fields = {
    "SLUG": p.slug,
    "BUNDLE_NAME": p.bundle_name,
    "DISPLAY_NAME": p.display_name,
    "PKG_ID": p.mac_pkg_identifier,
}
for k, v in fields.items():
    print(f"{k}={shlex.quote(v)}")
PY
)"

if [ ! -d "$bundle" ]; then
  echo "Bundle not found: $bundle" >&2
  exit 1
fi

install_kbytes="$(du -sk "$bundle" | awk '{print $1}')"

render() {
  sed -e "s|__DISPLAY_NAME__|${DISPLAY_NAME}|g" \
      -e "s|__BUNDLE__|${BUNDLE_NAME}|g" \
      -e "s|__SLUG__|${SLUG}|g" \
      -e "s|__PKG_ID__|${PKG_ID}|g" \
      -e "s|__VERSION__|${version}|g" \
      -e "s|__INSTALL_KBYTES__|${install_kbytes}|g" \
      "$1" > "$2"
}

render_resources() {
  local out="$1"
  mkdir -p "$out/resources" "$out/scripts"
  render "$repo_root/scripts/installers/mac/postinstall" "$out/scripts/postinstall"
  chmod +x "$out/scripts/postinstall"
  cp "$repo_root/docs/EULA.md" "$out/resources/EULA.txt"
  render "$repo_root/scripts/installers/mac/welcome.html" "$out/resources/welcome.html"
  render "$repo_root/scripts/installers/mac/conclusion.html" "$out/resources/conclusion.html"
  render "$repo_root/scripts/installers/mac/Distribution.xml" "$out/Distribution.xml"
}

if [ "$render_only" = "1" ]; then
  render_resources "$dist_dir"
  echo "Rendered installer resources for ${SLUG}: $dist_dir"
  exit 0
fi

# shellcheck source=lib/mac_signing_identities.sh
source "$repo_root/scripts/lib/mac_signing_identities.sh"
for var in AA_CODESIGN_IDENTITY AA_NOTARY_KEY AA_NOTARY_KEY_ID AA_NOTARY_ISSUER; do
  if [ -z "${!var:-}" ]; then
    echo "Missing env: $var" >&2
    exit 1
  fi
done

mkdir -p "$dist_dir"
pkg_name="${BUNDLE_NAME%.vst3}-${version}-mac-installer.pkg"
signed="${dist_dir}/${pkg_name}"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

render_resources "$work"
component_root="$work/component-root/Library/Audio/Plug-Ins/VST3"
mkdir -p "$component_root"
ditto "$bundle" "$component_root/${BUNDLE_NAME}"

component_pkg="$work/${SLUG}-component.pkg"
component_plist="$work/components.plist"
# Never relocatable: Installer must write to the VST3 folder hosts scan, not "upgrade" a bundle
# with the same bundle ID found elsewhere on disk (downloads, build dirs).
cat > "$component_plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<array>
  <dict>
    <key>RootRelativeBundlePath</key>
    <string>Library/Audio/Plug-Ins/VST3/${BUNDLE_NAME}</string>
    <key>BundleIsRelocatable</key>
    <false/>
    <key>BundleIsVersionChecked</key>
    <true/>
    <key>BundleHasStrictIdentifier</key>
    <true/>
    <key>BundleOverwriteAction</key>
    <string>upgrade</string>
  </dict>
</array>
</plist>
PLIST

echo "== pkgbuild (component)"
pkgbuild --root "$work/component-root" \
  --component-plist "$component_plist" \
  --identifier "$PKG_ID" \
  --version "$version" \
  --install-location "/" \
  --scripts "$work/scripts" \
  "$component_pkg"

unsigned="$work/${SLUG}-unsigned.pkg"
echo "== productbuild (Installer.app UI)"
productbuild --distribution "$work/Distribution.xml" \
  --package-path "$work" \
  --resources "$work/resources" \
  "$unsigned"

installer_identity="$(mac_installer_codesign_identity)"
echo "== productsign (Developer ID Installer)"
productsign --sign "$installer_identity" --timestamp "$unsigned" "$signed"

echo "== notarytool submit pkg"
xcrun notarytool submit "$signed" \
  --key "$AA_NOTARY_KEY" \
  --key-id "$AA_NOTARY_KEY_ID" \
  --issuer "$AA_NOTARY_ISSUER" \
  --wait

echo "== staple pkg"
xcrun stapler staple "$signed"

echo "== spctl verify pkg"
spctl -a -vv -t install "$signed"

shasum -a 256 "$signed"
echo ""
echo "Mac installer: $signed"
echo "Users: double-click the .pkg → Continue → accept license → Install (Change Install Location to pick folder)."
