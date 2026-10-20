# Duck release pipeline

## Version (semver)

**Single source of truth:** `project(AnimalAudio VERSION x.y.z …)` in the root [`CMakeLists.txt`](../CMakeLists.txt).

- Plugin **build stamp**, release **filenames**, and **GitHub tag** `vX.Y.Z` all use this version.
- Read it locally: `python3 scripts/read_project_version.py` (add `--tag` for `v1.0.0`).
- **Before each release:** bump `VERSION` in CMake, commit, then run the release pipeline (no manual version field in Actions).
- **GitHub:** successful release workflows create or update a [GitHub Release](https://github.com/SammyEl-Sherif/ableton/releases) tagged `vX.Y.Z` with platform artifacts (toggle **publish_github_release** if you only want Supabase). Mac and Windows runs attach to the **same** tag when the version matches.
- **animal.audio:** still requires a matching row in **animal-audio-web** `plugin_releases` + Storage path for new versions (upload script updates checksums for the active row only).

Override for local experiments: `AA_DUCK_VERSION=1.0.1` or `./scripts/release_duck.sh --version 1.0.1` (avoid for production).

## Status

| Platform | animal.audio | How to ship |
| --- | --- | --- |
| **Mac** | **Live** — `Duck-1.0.0-mac-installer.pkg` | `./scripts/release_duck.sh --upload` or Actions → **Release Duck (macOS)** |
| **Windows** | **Live** — `Duck-1.0.0-win-vst3.zip` | Actions → **Release Duck (Windows)** (build); upload needs repo secrets below |

---

## macOS

```bash
source ~/.appstoreconnect/notary.env
./scripts/release_duck.sh --upload
```

Requires **two** certs in `notary.env`: `AA_CODESIGN_IDENTITY` = **Developer ID Application**, `AA_INSTALLER_CODESIGN_IDENTITY` = **Developer ID Installer**.

Primary artifact: `dist/releases/<version>/Duck-<version>-mac-installer.pkg` (Installer.app: welcome, license, change install location, password, Ableton rescan message).

## Windows

On a Windows machine (PowerShell, repo root):

```powershell
.\scripts\release_win_duck.ps1 -Upload
```

Zip includes `Duck.vst3`, `Install-Duck.ps1`, EULA, and third-party notices. Users run **Install-Duck.ps1** (admin) or copy the bundle manually.

### GitHub Actions (recommended)

1. Set repository secrets (names are already created with placeholder values — replace each one): see [docs/GITHUB_ACTIONS_SECRETS.md](GITHUB_ACTIONS_SECRETS.md).

2. Bump `VERSION` in `CMakeLists.txt`, push, then Actions → **Release Duck (Windows)** → upload **on** (version is read from CMake).

3. Confirm on Supabase: Storage `plugin-releases/duck/1.0.0/Duck-1.0.0-win-vst3.zip` and `plugin_releases.win` has `sha256` + `byte_size`.

Mac release workflow: **Release Duck (macOS)** (same Supabase secrets for upload).

---

## Scripts

| Script | Role |
| --- | --- |
| [`scripts/release_duck.sh`](../scripts/release_duck.sh) | Mac orchestrator |
| [`scripts/release_win_duck.ps1`](../scripts/release_win_duck.ps1) | Windows orchestrator |
| [`scripts/validate_duck_release.py`](../scripts/validate_duck_release.py) | Validate `.pkg` or `.zip` |
| [`scripts/upload_duck_release.py`](../scripts/upload_duck_release.py) | Supabase Storage + `plugin_releases` |

Local upload (after validate + `manifest-*.json`):

```bash
export AA_WEB_REPO=/path/to/animal-audio-web
python3 scripts/upload_duck_release.py --manifest dist/releases/1.0.0/manifest-win.json \
  --zip dist/releases/1.0.0/Duck-1.0.0-win-vst3.zip
```

CI upload uses the same script with `AA_SUPABASE_ACCESS_TOKEN` (see `ensure_supabase_linked` in upload script).

## Validate only

```bash
python3 scripts/validate_duck_release.py --platform mac \
  --artifact dist/releases/1.0.0/Duck-1.0.0-mac-installer.pkg \
  --version 1.0.0 --require-notarized

python3 scripts/validate_duck_release.py --platform win \
  --artifact dist/releases/1.0.0/Duck-1.0.0-win-vst3.zip \
  --version 1.0.0 --allow-unsigned
```
