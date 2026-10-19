#!/usr/bin/env python3
"""Build and install every Animal Audio plugin (everything under plugins/) as VST3.

Cross-platform (macOS, Windows, Linux), standard library only. This is what the /setup agent skill runs,
and it is meant to be usable by hand too:

    python3 scripts/install_plugins.py --prereqs               # check build tools (add --install-prereqs to install them)
    python3 scripts/install_plugins.py --build --clean         # clean Release build of ALL plugins, then install them
    python3 scripts/install_plugins.py                         # install what is already built in build/
    python3 scripts/install_plugins.py --list                  # every copy on this machine, with its build stamp
    python3 scripts/install_plugins.py --uninstall             # remove every Animal Audio plugin
    python3 scripts/install_plugins.py --dry-run ...           # show what would happen, change nothing

Where plugins go (default): macOS ~/Library/Audio/Plug-Ins/VST3, Linux ~/.vst3, Windows
%COMMONPROGRAMFILES%\\VST3 when run as administrator, else %LOCALAPPDATA%\\Programs\\Common\\VST3.
`--system` / `--user` force one; `--prefix DIR` installs into DIR instead (for testing).
Copies of the same plugin in the other standard folders are stale (a host may load the wrong one), so they are
removed unless `--keep-stale`. Each installed bundle's build stamp (version, git SHA, date: embedded by
cmake/GenerateBuildInfo.cmake) is read from the binary and printed, so you can compare it with what the plugin
UI shows (hover the ANIMAL AUDIO badge, or read the small text at the bottom of the left panel).
"""
from __future__ import annotations

import argparse
import ctypes
import os
import platform
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Optional

ROOT = Path(__file__).resolve().parent.parent
MARKER = re.compile(rb"AA-BUILD\|([^|\x00]+)\|([^|\x00]+)\|([^|\x00]+)\|")
MIN_CMAKE = (3, 22)


# ---------------------------------------------------------------- environment

def detect_os() -> str:
    system = platform.system()
    return {"Darwin": "macos", "Windows": "windows"}.get(system, "linux")


def is_admin() -> bool:
    if detect_os() == "windows":
        try:
            return bool(ctypes.windll.shell32.IsUserAnAdmin())  # type: ignore[attr-defined]
        except Exception:
            return False
    return hasattr(os, "geteuid") and os.geteuid() == 0


def standard_dirs(os_name: str) -> dict[str, list[Path]]:
    """The VST3 folders hosts scan, per scope."""
    home = Path.home()
    if os_name == "macos":
        return {"user": [home / "Library/Audio/Plug-Ins/VST3"], "system": [Path("/Library/Audio/Plug-Ins/VST3")]}
    if os_name == "windows":
        common = Path(os.environ.get("COMMONPROGRAMFILES", r"C:\Program Files\Common Files"))
        local = Path(os.environ.get("LOCALAPPDATA", str(home / "AppData/Local")))
        return {"user": [local / "Programs/Common/VST3"], "system": [common / "VST3"]}
    return {"user": [home / ".vst3"], "system": [Path("/usr/lib/vst3"), Path("/usr/local/lib/vst3")]}


def default_scope(os_name: str) -> str:
    return "system" if os_name == "windows" and is_admin() else "user"


# ---------------------------------------------------------------- repo knowledge

def plugin_names() -> list[str]:
    """Base product names from each plugin's CMakeLists (release .vst3 name, without Local suffix)."""
    names = []
    for cmake in sorted((ROOT / "plugins").glob("*/CMakeLists.txt")):
        match = re.search(r'aa_add_plugin\(\s*\w+\s+PRODUCT_NAME\s+"([^"]+)"', cmake.read_text(encoding="utf-8"))
        names.append(match.group(1) if match else cmake.parent.name.capitalize())
    return names


