"""Per-plugin release metadata (semver, paths, Supabase slug).

Source of truth for product version: plugins/<slug>/VERSION (semver x.y.z).
CMake target and VST3 bundle name come from plugins/<slug>/CMakeLists.txt.
"""
from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
PLUGINS_DIR = ROOT / "plugins"
_SEMVER = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+$")
_CMAKE_TARGET = re.compile(r"aa_add_plugin\s*\(\s*(\w+)", re.MULTILINE)
_PRODUCT_NAME = re.compile(r'PRODUCT_NAME\s+"([^"]+)"')
# Plugins with a signed .pkg installer (macOS); others ship zip-only on Mac when added later.
MAC_PKG_SLUGS = frozenset({"duck"})


@dataclass(frozen=True)
class PluginReleaseInfo:
    slug: str
    cmake_target: str
    product_name: str
    version: str

    @property
    def bundle_name(self) -> str:
        return f"{self.product_name}.vst3"

    @property
    def supports_mac_pkg(self) -> bool:
        return self.slug in MAC_PKG_SLUGS

    @property
    def github_tag(self) -> str:
        return f"{self.slug}-v{self.version}"

    def release_root(self, repo_root: Path | None = None) -> Path:
        base = repo_root or ROOT
        return base / "dist" / "releases" / self.slug / self.version

    def mac_pkg_name(self) -> str:
        return f"{self.product_name}-{self.version}-mac-installer.pkg"

    def mac_zip_name(self) -> str:
        return f"{self.product_name}-{self.version}-mac-vst3.zip"

    def win_zip_name(self) -> str:
        return f"{self.product_name}-{self.version}-win-vst3.zip"

    def storage_path(self, file_name: str, version: str | None = None) -> str:
        return f"{self.slug}/{version or self.version}/{file_name}"

    def bundle_build_rel(self, config: str = "Release") -> str:
        return f"plugins/{self.slug}/{self.cmake_target}_artefacts/{config}/VST3/{self.bundle_name}"


def _read_version(version_file: Path) -> str:
    if not version_file.is_file():
        raise ValueError(f"missing VERSION file: {version_file}")
    version = version_file.read_text(encoding="utf-8").strip()
    if not _SEMVER.match(version):
        raise ValueError(f"invalid semver in {version_file}: {version!r} (expected x.y.z)")
    return version


def _parse_cmake(cmake_path: Path) -> tuple[str, str]:
    text = cmake_path.read_text(encoding="utf-8")
    target_match = _CMAKE_TARGET.search(text)
    product_match = _PRODUCT_NAME.search(text)
    if not target_match or not product_match:
        raise ValueError(f"could not parse aa_add_plugin / PRODUCT_NAME in {cmake_path}")
    return target_match.group(1), product_match.group(1)


def list_plugin_slugs() -> list[str]:
    slugs: list[str] = []
    if not PLUGINS_DIR.is_dir():
        return slugs
    for path in sorted(PLUGINS_DIR.iterdir()):
        if path.is_dir() and (path / "CMakeLists.txt").is_file():
            slugs.append(path.name)
    return slugs


def get_plugin(slug: str, *, repo_root: Path | None = None) -> PluginReleaseInfo:
    base = repo_root or ROOT
    plugin_dir = base / "plugins" / slug
    if not plugin_dir.is_dir():
        raise ValueError(f"unknown plugin slug: {slug}")
    cmake = plugin_dir / "CMakeLists.txt"
    version = _read_version(plugin_dir / "VERSION")
    target, product = _parse_cmake(cmake)
    return PluginReleaseInfo(slug=slug, cmake_target=target, product_name=product, version=version)


def read_plugin_version(slug: str, *, repo_root: Path | None = None) -> str:
    return get_plugin(slug, repo_root=repo_root).version
