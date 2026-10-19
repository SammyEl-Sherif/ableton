# shellcheck shell=bash
# Resolve Developer ID identities for VST3 (Application) vs .pkg (Installer).
# Source from release scripts; do not execute directly.

mac_assert_application_identity() {
  local id="${1:?identity}"
  if [[ "$id" != *"Developer ID Application:"* ]]; then
    echo "AA_CODESIGN_IDENTITY must be a Developer ID Application certificate (signs Duck.vst3)." >&2
    echo "Got: $id" >&2
    echo "Use Developer ID Installer only in AA_INSTALLER_CODESIGN_IDENTITY (for the .pkg)." >&2
    return 1
  fi
}

mac_installer_codesign_identity() {
  if [ -n "${AA_INSTALLER_CODESIGN_IDENTITY:-}" ]; then
    if [[ "$AA_INSTALLER_CODESIGN_IDENTITY" != *"Developer ID Installer:"* ]]; then
      echo "AA_INSTALLER_CODESIGN_IDENTITY must be a Developer ID Installer certificate." >&2
      return 1
    fi
    echo "$AA_INSTALLER_CODESIGN_IDENTITY"
    return 0
  fi
  local line
  # Installer identities often do not appear with -p codesigning; search the full list.
  line="$(security find-identity -v 2>/dev/null | grep 'Developer ID Installer:' | head -n 1 || true)"
  if [ -z "$line" ]; then
    echo "Missing Developer ID Installer certificate." >&2
    echo "Keychain may show it, but security needs the cert + private key pair on this Mac." >&2
    echo "List with: security find-identity -v | grep \"Developer ID Installer\"" >&2
    echo 'Then set AA_INSTALLER_CODESIGN_IDENTITY="Developer ID Installer: … (TEAMID)"' >&2
    return 1
  fi
  echo "$line" | sed -E 's/^[[:space:]]*[0-9]+\) +"([^"]+)".*/\1/'
}
