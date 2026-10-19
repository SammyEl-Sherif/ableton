# Apple Developer setup (Mac DUCK releases)

Required before uploading any Mac build to animal.audio. Enrollment is manual; this repo cannot do it for you.

## 1. Enroll

1. Join the [Apple Developer Program](https://developer.apple.com/programs/) ($99/year).
2. Wait for approval (often 24–48 hours).

## 2. Signing certificates (you need both)

1. Open [Certificates, Identifiers & Profiles](https://developer.apple.com/account/resources/certificates/list).
2. Create **Developer ID Application** — signs `Duck.vst3` (and other code).
3. Create **Developer ID Installer** — signs the **`.pkg`** installer (`productsign` will fail without this).
4. Download each `.cer`, double-click to install in Keychain Access (same Mac that created the CSRs).
5. Note the exact identity strings, e.g.:
   - `Developer ID Application: Your Company (TEAMID)`
   - `Developer ID Installer: Your Company (TEAMID)`

## 3. Notarization API key

1. [App Store Connect](https://appstoreconnect.apple.com/) → **Users and Access** → **Integrations** → **App Store Connect API**.
2. Create a key with at least **Developer** access.
3. Save **Issuer ID**, **Key ID**, and the `.p8` file (never commit).

Suggested local layout:

```text
~/.appstoreconnect/AuthKey_XXXXXXXXXX.p8
```

## 4. Environment for `scripts/release_mac_duck.sh`

Store secrets outside the repo (e.g. `~/.appstoreconnect/notary.env`, mode `600`):

```bash
export AA_CODESIGN_IDENTITY="Developer ID Application: Your Company (TEAMID)"
export AA_INSTALLER_CODESIGN_IDENTITY="Developer ID Installer: Your Company (TEAMID)"
export AA_NOTARY_KEY="$HOME/.appstoreconnect/AuthKey_XXXXXXXXXX.p8"
export AA_NOTARY_KEY_ID="XXXXXXXXXX"
export AA_NOTARY_ISSUER="xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
```

Then:

```bash
source ~/.appstoreconnect/notary.env
./scripts/release_mac_duck.sh
```

Complete [LICENSING.md](LICENSING.md) (JUCE commercial, EULA) before uploading binaries.

Optional: `AA_DUCK_VERSION=1.0.0` (default `1.0.0`).

## 5. Verify membership

```bash
security find-identity -v -p codesigning | grep "Developer ID Application"
security find-identity -v | grep "Developer ID Installer"
```

Installer certificates usually **do not** appear under `-p codesigning`; that is normal. Do not put the Installer name in `AA_CODESIGN_IDENTITY`.

When enrollment is complete, run `scripts/release_mac_duck.sh` from the repo root.
