# Apple Developer setup (Mac DUCK releases)

Required before uploading any Mac build to animal.audio. Enrollment is manual; this repo cannot do it for you.

## 1. Enroll

1. Join the [Apple Developer Program](https://developer.apple.com/programs/) ($99/year).
2. Wait for approval (often 24–48 hours).

## 2. Signing certificate

1. Open [Certificates, Identifiers & Profiles](https://developer.apple.com/account/resources/certificates/list).
2. Create **Developer ID Application** (distribution outside the Mac App Store).
3. Download and install the certificate in Keychain Access.
4. Note the exact identity string, e.g. `Developer ID Application: Animal Audio (TEAMID)`.

## 3. Notarization API key

1. [App Store Connect](https://appstoreconnect.apple.com/) → **Users and Access** → **Integrations** → **App Store Connect API**.
2. Create a key with at least **Developer** access.
3. Save **Issuer ID**, **Key ID**, and the `.p8` file (never commit).

Suggested local layout:

```text
~/.appstoreconnect/AuthKey_XXXXXXXXXX.p8
```

## 4. Environment for `scripts/release_mac_duck.sh`

Export before running the release script:

```bash
export AA_CODESIGN_IDENTITY="Developer ID Application: Animal Audio (TEAMID)"
export AA_NOTARY_KEY="~/.appstoreconnect/AuthKey_XXXXXXXXXX.p8"
export AA_NOTARY_KEY_ID="XXXXXXXXXX"
export AA_NOTARY_ISSUER="xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
```

Optional: `AA_DUCK_VERSION=1.0.0` (default `1.0.0`).

## 5. Verify membership

```bash
security find-identity -v -p codesigning | grep "Developer ID Application"
```

When enrollment is complete, run `scripts/release_mac_duck.sh` from the repo root.
