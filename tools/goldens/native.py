"""Render Golden corpus Sets with the native renderer (test/goldens/golden_render.cpp), on this
machine (`--renderer`) or on an Android device or emulator (`--android`, #44).

The renderer runs the facade on the wx-free Skia host, as the app does. It needs, per Set, a
Template store, a shared font store and the system fonts the reference rendered with: for the
Test fixture its data/ and fonts/ only; for Tier 1/2 the pack's data/ and "Magic - Fonts", plus
Noto Sans (the fallback family, ADR 0004) and DejaVu Sans from this machine.

On Android everything is pushed under /data/local/tmp/mse-goldens with `adb push --sync`, which
only copies files that are newer than the device's, so the ~800 MiB pack is pushed once. Results
are pulled back into the run directory and compared like the desktop's.
"""

from __future__ import annotations

import hashlib
import os
import shlex
import shutil
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path

from common import REPO

DEVICE_ROOT = "/data/local/tmp/mse-goldens"
FIXTURE = REPO / "test" / "fixture"
# The program's own data, for the English locale the desktop CLI runs with
PROGRAM_DATA = REPO / "data"
LOCALE = "en"

# The fonts from outside the pack that the reference (wx, fontconfig) used: the fallback family
# and DejaVu Sans, which some Styles ask for. Found on this machine with fc-match.
SYSTEM_FONTS = ["Noto Sans:style=Regular", "Noto Sans:style=Bold", "Noto Sans:style=Italic",
                "Noto Sans:style=Bold Italic", "DejaVu Sans:style=Book", "DejaVu Sans:style=Bold",
                "DejaVu Sans:style=Oblique"]


@dataclass
class Profile:
    """Where a group of Sets gets its Templates and fonts."""
    name: str                 # "fixture" or "pack"
    templates: Path
    font_store: Path | None
    system_fonts: Path        # a directory of font files


@dataclass
class Job:
    profile: Profile
    set_path: Path
    out_dir: Path             # local run directory of the Set


def system_font_dir(goldens: Path) -> Path:
    """A directory with copies of SYSTEM_FONTS, as this machine's fontconfig finds them.

    Made once next to the goldens, and reused as it is: CI jobs get it with the goldens, so they
    render with the reference machine's files and need no fontconfig of their own.
    """
    d = goldens / "native-system-fonts"
    if d.is_dir() and len(list(d.glob("*.ttf"))) >= len(SYSTEM_FONTS):
        return d
    d.mkdir(parents=True, exist_ok=True)
    for pattern in SYSTEM_FONTS:
        path = subprocess.run(["fc-match", "-f", "%{file}", pattern], capture_output=True, text=True).stdout.strip()
        if not path:
            raise SystemExit(f"fc-match found no font for {pattern!r}")
        target = d / Path(path).name
        if not target.exists():
            shutil.copy2(path, target)
    return d


def profiles(goldens: Path, pack: Path, templates: Path | None, font_store: Path | None) -> dict[str, Profile]:
    fixture_fonts = FIXTURE / "fonts"
    out = {"fixture": Profile("fixture", FIXTURE / "data", None, fixture_fonts)}
    if templates:
        out["pack"] = Profile("pack", templates, font_store, system_font_dir(goldens))
    elif (pack / "data").is_dir():
        out["pack"] = Profile("pack", pack / "data", pack / "Magic - Fonts", system_font_dir(goldens))
    return out


def read_result(out_dir: Path) -> tuple[str, str]:
    """(status, output) of a rendered Set, classified like run_cli's."""
    log = out_dir / "render.log"
    if not log.exists():
        return "crash", ""
    text = log.read_text(encoding="utf-8", errors="replace")
    if "status: " not in text:
        return "crash", text
    if "status: 0" not in text or "ERROR" in text:
        return "error", text
    return ("warning" if "WARNING" in text else "ok"), text


def chunks(items: list, n: int) -> list[list]:
    n = max(1, min(n, len(items)))
    return [items[i::n] for i in range(n)]


# ----------------------------------------------------------------------------- : Locally

def render_local(renderer: Path, jobs: list[Job], parallel: int) -> None:
    """Run the renderer on this machine, `parallel` processes per profile."""
    by_profile: dict[str, list[Job]] = {}
    for j in jobs:
        by_profile.setdefault(j.profile.name, []).append(j)
    procs = []
    for group in by_profile.values():
        prof = group[0].profile
        env = dict(os.environ, MSE_SYSTEM_FONT_DIRS=str(prof.system_fonts))
        for chunk in chunks(group, parallel):
            args = [str(renderer), "--templates", str(prof.templates),
                    "--program-data", str(PROGRAM_DATA), "--locale", LOCALE]
            if prof.font_store:
                args += ["--font-store", str(prof.font_store)]
            for j in chunk:
                args += [str(j.set_path), str(j.out_dir)]
            procs.append(subprocess.Popen(args, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL))
    for p in procs:
        p.wait()


# ----------------------------------------------------------------------------- : On Android

