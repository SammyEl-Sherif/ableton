#!/usr/bin/env python3
"""Validate a Duck macOS .pkg installer or Windows zip before upload.

    python3 scripts/validate_duck_release.py --platform mac --artifact dist/.../Duck-1.0.0-mac-installer.pkg --require-notarized
    python3 scripts/validate_duck_release.py --platform win --artifact dist/.../Duck-1.0.0-win-vst3.zip --allow-unsigned

Exit 0 on success; optional --write-manifest for upload_duck_release.py.
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

MAC_ZIP_REQUIRED = (
    "Duck.vst3",
    "Animal-Audio-Duck-EULA.txt",
    "Third-Party-Notices.txt",
)
WIN_REQUIRED = (
    "Duck.vst3",
    "Install-Duck.ps1",
    "Animal-Audio-Duck-EULA.txt",
    "Third-Party-Notices.txt",
)


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


def extract_duck_from_pkg(pkg_path: Path, tmp: Path) -> Path:
    expanded = tmp / "pkg-expanded"
    if expanded.exists():
        shutil.rmtree(expanded)
    run(["pkgutil", "--expand-full", str(pkg_path), str(expanded)])
    matches = [p for p in expanded.rglob("Duck.vst3") if p.is_dir()]
    if not matches:
        raise SystemExit("Duck.vst3 not found inside .pkg payload")
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

    required = MAC_ZIP_REQUIRED if platform_name == "mac" else WIN_REQUIRED
    file_name = zip_path.name
    storage_path = f"duck/{version}/{file_name}"

    with zipfile.ZipFile(zip_path) as zf:
        top = zip_top_level_names(zf)
        missing = [name for name in required if name not in top]
        if missing:
            raise SystemExit(f"zip missing top-level entries {missing}; has {sorted(top)}")
        if "Duck Local.vst3" in top:
            raise SystemExit("release zip must not contain Duck Local.vst3 (dev AA_LOCAL_BUILD bundle)")

    with tempfile.TemporaryDirectory(prefix="duck-validate-") as tmp:
        extract_dir = Path(tmp)
        with zipfile.ZipFile(zip_path) as zf:
            zf.extractall(extract_dir)
        bundle = extract_dir / "Duck.vst3"
        if not bundle.is_dir():
            raise SystemExit("Duck.vst3 bundle missing after extract")

        ip = load_install_plugins()
        stamp = ip.read_stamp(bundle)
        if stamp is None:
            raise SystemExit("no AA-BUILD stamp in Duck.vst3 (too old or wrong binary)")

        if platform_name == "mac" and not allow_unsigned:
            validate_mac_codesign(bundle, require_notarized=require_notarized)

    return ReleaseManifest(
        plugin_slug="duck",
        version=version,
        platform=platform_name,
        file_name=file_name,
        storage_path=storage_path,
        sha256=digest,
        byte_size=byte_size,
    )


def validate_mac_pkg(
    pkg_path: Path,
    *,
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

    with tempfile.TemporaryDirectory(prefix="duck-pkg-validate-") as tmp_name:
        tmp = Path(tmp_name)
        bundle = extract_duck_from_pkg(pkg_path, tmp)
        ip = load_install_plugins()
        if ip.read_stamp(bundle) is None:
            raise SystemExit("no AA-BUILD stamp in Duck.vst3 inside .pkg")

    file_name = pkg_path.name
    return ReleaseManifest(
        plugin_slug="duck",
        version=version,
        platform="mac",
        file_name=file_name,
        storage_path=f"duck/{version}/{file_name}",
        sha256=digest,
        byte_size=byte_size,
    )


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--platform", choices=("mac", "win"), required=True)
    p.add_argument("--artifact", type=Path, help="Release .pkg (mac) or .zip (win / optional mac zip)")
    p.add_argument("--zip", type=Path, help=argparse.SUPPRESS)
    p.add_argument("--version", default="1.0.0", help="Release version segment (default 1.0.0)")
    p.add_argument("--expected-sha", help="Optional sha256 to match")
    p.add_argument("--expected-bytes", type=int, help="Optional byte size to match")
    p.add_argument(
        "--require-notarized",
        action="store_true",
        default=False,
        help="Require spctl to report Notarized Developer ID (mac)",
    )
    p.add_argument(
        "--allow-unsigned",
        action="store_true",
        help="Skip codesign/spctl (mac); for CI artifacts only",
    )
    p.add_argument("--write-manifest", type=Path, help="Write ReleaseManifest JSON for upload step")
    return p.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    artifact = (args.artifact or args.zip)
    if artifact is None:
        raise SystemExit("pass --artifact path to the .pkg or .zip")
    artifact = artifact.expanduser().resolve()

    if args.platform == "mac" and artifact.suffix == ".pkg":
        manifest = validate_mac_pkg(
            artifact,
            version=args.version,
            require_notarized=args.require_notarized,
            expected_sha=args.expected_sha,
            expected_bytes=args.expected_bytes,
        )
    else:
        manifest = validate_zip(
            artifact,
            platform_name=args.platform,
            version=args.version,
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
