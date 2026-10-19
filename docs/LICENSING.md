# Licensing before you ship Duck

Animal Audio plugins are **proprietary** (see [LICENSE](../LICENSE) and [EULA](EULA.md)). Build-time dependencies have their own terms; **you must satisfy them before distributing binaries**, even for a free download.

## Checklist

| Step | Status | Notes |
| --- | --- | --- |
| **JUCE licence (Starter / Indie / Pro)** | You | **Starter** (free, perpetual) is enough to ship **closed-source** binaries if the **licensee** (e.g. Workpace Technology LLC) had **≤ $20k USD** total revenue *and* funding in the last 12 months from all sources — not just Duck. Above that, use [Indie or Pro](https://juce.com/get-juce/). Accept the tier on your [JUCE account](https://account.juce.com/) (download/clone alone is not the contract). EULA: [JUCE 9 licence](https://juce.com/legal/juce-9-licence/) (also covers older majors this repo may build against). |
| **VST3 SDK** | Via JUCE | VST3 support comes through JUCE. Follow [Steinberg’s VST SDK license](https://www.steinberg.net/en/company/developers.html) for proprietary products. |
| **Animal Audio EULA** | Repo | [docs/EULA.md](EULA.md) — ship a copy with release zips (see `release_mac_duck.sh`). Link from animal.audio download / legal pages. |
| **Third-party notices** | Repo | [docs/THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) — include in release zips. |
| **Source repo** | Repo | [LICENSE](../LICENSE) — all rights reserved; no public source license. |

Catch2 is used **only in tests** and is not linked into shipped VST3 binaries.

## What we do not commit

- JUCE license certificate or account credentials
- Apple notary `.p8` keys (see [APPLE_DEVELOPER_SETUP.md](APPLE_DEVELOPER_SETUP.md))
- Customer emails or Supabase secrets

## Build with a local JUCE checkout (optional)

This repo normally fetches JUCE at configure time. To use `~/Projects/JUCE` instead:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DFETCHCONTENT_SOURCE_DIR_JUCE="$HOME/Projects/JUCE"
```

No separate CMake “licence flag” is required. Re-check your JUCE tier if company revenue or funding crosses a limit.
