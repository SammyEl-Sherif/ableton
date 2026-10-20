#!/usr/bin/env python3
"""Validate a plugin release (.pkg on mac or .zip on mac/win) before upload.

    python3 scripts/validate_plugin_release.py --plugin duck --platform mac \\
        --artifact dist/releases/duck/1.0.0/Duck-1.0.0-mac-installer.pkg --require-notarized
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import platform
import shutil
import subprocess
import sys
import tempfile
import zipfile
from dataclasses import asdict, dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

from lib.plugin_release import PluginReleaseInfo, get_plugin  # noqa: E402


@dataclass
class ReleaseManifest:
    plugin_slug: str
    version: str
    platform: str
    file_name: str
    storage_path: str
    sha256: str
    byte_size: int


def load_install_plugins():
    spec = importlib.util.spec_from_file_location("install_plugins", ROOT / "scripts" / "install_plugins.py")
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    sys.modules["install_plugins"] = module
    spec.loader.exec_module(module)
    return module


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def zip_top_level_names(zf: zipfile.ZipFile) -> set[str]:
    names: set[str] = set()
    for info in zf.infolist():
        part = info.filename.split("/", 1)[0]
        if part and not part.startswith("."):
            names.add(part.rstrip("/"))
    return names


def run(cmd: list[str], *, check: bool = True) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, text=True, capture_output=True, check=check)


def eula_zip_name(info: PluginReleaseInfo) -> str:
    if info.slug == "duck":
        return "Animal-Audio-Duck-EULA.txt"
    return "Animal-Audio-EULA.txt"


def zip_required_entries(info: PluginReleaseInfo, platform_name: str) -> tuple[str, ...]:
    eula = eula_zip_name(info)
    base = (info.bundle_name, eula, "Third-Party-Notices.txt")
    if platform_name == "win":
        return base + (f"Install-{info.product_name}.ps1",)
    return base


def validate_mac_installer_pkg(pkg_path: Path, *, require_notarized: bool) -> None:
    if platform.system() != "Darwin":
        raise SystemExit("macOS .pkg validation requires Darwin.")
    spctl = run(["spctl", "-a", "-vv", "-t", "install", str(pkg_path)], check=False)
    output = (spctl.stdout or "") + (spctl.stderr or "")
    if spctl.returncode != 0:
        raise SystemExit(f"spctl rejected installer:\n{output}")
    if require_notarized and "Notarized Developer ID" not in output:
        raise SystemExit(f"expected notarized installer; spctl said:\n{output}")
    sig = run(["pkgutil", "--check-signature", str(pkg_path)], check=False)
    if sig.returncode != 0:
        raise SystemExit(f"pkg signature check failed:\n{sig.stderr or sig.stdout}")


def extract_bundle_from_pkg(pkg_path: Path, tmp: Path, bundle_name: str) -> Path:
    expanded = tmp / "pkg-expanded"
    if expanded.exists():
        shutil.rmtree(expanded)
    run(["pkgutil", "--expand-full", str(pkg_path), str(expanded)])
    matches = [p for p in expanded.rglob(bundle_name) if p.is_dir()]
    if not matches:
        raise SystemExit(f"{bundle_name} not found inside .pkg payload")
    return matches[0]


def validate_mac_codesign(bundle: Path, *, require_notarized: bool) -> None:
    if platform.system() != "Darwin":
        raise SystemExit("macOS codesign/spctl checks require Darwin (run on a Mac).")

    verify = run(["codesign", "--verify", "--deep", "--strict", "--verbose=2", str(bundle)], check=False)
    if verify.returncode != 0:
        raise SystemExit(f"codesign verify failed:\n{verify.stderr or verify.stdout}")

    spctl = run(["spctl", "-a", "-vv", "-t", "install", str(bundle)], check=False)
    output = (spctl.stdout or "") + (spctl.stderr or "")
    if spctl.returncode != 0:
        raise SystemExit(f"spctl rejected bundle:\n{output}")
    if require_notarized and "Notarized Developer ID" not in output:
        raise SystemExit(f"expected Notarized Developer ID; spctl said:\n{output}")


def validate_zip(
    zip_path: Path,
    *,
    info: PluginReleaseInfo,
    platform_name: str,
    version: str,
    require_notarized: bool,
    allow_unsigned: bool,
    expected_sha: str | None,
    expected_bytes: int | None,
) -> ReleaseManifest:
    if not zip_path.is_file():
        raise SystemExit(f"zip not found: {zip_path}")

    byte_size = zip_path.stat().st_size
    digest = sha256_file(zip_path)
    if expected_bytes is not None and byte_size != expected_bytes:
        raise SystemExit(f"byte size mismatch: got {byte_size}, expected {expected_bytes}")
    if expected_sha is not None and digest.lower() != expected_sha.lower():
        raise SystemExit(f"sha256 mismatch: got {digest}, expected {expected_sha}")

    required = zip_required_entries(info, platform_name)
    file_name = zip_path.name

    with zipfile.ZipFile(zip_path) as zf:
        top = zip_top_level_names(zf)
        missing = [name for name in required if name not in top]
        if missing:
            raise SystemExit(f"zip missing top-level entries {missing}; has {sorted(top)}")
        local_name = f"{info.product_name} Local.vst3"
        if local_name in top:
            raise SystemExit(f"release zip must not contain {local_name} (dev AA_LOCAL_BUILD bundle)")

    with tempfile.TemporaryDirectory(prefix=f"{info.slug}-validate-") as tmp:
        extract_dir = Path(tmp)
        with zipfile.ZipFile(zip_path) as zf:
            zf.extractall(extract_dir)
        bundle = extract_dir / info.bundle_name
        if not bundle.is_dir():
            raise SystemExit(f"{info.bundle_name} bundle missing after extract")

        ip = load_install_plugins()
        stamp = ip.read_stamp(bundle)
        if stamp is None:
            raise SystemExit(f"no AA-BUILD stamp in {info.bundle_name}")

        if platform_name == "mac" and not allow_unsigned:
            validate_mac_codesign(bundle, require_notarized=require_notarized)

    return ReleaseManifest(
        plugin_slug=info.slug,
        version=version,
        platform=platform_name,
        file_name=file_name,
        storage_path=info.storage_path(file_name),
        sha256=digest,
        byte_size=byte_size,
    )


def validate_mac_pkg(
    pkg_path: Path,
    *,
    info: PluginReleaseInfo,
    version: str,
    require_notarized: bool,
    expected_sha: str | None,
    expected_bytes: int | None,
) -> ReleaseManifest:
    if not pkg_path.is_file():
        raise SystemExit(f"pkg not found: {pkg_path}")
    byte_size = pkg_path.stat().st_size
    digest = sha256_file(pkg_path)
    if expected_bytes is not None and byte_size != expected_bytes:
        raise SystemExit(f"byte size mismatch: got {byte_size}, expected {expected_bytes}")
    if expected_sha is not None and digest.lower() != expected_sha.lower():
        raise SystemExit(f"sha256 mismatch: got {digest}, expected {expected_sha}")

    validate_mac_installer_pkg(pkg_path, require_notarized=require_notarized)

    with tempfile.TemporaryDirectory(prefix=f"{info.slug}-pkg-validate-") as tmp_name:
        tmp = Path(tmp_name)
        bundle = extract_bundle_from_pkg(pkg_path, tmp, info.bundle_name)
        ip = load_install_plugins()
        if ip.read_stamp(bundle) is None:
            raise SystemExit(f"no AA-BUILD stamp in {info.bundle_name} inside .pkg")

    file_name = pkg_path.name
    return ReleaseManifest(
        plugin_slug=info.slug,
        version=version,
        platform="mac",
        file_name=file_name,
        storage_path=info.storage_path(file_name),
        sha256=digest,
        byte_size=byte_size,
    )


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--plugin", required=True, help="Plugin slug (folder under plugins/, e.g. duck)")
    p.add_argument("--platform", choices=("mac", "win"), required=True)
    p.add_argument("--artifact", type=Path, help="Release .pkg (mac) or .zip")
    p.add_argument("--zip", type=Path, help=argparse.SUPPRESS)
    p.add_argument(
        "--version",
        help="Release version (default: plugins/<plugin>/VERSION)",
    )
    p.add_argument("--expected-sha", help="Optional sha256 to match")
    p.add_argument("--expected-bytes", type=int, help="Optional byte size to match")
    p.add_argument("--require-notarized", action="store_true", help="Require notarized Developer ID (mac)")
    p.add_argument("--allow-unsigned", action="store_true", help="Skip codesign/spctl (mac CI dev)")
    p.add_argument("--write-manifest", type=Path, help="Write ReleaseManifest JSON for upload")
    return p.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    info = get_plugin(args.plugin)
    version = args.version or info.version
    if version != info.version:
        print(
            f"warning: --version {version} differs from plugins/{info.slug}/VERSION ({info.version})",
            file=sys.stderr,
        )

    artifact = args.artifact or args.zip
    if artifact is None:
        raise SystemExit("pass --artifact path to the .pkg or .zip")
    artifact = artifact.expanduser().resolve()

    if args.platform == "mac" and artifact.suffix == ".pkg":
        manifest = validate_mac_pkg(
            artifact,
            info=info,
            version=version,
            require_notarized=args.require_notarized,
            expected_sha=args.expected_sha,
            expected_bytes=args.expected_bytes,
        )
    else:
        manifest = validate_zip(
            artifact,
            info=info,
            platform_name=args.platform,
            version=version,
            require_notarized=args.require_notarized,
            allow_unsigned=args.allow_unsigned,
            expected_sha=args.expected_sha,
            expected_bytes=args.expected_bytes,
        )

    print(f"OK  {manifest.file_name}")
    print(f"    sha256:    {manifest.sha256}")
    print(f"    bytes:     {manifest.byte_size}")
    print(f"    storage:   {manifest.storage_path}")

    if args.write_manifest:
        args.write_manifest.parent.mkdir(parents=True, exist_ok=True)
        args.write_manifest.write_text(json.dumps(asdict(manifest), indent=2) + "\n", encoding="utf-8")
        print(f"    manifest:  {args.write_manifest}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