class Device:
    def __init__(self, adb: str, serial: str | None):
        self.base = [adb] + (["-s", serial] if serial else [])

    def run(self, *args: str, capture: bool = False) -> str:
        r = subprocess.run(self.base + list(args), check=True, capture_output=capture, text=True)
        return r.stdout if capture else ""

    def shell(self, command: str, capture: bool = True) -> str:
        return self.run("shell", command, capture=capture)

    def push_sync(self, local: Path, remote: str) -> None:
        """Push a file or directory's contents to remote, copying only what changed."""
        self.shell(f"mkdir -p {shlex.quote(remote)}")
        if local.is_dir():
            # (push the directory's contents into remote, not the directory into it)
            self.run("push", "--sync", "-q", str(local) + "/.", remote)
        else:
            self.run("push", "--sync", "-q", str(local), remote)


def find_adb() -> str:
    adb = shutil.which("adb")
    if adb:
        return adb
    home = os.environ.get("ANDROID_HOME") or str(Path.home() / "Android" / "Sdk")
    adb = Path(home) / "platform-tools" / "adb"
    if not adb.exists():
        raise SystemExit("adb not found: put it on PATH or set ANDROID_HOME")
    return str(adb)


def stripped(renderer: Path) -> Path:
    """A stripped copy of the renderer (the NDK's llvm-strip), cached next to it."""
    out = renderer.with_name(renderer.name + ".stripped")
    if out.exists() and out.stat().st_mtime >= renderer.stat().st_mtime:
        return out
    ndk = os.environ.get("ANDROID_NDK_HOME")
    strip = next(iter(sorted(Path(ndk).glob("toolchains/llvm/prebuilt/*/bin/llvm-strip"))), None) if ndk else None
    shutil.copy2(renderer, out)
    if strip:
        subprocess.run([str(strip), str(out)], check=True)
    return out


def remote_set_path(prof: Profile, set_path: Path) -> str:
    digest = hashlib.sha1(str(set_path.parent).encode()).hexdigest()[:10]
    return f"{DEVICE_ROOT}/sets/{prof.name}-{digest}/{set_path.name}"


def render_android(renderer: Path, jobs: list[Job], parallel: int, serial: str | None) -> None:
    dev = Device(find_adb(), serial)
    dev.run("wait-for-device")
    cores = int(dev.shell("nproc").strip() or "1")
    parallel = max(1, min(parallel, cores))
    # the renderer and the inputs, pushed only where they changed
    binary = stripped(renderer)
    dev.push_sync(binary, f"{DEVICE_ROOT}/bin")
    remote_bin = f"{DEVICE_ROOT}/bin/{binary.name}"
    dev.shell(f"chmod 755 {remote_bin}")
    dev.push_sync(PROGRAM_DATA / f"{LOCALE}.mse-locale", f"{DEVICE_ROOT}/program-data/{LOCALE}.mse-locale")
    pushed_profiles: set[str] = set()
    pushed_set_dirs: set[Path] = set()
    for j in jobs:
        prof = j.profile
        if prof.name not in pushed_profiles:
            print(f"pushing {prof.name} Templates and fonts (only what changed)...", flush=True)
            dev.push_sync(prof.templates, f"{DEVICE_ROOT}/{prof.name}/templates")
            if prof.font_store:
                dev.push_sync(prof.font_store, f"{DEVICE_ROOT}/{prof.name}/font-store")
            dev.push_sync(prof.system_fonts, f"{DEVICE_ROOT}/{prof.name}/system-fonts")
            pushed_profiles.add(prof.name)
        if j.set_path.parent not in pushed_set_dirs:
            remote_dir = remote_set_path(prof, j.set_path).rsplit("/", 1)[0]
            dev.push_sync(j.set_path.parent, remote_dir)
            pushed_set_dirs.add(j.set_path.parent)
    # run, `parallel` processes per profile, into a fresh output directory
    out_root = f"{DEVICE_ROOT}/out"
    dev.shell(f"rm -rf {out_root} && mkdir -p {out_root}")
    by_profile: dict[str, list[Job]] = {}
    for j in jobs:
        by_profile.setdefault(j.profile.name, []).append(j)
    commands = []
    names: dict[str, Job] = {}
    for group in by_profile.values():
        prof = group[0].profile
        base = f"{DEVICE_ROOT}/{prof.name}"
        for chunk in chunks(group, parallel):
            args = [remote_bin, "--templates", f"{base}/templates",
                    "--program-data", f"{DEVICE_ROOT}/program-data", "--locale", LOCALE]
            if prof.font_store:
                args += ["--font-store", f"{base}/font-store"]
            for j in chunk:
                name = f"{len(names)}"
                names[name] = j
                args += [remote_set_path(prof, j.set_path), f"{out_root}/{name}"]
            commands.append(f"MSE_SYSTEM_FONT_DIRS={base}/system-fonts " + " ".join(shlex.quote(a) for a in args) + " >/dev/null 2>&1 &")
    print(f"rendering {len(jobs)} Sets on the device, {len(commands)} processes...", flush=True)
    dev.shell(" ".join(commands) + " wait", capture=False)
    # pull the results back into each Set's run directory
    with tempfile.TemporaryDirectory() as tmp:
        dev.run("pull", "-q", out_root, tmp)
        pulled = Path(tmp) / "out"
        for name, j in names.items():
            if j.out_dir.exists():
                shutil.rmtree(j.out_dir)
            j.out_dir.parent.mkdir(parents=True, exist_ok=True)
            src = pulled / name
            if src.exists():
                shutil.move(str(src), j.out_dir)
            else:
                j.out_dir.mkdir()
