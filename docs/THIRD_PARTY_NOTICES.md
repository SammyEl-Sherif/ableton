# Third-party notices — Duck (Animal Audio)

Duck is built with the following third-party components. This file summarizes obligations for **binary** distributions. Full license texts for open-source components are available from the projects linked below.

## JUCE

- **Project:** [JUCE](https://juce.com/) (fetched at build time, tag pinned in `cmake/FetchDependencies.cmake`)
- **Use in Duck:** Audio plugin framework (GUI, VST3 hosting, parameters, etc.)
- **License:** JUCE **Starter**, **Indie**, or **Pro** licence (or AGPL v3 if you open-source under those terms)
- **Distribution:** Proprietary Duck builds need a JUCE licence tier that matches your revenue/funding (often **Starter** under the $20k limit). See [docs/LICENSING.md](LICENSING.md).

## VST3 SDK

- **Project:** Steinberg VST3 SDK (included via JUCE)
- **Use in Duck:** VST3 plugin format
- **License:** Steinberg VST3 SDK license (see Steinberg developer terms)
- **Distribution:** Follow Steinberg’s requirements for proprietary VST3 products.

## Catch2 (tests only)

- **Project:** [Catch2](https://github.com/catchorg/Catch2)
- **Use:** Unit tests in development builds only — **not** linked into released VST3 binaries
- **License:** Boost Software License 1.0

---

Animal Audio proprietary code is © Animal Audio. See [LICENSE](../LICENSE) and [EULA.md](EULA.md).
