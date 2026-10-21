# Plugin release pipeline

## Version (independent per plugin)

| Source | Role |
| --- | --- |
| `plugins/<slug>/VERSION` | **Semver for that product only** (e.g. Duck `1.0.0`) |
| Plugin UI build stamp | Same version as that plugin’s VERSION file |
| Artifacts | `dist/releases/<slug>/<version>/…` |
| GitHub Releases | Tag `<slug>-vX.Y.Z` (e.g. `duck-v1.0.0`) |
| animal.audio | `plugin_releases` row per slug + platform (upload updates checksums) |

```bash
python3 scripts/read_plugin_version.py duck
python3 scripts/read_plugin_version.py duck --tag
```

**Ship a patch for one plugin:** edit only `plugins/duck/VERSION` (or the relevant slug), commit, add/update Supabase rows in **animal-audio-web** if needed, run that plugin’s release — no monorepo-wide bump.

Root `CMakeLists.txt` `project(AnimalAudio VERSION 0.0.0)` is the monorepo marker only, not a product version.

---

## Commands

| Goal | Command |
| --- | --- |
| Mac (notarized .pkg) | `source ~/.appstoreconnect/notary.env && ./scripts/release_plugin.sh <slug> --upload` |
| Windows | `.\scripts\release_win_plugin.ps1 -Plugin <slug> -Upload` |
| GitHub Actions | **Release plugin** — type the slug, choose platform |

Every plugin goes through the same pipeline; nothing in the scripts or workflow is per plugin. Wrappers
`./scripts/release_duck.sh` and `release_win_duck.ps1` still work.

---

## What each platform ships

| Platform | Artifact | Notes |
| --- | --- | --- |
| Mac | `<Product>-<ver>-mac-installer.pkg` | Bundle signed + notarized + stapled, then the `.pkg` is productsigned (Developer ID Installer) + notarized + stapled. Package id `com.animalaudio.<slug>.vst3`. |
| Windows | `<Product>-<ver>-win-vst3.zip` | Bundle, EULA, notices, and `Install-<Product>.ps1` rendered from `scripts/installers/win/Install-Plugin.ps1`. |

Installer text comes from templates in `scripts/installers/mac/` and `scripts/installers/win/`, filled with the plugin's
`PRODUCT_NAME` (from `plugins/<slug>/CMakeLists.txt`) split into words (`HoneyBadger` → "Honey Badger"). Preview a
plugin's Mac installer resources without signing:
`scripts/build_mac_pkg.sh <slug> <path/to/Product.vst3> <version> /tmp/pkg-preview --render-only`.

---

## Releasing a new plugin (first time)

1. The plugin is merged to `main` with `plugins/<slug>/VERSION`, green CI and pluginval (`scripts/verify_plugin.sh <Name>`).
2. In **animal-audio-web**, add an active `plugin_releases` row for `<slug>` per platform (`mac`, `win`); the upload
   step only updates the checksum/size of an existing active row.
3. GitHub Actions → **Release plugin** → slug `<slug>`, platform `mac`, then again with `win`. Each run builds, signs,
   notarizes (mac), validates, uploads to Supabase and publishes the `<slug>-vX.Y.Z` GitHub Release.

Signing identities and the notary key live only in repo secrets ([docs/GITHUB_ACTIONS_SECRETS.md](GITHUB_ACTIONS_SECRETS.md));
contributors never need Apple credentials.

---

## Scripts

| Script | Role |
| --- | --- |
| [`scripts/lib/plugin_release.py`](../scripts/lib/plugin_release.py) | Catalog: slug, target, product name, paths |
| [`scripts/release_plugin.sh`](../scripts/release_plugin.sh) | Mac orchestrator |
| [`scripts/release_mac_plugin.sh`](../scripts/release_mac_plugin.sh) | Build, sign, notarize, staple the bundle |
| [`scripts/build_mac_pkg.sh`](../scripts/build_mac_pkg.sh) | Signed + notarized `.pkg` from the stapled bundle |
| [`scripts/release_win_plugin.ps1`](../scripts/release_win_plugin.ps1) | Windows orchestrator |
| [`scripts/validate_plugin_release.py`](../scripts/validate_plugin_release.py) | Validate `.pkg` / `.zip` |
| [`scripts/upload_plugin_release.py`](../scripts/upload_plugin_release.py) | Supabase upload |

Secrets: [docs/GITHUB_ACTIONS_SECRETS.md](GITHUB_ACTIONS_SECRETS.md).

---

## Validate only

```bash
python3 scripts/validate_plugin_release.py --plugin duck --platform mac \
  --artifact dist/releases/duck/1.0.0/Duck-1.0.0-mac-installer.pkg \
  --require-notarized

python3 scripts/validate_plugin_release.py --plugin duck --platform win \
  --artifact dist/releases/duck/1.0.0/Duck-1.0.0-win-vst3.zip --allow-unsigned
```
