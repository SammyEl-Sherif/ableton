#!/usr/bin/env bash
# Build a signed, notarized macOS installer (.pkg) — standard Installer.app flow:
# Welcome → License → choose install location → admin password → install → conclusion + alert.
#
# Usage (stapled Duck.vst3):
#   ./scripts/build_mac_duck_pkg.sh /path/to/Duck.vst3 1.0.0 /path/to/dist
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
# shellcheck source=lib/mac_signing_identities.sh
source "$repo_root/scripts/lib/mac_signing_identities.sh"
bundle="${1:?stapled Duck.vst3 path}"
version="${2:?version e.g. 1.0.0}"
dist_dir="${3:?output directory}"

for var in AA_CODESIGN_IDENTITY AA_NOTARY_KEY AA_NOTARY_KEY_ID AA_NOTARY_ISSUER; do
  if [ -z "${!var:-}" ]; then
    echo "Missing env: $var" >&2
    exit 1
  fi
done

if [ ! -d "$bundle" ]; then
  echo "Bundle not found: $bundle" >&2
  exit 1
fi

mkdir -p "$dist_dir"
pkg_name="Duck-${version}-mac-installer.pkg"
signed="${dist_dir}/${pkg_name}"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

resources="$work/resources"
scripts_dir="$work/scripts"
component_root="$work/component-root/Library/Audio/Plug-Ins/VST3"
mkdir -p "$resources" "$scripts_dir" "$component_root"

ditto "$bundle" "$component_root/Duck.vst3"
cp "$repo_root/scripts/installers/mac/postinstall" "$scripts_dir/postinstall"
chmod +x "$scripts_dir/postinstall"
cp "$repo_root/docs/EULA.md" "$resources/EULA.txt"
cp "$repo_root/scripts/installers/mac/welcome.html" "$resources/welcome.html"
cp "$repo_root/scripts/installers/mac/conclusion.html" "$resources/conclusion.html"

sed "s/__VERSION__/${version}/g" "$repo_root/scripts/installers/mac/Distribution.xml" > "$work/Distribution.xml"

component_pkg="$work/duck-component.pkg"
component_plist="$work/components.plist"
# Never relocatable: Installer must write to the VST3 folder hosts scan, not "upgrade" a Duck.vst3
# with the same bundle ID found elsewhere on disk (downloads, build dirs).
cat > "$component_plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<array>
  <dict>
    <key>RootRelativeBundlePath</key>
    <string>Library/Audio/Plug-Ins/VST3/Duck.vst3</string>
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
  --identifier "com.animalaudio.duck.vst3" \
  --version "$version" \
  --install-location "/" \
  --scripts "$scripts_dir" \
  "$component_pkg"

unsigned="$work/Duck-unsigned.pkg"
echo "== productbuild (Installer.app UI)"
productbuild --distribution "$work/Distribution.xml" \
  --package-path "$work" \
  --resources "$resources" \
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
