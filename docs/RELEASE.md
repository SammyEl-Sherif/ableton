# Plugin release pipeline

## Version (independent per plugin)

| Source | Role |
| --- | --- |
| `plugins/<slug>/VERSION` | **Semver for that product only** (e.g. Duck `1.0.0`, Ice Penguin `0.1.0`) |
| Plugin UI build stamp | Same version as that plugin’s VERSION file |
| Artifacts | `dist/releases/<slug>/<version>/…` |
| GitHub Releases | Tag `<slug>-vX.Y.Z` (e.g. `duck-v1.0.0`) |
| animal.audio | `plugin_releases` row per slug + platform (upload updates checksums) |

```bash
python3 scripts/read_plugin_version.py duck
python3 scripts/read_plugin_version.py icepenguin --tag
```

**Ship a patch for one plugin:** edit only `plugins/duck/VERSION` (or the relevant slug), commit, add/update Supabase rows in **animal-audio-web** if needed, run that plugin’s release — no monorepo-wide bump.

Root `CMakeLists.txt` `project(AnimalAudio VERSION 0.0.0)` is the monorepo marker only, not a product version.

---

## Commands

| Goal | Command |
| --- | --- |
| Mac (Duck .pkg) | `source ~/.appstoreconnect/notary.env && ./scripts/release_plugin.sh duck --upload` |
| Windows | `.\scripts\release_win_plugin.ps1 -Plugin duck -Upload` |
| GitHub Actions | **Release plugin** — choose slug + platform |

Wrappers `./scripts/release_duck.sh` and `release_win_duck.ps1` still work.

---

## Status (example)

| Plugin | Version file | Mac | Windows |
| --- | --- | --- | --- |
| **duck** | `plugins/duck/VERSION` | `.pkg` + Actions (mac) | zip + Actions (win) |
| **icepenguin** | `plugins/icepenguin/VERSION` | zip path TBD (no .pkg yet) | zip via `release_win_plugin.ps1` |

---

## Scripts

| Script | Role |
| --- | --- |
| [`scripts/lib/plugin_release.py`](../scripts/lib/plugin_release.py) | Catalog: slug, target, product name, paths |
| [`scripts/release_plugin.sh`](../scripts/release_plugin.sh) | Mac orchestrator |
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