def installed_product_names() -> list[str]:
    """Names to scan in VST3 folders (release bundle plus optional dev `… Local.vst3`)."""
    expanded: list[str] = []
    for name in plugin_names():
        expanded.append(name)
        expanded.append(f"{name} Local")
    return expanded


def find_bundles(build_dir: Path, only: Optional[set[str]] = None) -> list[Path]:
    """Built .vst3 bundles, one per plugin, preferring Release."""
    found: dict[str, list[Path]] = {}
    for bundle in build_dir.glob("plugins/*/*_artefacts/*/VST3/*.vst3"):
        if bundle.is_dir():
            found.setdefault(bundle.name, []).append(bundle)
    result = []
    for name in sorted(found):
        if only and bundle_product(name).lower() not in only:
            continue
        candidates = sorted(found[name], key=lambda p: (p.parts[-3] != "Release", str(p)))
        result.append(candidates[0])
    return result


def bundle_vst3_stem(bundle_name: str) -> str:
    return bundle_name[: -len(".vst3")] if bundle_name.endswith(".vst3") else bundle_name


def bundle_product(bundle_name: str) -> str:
    stem = bundle_vst3_stem(bundle_name)
    if stem.endswith(" Local"):
        stem = stem[: -len(" Local")]
    return stem.replace(" ", "").lower()


def binary_of(bundle: Path) -> Optional[Path]:
    """The plugin binary inside a VST3 bundle (or the file itself for a legacy single-file plugin)."""
    if bundle.is_file():
        return bundle
    contents = bundle / "Contents"
    if not contents.is_dir():
        return None
    for folder in sorted(contents.iterdir()):
        if folder.is_dir() and (folder.name == "MacOS" or folder.name.startswith(("x86_64", "aarch64", "arm64", "i386"))):
            for item in sorted(folder.iterdir()):
                if item.is_file():
                    return item
    return None


@dataclass
class Stamp:
    version: str
    sha: str
    date: str

    def __str__(self) -> str:
        return f"{self.version} | {self.sha} | {self.date}"


def read_stamp(bundle: Path) -> Optional[Stamp]:
    """Build stamp embedded in the binary, or None for builds that predate it."""
    binary = binary_of(bundle)
    if binary is None:
        return None
    try:
        match = MARKER.search(binary.read_bytes())
    except OSError:
        return None
    if match is None:
        return None
    return Stamp(*(group.decode("utf-8", "replace") for group in match.groups()))


def repo_head() -> Optional[str]:
    try:
        out = subprocess.run(["git", "-C", str(ROOT), "rev-parse", "--short=7", "HEAD"], capture_output=True,
                             text=True, check=True)
        return out.stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return None


# ---------------------------------------------------------------- actions (all side effects go through here)

class Runner:
    def __init__(self, dry_run: bool):
        self.dry_run = dry_run
        self.errors: list[str] = []

    def say(self, text: str = "") -> None:
        print(text, flush=True)

    def step(self, text: str) -> None:
        self.say(("[dry-run] " if self.dry_run else "") + text)

    def run(self, command: list[str], cwd: Optional[Path] = None, check: bool = True) -> int:
        self.step("$ " + " ".join(command))
        if self.dry_run:
            return 0
        result = subprocess.run(command, cwd=cwd)
        if check and result.returncode != 0:
            raise SystemExit(f"command failed ({result.returncode}): {' '.join(command)}")
        return result.returncode

    def remove(self, path: Path) -> bool:
        self.step(f"remove {path}")
        if self.dry_run:
            return True
        try:
            if path.is_dir() and not path.is_symlink():
                shutil.rmtree(path)
            else:
                path.unlink()
            return True
        except OSError as error:
            hint = "re-run as administrator" if detect_os() == "windows" else f"try: sudo rm -rf '{path}'"
            self.say(f"  could not remove {path}: {error} ({hint})")
            self.errors.append(f"could not remove {path}")
            return False

    def copy_bundle(self, source: Path, destination: Path) -> None:
        self.step(f"copy {source} -> {destination}")
        if self.dry_run:
            return
        destination.parent.mkdir(parents=True, exist_ok=True)
        staging = destination.with_name(destination.name + ".aa-new")
        if staging.exists():
            shutil.rmtree(staging, ignore_errors=True)
        shutil.copytree(source, staging, symlinks=True)
        if destination.exists():
            shutil.rmtree(destination) if destination.is_dir() else destination.unlink()
        staging.rename(destination)


# ---------------------------------------------------------------- prerequisites

def cmake_version() -> Optional[tuple[int, ...]]:
    try:
        out = subprocess.run(["cmake", "--version"], capture_output=True, text=True, check=True).stdout
    except (OSError, subprocess.CalledProcessError):
        return None
    match = re.search(r"(\d+)\.(\d+)(?:\.(\d+))?", out)
    return tuple(int(part) for part in match.groups(default="0")) if match else None


def windows_build_tools() -> bool:
    vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / \
        "Microsoft Visual Studio/Installer/vswhere.exe"
    if not vswhere.exists():
        return False
    out = subprocess.run([str(vswhere), "-products", "*", "-requires",
                          "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
                         capture_output=True, text=True)
    return bool(out.stdout.strip())


LINUX_PKGS = ["x11", "xrandr", "xinerama", "xcursor", "freetype2", "fontconfig", "alsa"]
APT_PACKAGES = ["build-essential", "cmake", "git", "ninja-build", "pkg-config", "libasound2-dev", "libfreetype-dev",
                "libfontconfig1-dev", "libx11-dev", "libxrandr-dev", "libxinerama-dev", "libxcursor-dev",
                "libxext-dev", "libgl1-mesa-dev", "libcurl4-openssl-dev"]


def check_prereqs(os_name: str) -> list[tuple[str, bool, str]]:
    """(name, ok, how to fix) for each prerequisite. Ninja is optional."""
    rows: list[tuple[str, bool, str]] = []
    version = cmake_version()
    rows.append((f"cmake >= {MIN_CMAKE[0]}.{MIN_CMAKE[1]}" + (f" (found {'.'.join(map(str, version))})" if version else ""),
                 bool(version and version[:2] >= MIN_CMAKE), "install CMake from https://cmake.org/download/"))
    rows.append(("git", shutil.which("git") is not None, "install git"))
    rows.append(("ninja (optional, faster builds)", shutil.which("ninja") is not None, "install ninja"))
    if os_name == "macos":
        clt = subprocess.run(["xcode-select", "-p"], capture_output=True).returncode == 0
        rows.append(("Xcode Command Line Tools (clang)", clt, "xcode-select --install"))
    elif os_name == "windows":
        rows.append(("Visual Studio 2022 Build Tools (C++ workload)", windows_build_tools(),
                     "winget install Microsoft.VisualStudio.2022.BuildTools"))
    else:
        rows.append(("C++ compiler (c++/g++/clang++)", any(shutil.which(c) for c in ("c++", "g++", "clang++")),
                     "sudo apt-get install build-essential"))
        if shutil.which("pkg-config"):
            missing = [p for p in LINUX_PKGS if subprocess.run(["pkg-config", "--exists", p]).returncode != 0]
            rows.append(("JUCE system libraries" + (f" (missing: {', '.join(missing)})" if missing else ""),
                         not missing, "sudo apt-get install " + " ".join(APT_PACKAGES)))
        else:
            rows.append(("pkg-config (to check JUCE system libraries)", False, "sudo apt-get install pkg-config"))
    return rows


def install_prereqs(os_name: str, runner: Runner, assume_yes: bool) -> None:
    if os_name == "macos":
        if subprocess.run(["xcode-select", "-p"], capture_output=True).returncode != 0:
            runner.say("Installing the Xcode Command Line Tools opens a system dialog; accept it, wait for it to "
                       "finish, then re-run this script.")
            runner.run(["xcode-select", "--install"], check=False)
        if shutil.which("brew"):
            runner.run(["brew", "install", "cmake", "ninja", "git"])
        else:
            runner.say("Homebrew not found: install it from https://brew.sh, or install CMake/Ninja manually.")
    elif os_name == "windows":
        if not shutil.which("winget"):
            runner.say("winget not found: install CMake, Git, Ninja and Visual Studio 2022 Build Tools (C++) manually.")
            return
        runner.run(["winget", "install", "-e", "--id", "Kitware.CMake"], check=False)
        runner.run(["winget", "install", "-e", "--id", "Git.Git"], check=False)
        runner.run(["winget", "install", "-e", "--id", "Ninja-build.Ninja"], check=False)
        runner.run(["winget", "install", "-e", "--id", "Microsoft.VisualStudio.2022.BuildTools", "--override",
                    "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"], check=False)
    else:
        if not shutil.which("apt-get"):
            runner.say("Only apt-based distros are automated. Install: cmake git ninja a C++ compiler pkg-config and "
                       "the X11/ALSA/freetype/fontconfig/GL development packages (see README).")
            return
        prefix = [] if is_admin() else ["sudo"]
        runner.run(prefix + ["apt-get", "update"])
        runner.run(prefix + ["apt-get", "install"] + (["-y"] if assume_yes else []) + APT_PACKAGES)


# ---------------------------------------------------------------- build

def build_all(runner: Runner, build_dir: Path, clean: bool, only: Optional[set[str]], universal: bool,
              os_name: str) -> None:
    if clean and build_dir.exists():
        resolved = build_dir.resolve()
        if ROOT not in resolved.parents:
            raise SystemExit(f"refusing to clean {resolved}: not inside the repository")
        runner.remove(build_dir)

    configure = ["cmake", "-S", str(ROOT), "-B", str(build_dir), "-DCMAKE_BUILD_TYPE=Release", "-DAA_BUILD_TESTS=OFF",
                 "-DAA_LOCAL_BUILD=ON"]
    if shutil.which("ninja") and os_name != "windows":
        configure[1:1] = ["-G", "Ninja"]
    if only:
        configure.append("-DAA_PLUGINS=" + ";".join(sorted(only)))
    if universal and os_name == "macos":
        configure.append("-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64")
    runner.say("== configure (Release, every plugin under plugins/)")
    runner.run(configure)
    runner.say("== build")
    runner.run(["cmake", "--build", str(build_dir), "--config", "Release", "--parallel"])


# ---------------------------------------------------------------- install / uninstall / list

def target_dirs(args, os_name: str) -> tuple[Path, list[Path]]:
    """(folder to install into, other standard folders where copies would be stale)."""
    dirs = standard_dirs(os_name)
    if args.prefix:
        return Path(args.prefix).expanduser(), []
    scope = "system" if args.system else "user" if args.user else default_scope(os_name)
    chosen = dirs[scope][0]
    others = [d for s, lst in dirs.items() for d in lst if d != chosen]
    return chosen, others


def installed_copies(directories: Iterable[Path], names: Iterable[str]) -> list[Path]:
    wanted = {f"{n}.vst3" for n in names}
    found = []
    for directory in directories:
        if directory.is_dir():
            found += [child for child in sorted(directory.iterdir()) if child.name in wanted]
    return found


def prepare_for_host(runner: Runner, bundle: Path, os_name: str) -> None:
    """macOS refuses unsigned or quarantined bundles; a local ad-hoc signature is enough for a locally built plugin."""
    if os_name != "macos":
        return
    runner.run(["xattr", "-dr", "com.apple.quarantine", str(bundle)], check=False)
    runner.run(["codesign", "--force", "--deep", "--sign", "-", str(bundle)], check=False)


def do_install(args, runner: Runner, os_name: str) -> int:
    build_dir = Path(args.build_dir)
    only = {n.lower() for n in args.only.split(",")} if args.only else None
    bundles = find_bundles(build_dir, only)
    known = plugin_names()
    if not bundles:
        runner.say(f"No built .vst3 bundles under {build_dir}. Build first: python3 scripts/install_plugins.py --build")
        return 1
    built = {bundle_product(b.name).lower() for b in bundles}
    for name in known:
        if name.lower() not in built and (not only or name.lower() in only):
            runner.say(f"warning: plugin '{name}' is in plugins/ but has no built bundle in {build_dir}")

    destination_dir, others = target_dirs(args, os_name)
    runner.say(f"Installing {len(bundles)} plugin(s) into {destination_dir}")
    if os_name == "windows" and not args.prefix and destination_dir == standard_dirs(os_name)["user"][0] \
            and not args.user:
        runner.say("note: not running as administrator, so the per-user VST3 folder is used "
                   "(hosts scan it too). Use an elevated terminal with --system for %COMMONPROGRAMFILES%\\VST3.")

    for bundle in bundles:
        new_stamp = read_stamp(bundle)
        old = destination_dir / bundle.name
        if old.exists():
            old_stamp = read_stamp(old)
            runner.say(f"- {bundle.name}: replacing existing copy ({old_stamp or 'no build stamp: older build'})")
        else:
            runner.say(f"- {bundle.name}: new install")
        runner.copy_bundle(bundle, old)
        prepare_for_host(runner, old, os_name)
        if not args.keep_stale:
            for stale in installed_copies(others, [bundle_vst3_stem(bundle.name)]):
                runner.say(f"  stale copy in another VST3 folder ({read_stamp(stale) or 'no build stamp'}): {stale}")
                runner.remove(stale)
        else:
            for stale in installed_copies(others, [bundle_vst3_stem(bundle.name)]):
                runner.say(f"  warning: another copy exists at {stale}; a host may load that one instead")

    report(runner, [destination_dir / b.name for b in bundles], dry_run=runner.dry_run)
    return 1 if runner.errors else 0


def report(runner: Runner, paths: list[Path], dry_run: bool) -> None:
    runner.say()
    runner.say("Installed plugins" + (" (dry run: nothing was written)" if dry_run else "") + ":")
    head = repo_head()
    for path in paths:
        stamp = read_stamp(path) if path.exists() else None
        runner.say(f"  {path}")
        runner.say(f"      build stamp: {stamp if stamp else '(not readable)' if path.exists() else '(would be installed)'}")
        if stamp and head and not stamp.sha.startswith(head) and not dry_run:
            runner.say(f"      note: built from {stamp.sha}, but this checkout is at {head}; rebuild with --build --clean")


def do_uninstall(args, runner: Runner, os_name: str) -> int:
    dirs = [Path(args.prefix).expanduser()] if args.prefix else [d for lst in standard_dirs(os_name).values() for d in lst]
    names = installed_product_names()
    if getattr(args, "only", None):
        only = {p.strip().lower() for p in args.only.split(",") if p.strip()}
        names = [n for n in names if n.replace(" Local", "").lower() in only]
    copies = installed_copies(dirs, names)
    if not copies:
        runner.say("No Animal Audio plugins found in the VST3 folders: " + ", ".join(str(d) for d in dirs))
        return 0
    for copy in copies:
        runner.say(f"- {copy.name} ({read_stamp(copy) or 'no build stamp'})")
        runner.remove(copy)
    runner.say("\nRescan plug-ins in your host afterwards so it forgets them.")
    return 1 if runner.errors else 0


def do_list(args, runner: Runner, os_name: str) -> int:
    dirs = [Path(args.prefix).expanduser()] if args.prefix else [d for lst in standard_dirs(os_name).values() for d in lst]
    copies = installed_copies(dirs, installed_product_names())
    head = repo_head()
    runner.say(f"This checkout is at {head or 'unknown'}. Animal Audio plugins installed on this machine:")
    if not copies:
        runner.say("  (none)")
    for copy in copies:
        stamp = read_stamp(copy)
        mark = "" if (stamp and head and stamp.sha.startswith(head)) else "   <- not this checkout's commit"
        runner.say(f"  {copy}\n      build stamp: {stamp if stamp else 'none (built before build stamps existed)'}{mark}")
    if len({c.name for c in copies}) != len(copies):
        runner.say("\nwarning: the same plugin is installed in more than one folder; remove the stale one(s) "
                   "(re-run the install, which removes them).")
    return 0


HOST_INSTRUCTIONS = """
Next, in Ableton Live:
  1. Preferences > Plug-ins: make sure "Use VST3 Plug-In System Folders" is ON.
  2. Click "Rescan". If a plugin does not appear (or still looks old), hold Alt (Windows) / Option (macOS) while
     clicking Rescan for a full rescan. Quit and reopen Live if it still shows the old build.
  3. Find the plugins under Plug-Ins > VST3 (category: Animal Audio), drop one on an audio track and test it.
  4. Verify the build: hover the ANIMAL AUDIO badge in the plugin UI (or read the small text at the bottom of the
     left panel). It must match the build stamp printed above.
"""


# ---------------------------------------------------------------- main

def parse_args(argv: Optional[list[str]] = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    action = p.add_mutually_exclusive_group()
    action.add_argument("--uninstall", action="store_true", help="remove every Animal Audio plugin from the VST3 folders")
    action.add_argument("--list", action="store_true", help="show installed copies and their build stamps")
    action.add_argument("--prereqs", action="store_true", help="only check build prerequisites")
    p.add_argument("--install-prereqs", action="store_true", help="install missing prerequisites (brew/winget/apt)")
    p.add_argument("--build", action="store_true", help="configure and build all plugins (Release) before installing")
    p.add_argument("--clean", action="store_true", help="with --build: delete the build directory first")
    p.add_argument("--universal", action="store_true", help="macOS: build arm64 + x86_64 (for Intel / Rosetta hosts)")
    p.add_argument("--only", help="comma-separated plugin names (default: all under plugins/)")
    p.add_argument("--build-dir", default=str(ROOT / "build"))
    scope = p.add_mutually_exclusive_group()
    scope.add_argument("--user", action="store_true", help="install into the per-user VST3 folder")
    scope.add_argument("--system", action="store_true", help="install into the machine-wide VST3 folder (needs admin)")
    p.add_argument("--prefix", help="install into / uninstall from this folder instead of the OS folders")
    p.add_argument("--keep-stale", action="store_true", help="do not remove copies from the other VST3 folders")
    p.add_argument("--yes", "-y", action="store_true", help="do not ask questions")
    p.add_argument("--dry-run", action="store_true", help="print what would happen, change nothing")
    return p.parse_args(argv)


def main(argv: Optional[list[str]] = None) -> int:
    args = parse_args(argv)
    os_name = detect_os()
    runner = Runner(args.dry_run)

    if args.prereqs or args.install_prereqs:
        if args.install_prereqs:
            install_prereqs(os_name, runner, args.yes)
        rows = check_prereqs(os_name)
        failed = False
        for name, ok, fix in rows:
            optional = "optional" in name
            runner.say(f"  [{'ok' if ok else ('--' if optional else 'MISSING')}] {name}" + ("" if ok else f"   -> {fix}"))
            failed |= not ok and not optional
        if failed or not args.build:
            return 1 if failed else 0

    if args.list:
        return do_list(args, runner, os_name)
    if args.uninstall:
        return do_uninstall(args, runner, os_name)

    if args.build:
        only = {n.lower() for n in args.only.split(",")} if args.only else None
        build_all(runner, Path(args.build_dir), args.clean, only, args.universal, os_name)
    status = do_install(args, runner, os_name)
    if status == 0:
        runner.say(HOST_INSTRUCTIONS)
    return status


if __name__ == "__main__":
    sys.exit(main())
