#!/usr/bin/env python3
"""Capture and check Golden images and Layout dumps (ADR 0010).

  goldens.py capture   render every card to PNG + layout dump under <goldens>/images
  goldens.py check     render again into <goldens>/check-run and compare with the goldens

Corpora: Tier 1 and Tier 2 Sets (under <goldens>, from generate_tier1.py and tier2_sets.py)
and the public Test fixture (test/fixture). `--fixture-only` uses just the fixture, for CI
without the private pack (ADR 0008). `--templates DIR --font-store DIR` renders the Tier 1/2
Sets with a Template store and shared font store made by Template import instead of the pack
(fontconfig then has no pack fonts); its run and report get an `-imported` suffix.

`--renderer PATH` renders with the native renderer (test/goldens/golden_render.cpp: the facade
on the wx-free Skia host) instead of the desktop CLI, and `--android` runs that renderer on an
Android device or emulator (#44, see native.py); their runs and reports get a `-native` or
`-android` suffix.

The Layout dump is the gate: `check` exits 1 if any card's dump differs from its golden (or a
golden card no longer renders). The pixel comparison (SSIM) is advisory and only goes into the
report. Both modes write <goldens>/report-<mode>.md and styles.tsv, the per-Style pass/fail list
that Unsupported style status is read from.
"""

from __future__ import annotations

import argparse
import difflib
import shutil
import sys
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import (  # noqa: E402
    DEFAULT_GOLDENS, DEFAULT_MSE, DEFAULT_PACK, REPO, mse_env, mse_string, run_cli,
)

FIXTURE = REPO / "test" / "fixture"
# The Test fixture's Golden images are free, so they are in the repository (#52), not with the
# private goldens: a public clone checks the fixture against them.
FIXTURE_GOLDENS = FIXTURE / "goldens"


def golden_dir(images: Path, gs: "GoldenSet") -> Path:
    """Where a Set's Golden images are: the fixture's in the repository, the others under images/."""
    return FIXTURE_GOLDENS / gs.name if gs.corpus == "fixture" else images / gs.corpus / gs.name
SSIM_WARN = 0.98  # below this a card's pixels are flagged (advisory only)
try:  # the pixel comparison needs numpy and Pillow; without them only the Layout dumps are compared
    import numpy  # noqa: F401
    import PIL  # noqa: F401
    HAVE_SSIM = True
except ImportError:
    HAVE_SSIM = False


@dataclass
class GoldenSet:
    corpus: str      # tier1 / tier2 / fixture
    name: str        # set directory name without .mse-set
    path: Path
    style: str       # the Set's Style, for the per-Style list
    env: dict[str, str]

    @property
    def key(self) -> str:
        return f"{self.corpus}/{self.name}"


@dataclass
class SetResult:
    gs: GoldenSet
    render_status: str = "ok"
    render_output: str = ""
    cards: list[str] = field(default_factory=list)                    # card ids rendered
    dump_diffs: dict[str, str] = field(default_factory=dict)          # card -> unified diff
    missing: list[str] = field(default_factory=list)                  # golden cards not rendered now
    ssim: dict[str, float] = field(default_factory=dict)              # card -> SSIM

    @property
    def dump_ok(self) -> bool:
        return not self.dump_diffs and not self.missing


def set_style(set_path: Path) -> str:
    for line in (set_path / "set").read_text(encoding="utf-8-sig").splitlines():
        if line.startswith("stylesheet:"):
            return line.split(":", 1)[1].strip()
    return "?"


def fixture_env(goldens: Path) -> dict[str, str]:
    """Only the fixture's bundled fonts and packages, as in ctest."""
    import os
    env = dict(os.environ)
    conf_dir = goldens / "fixture-fontconfig"
    conf_dir.mkdir(parents=True, exist_ok=True)
    conf = conf_dir / "fonts.conf"
    conf.write_text(
        '<?xml version="1.0"?>\n<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">\n<fontconfig>\n'
        f"  <dir>{FIXTURE / 'fonts'}</dir>\n  <cachedir>{conf_dir / 'cache'}</cachedir>\n</fontconfig>\n",
        encoding="utf-8",
    )
    env["FONTCONFIG_FILE"] = str(conf)
    env["MSE_TEMPLATES_DIR"] = str(FIXTURE / "data")
    return env


def collect(args) -> list[GoldenSet]:
    sets: list[GoldenSet] = []
    for p in sorted((FIXTURE / "sets").glob("*.mse-set")):
        sets.append(GoldenSet("fixture", p.name.removesuffix(".mse-set"), p, "fixture-" + set_style(p), fixture_env(args.goldens)))
    if not args.fixture_only:
        templates = getattr(args, "templates", None)
        if templates:
            env = mse_env(args.pack, args.goldens, templates, getattr(args, "font_store", None))
        elif not (args.pack / "data").is_dir():
            sys.exit(f"no template pack at {args.pack}: copy one there or pass --pack, or use --fixture-only")
        else:
            env = mse_env(args.pack, args.goldens)
        for corpus in ("tier1", "tier2"):
            for p in sorted((args.goldens / corpus / "sets").glob("*.mse-set")):
                style = "magic-" + set_style(p)
                sets.append(GoldenSet(corpus, p.name.removesuffix(".mse-set"), p, style, env))
        if not any(s.corpus != "fixture" for s in sets):
            sys.exit(f"no Tier 1/2 Sets under {args.goldens}; run generate_tier1.py and tier2_sets.py first")
    if args.only:
        sets = [s for s in sets if s.name in args.only or s.style in args.only or s.key in args.only]
    return sets


def render(gs: GoldenSet, out_dir: Path, mse: Path, timeout: float) -> SetResult:
    """Render every card of a Set to <out_dir>/<n>.png and <n>.json."""
    if out_dir.exists():
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True)
    prefix = mse_string(str(out_dir) + "/")
    pos = "position(of: c, in: set.cards)"
    commands = (
        f"for each c in set.cards do write_layout_file(c, file: {prefix} + {pos} + \".json\")\n"
        f"for each c in set.cards do write_image_file(c, file: {prefix} + {pos} + \".png\")\n"
    )
    r = run_cli(mse, gs.path, commands, gs.env, timeout)
    cards = sorted((p.stem for p in out_dir.glob("*.json")), key=int)
    return SetResult(gs, r.status, r.output, cards)


def ssim(a_path: Path, b_path: Path) -> float:
    import numpy as np
    from PIL import Image

    def load(p):
        img = Image.open(p).convert("RGBA")
        bg = Image.new("RGBA", img.size, (255, 255, 255, 255))
        return np.asarray(Image.alpha_composite(bg, img).convert("L"), dtype=np.float64)

    a, b = load(a_path), load(b_path)
    if a.shape != b.shape:
        return 0.0
    k = 7

    def box(x):
        c = np.cumsum(np.cumsum(np.pad(x, ((1, 0), (1, 0))), 0), 1)
        return (c[k:, k:] - c[:-k, k:] - c[k:, :-k] + c[:-k, :-k]) / (k * k)

    if min(a.shape) < k:
        return 1.0 if np.array_equal(a, b) else 0.0
    mu_a, mu_b = box(a), box(b)
    var_a = box(a * a) - mu_a ** 2
    var_b = box(b * b) - mu_b ** 2
    cov = box(a * b) - mu_a * mu_b
    c1, c2 = (0.01 * 255) ** 2, (0.03 * 255) ** 2
    s = ((2 * mu_a * mu_b + c1) * (2 * cov + c2)) / ((mu_a ** 2 + mu_b ** 2 + c1) * (var_a + var_b + c2))
    return float(s.mean())


def compare(res: SetResult, golden_dir: Path, run_dir: Path) -> None:
    golden_cards = sorted((p.stem for p in golden_dir.glob("*.json")), key=int)
    for card in golden_cards:
        new = run_dir / f"{card}.json"
        if not new.exists():
            res.missing.append(card)
            continue
        old_text = (golden_dir / f"{card}.json").read_text(encoding="utf-8")
        new_text = new.read_text(encoding="utf-8")
        if old_text != new_text:
            diff = difflib.unified_diff(
                old_text.splitlines(), new_text.splitlines(),
                f"golden/{res.gs.key}/{card}.json", f"now/{res.gs.key}/{card}.json", lineterm="", n=1,
            )
            res.dump_diffs[card] = "\n".join(diff)
        if HAVE_SSIM and (golden_dir / f"{card}.png").exists() and (run_dir / f"{card}.png").exists():
            res.ssim[card] = ssim(golden_dir / f"{card}.png", run_dir / f"{card}.png")


def write_styles(path: Path, results: list[SetResult], mode: str) -> None:
    """Per-Style status: a Style passes only if every Set using it rendered (and, in check, matched)."""
    status: dict[str, str] = {}
    for r in results:
        ok = r.render_status in ("ok", "warning") and (mode == "capture" or r.dump_ok)
        if not ok or r.gs.style not in status:
            if status.get(r.gs.style) != "fail":
                status[r.gs.style] = "pass" if ok else "fail"
    path.write_text("style\tstatus\n" + "".join(f"{s}\t{v}\n" for s, v in sorted(status.items())), encoding="utf-8")


def style_status(results: list[SetResult]) -> dict[str, str]:
    """A Style passes only if every Set using it rendered and its Layout dumps match."""
    status: dict[str, str] = {}
    for r in results:
        ok = r.render_status in ("ok", "warning") and r.dump_ok
        status[r.gs.style] = "pass" if ok and status.get(r.gs.style, "pass") == "pass" else "fail"
    return status


def write_status(path: Path, results: list[SetResult], data: Path) -> None:
    """The app's Style support status (#72, ADR 0010): each Style the Android run tested, with the
    version tested; a Style that failed the Layout dump gate (or didn't render) is unsupported.
    The app treats Styles not listed, or of another version, as untested."""
    lines = []
    for style, st in sorted(style_status([r for r in results if r.gs.corpus != "fixture"]).items()):
        version = ""
        style_file = data / f"{style}.mse-style" / "style"
        if style_file.exists():
            for line in style_file.read_text(encoding="utf-8-sig", errors="replace").splitlines():
                if line.startswith("version:"):
                    version = line.split(":", 1)[1].strip()
                    break
        lines.append(f"{style}.mse-style\t{version}\t{'supported' if st == 'pass' else 'unsupported'}\n")
    path.write_text(
        "# Style support for this app version: an Android golden run (tools/goldens/goldens.py --android\n"
        "# --status-out, ADR 0010). package, version tested, supported/unsupported. Styles not listed\n"
        "# (or of another version) are untested.\n" + "".join(lines),
        encoding="utf-8",
    )
    print(f"status: {path} ({sum(1 for l in lines if l.endswith('unsupported\n'))} unsupported of {len(lines)})")


def write_report(path: Path, results: list[SetResult], mode: str) -> None:
    rendered = sum(len(r.cards) for r in results)
    errors = [r for r in results if r.render_status not in ("ok", "warning")]
    out = [f"# Goldens {mode}", "", f"{len(results)} Sets, {rendered} cards rendered, {len(errors)} Sets failed to render.", ""]
    if mode == "check":
        diffs = [r for r in results if not r.dump_ok]
        flagged = [(r, c, v) for r in results for c, v in r.ssim.items() if v < SSIM_WARN]
        out += [f"**Layout dump: {'PASS' if not diffs else f'FAIL ({len(diffs)} Sets differ)'}**",
                f"Pixels (advisory): {len(flagged)} cards below SSIM {SSIM_WARN}." if HAVE_SSIM
                else "Pixels (advisory): not compared (numpy and Pillow are not installed).", ""]
        for r in diffs:
            out += [f"## Layout differs: {r.gs.key} ({r.gs.style})", ""]
            if r.missing:
                out.append(f"Golden cards not rendered now: {', '.join(r.missing)}")
            for card, d in r.dump_diffs.items():
                lines = d.splitlines()
                out += ["```diff", *lines[:60], *(["... (truncated)"] if len(lines) > 60 else []), "```", ""]
        if flagged:
            out += ["## Pixel differences (advisory)", "", "| Set | Card | SSIM |", "|---|---|---|"]
            out += [f"| {r.gs.key} | {c} | {v:.4f} |" for r, c, v in sorted(flagged, key=lambda x: x[2])]
            out.append("")
    if errors:
        out += ["## Render failures", "", "| Set | Style | Status | First messages |", "|---|---|---|---|"]
        for r in errors:
            msgs = [l.strip() for l in r.render_output.splitlines() if "ERROR" in l][:2]
            out.append(f"| {r.gs.key} | {r.gs.style} | {r.render_status} | {' / '.join(msgs).replace('|', '/')[:200]} |")
    path.write_text("\n".join(out) + "\n", encoding="utf-8")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("mode", choices=["capture", "check"])
    ap.add_argument("--pack", type=Path, default=DEFAULT_PACK)
    ap.add_argument("--goldens", type=Path, default=DEFAULT_GOLDENS)
    ap.add_argument("--mse", type=Path, default=DEFAULT_MSE)
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("--timeout", type=float, default=180)
    ap.add_argument("--fixture-only", action="store_true", help="only the public Test fixture (no pack needed)")
    ap.add_argument("--templates", type=Path, help="an imported Template store to use instead of the pack's data/")
    ap.add_argument("--font-store", type=Path, help="with --templates: its shared font store")
    ap.add_argument("--renderer", type=Path, help="render with this native golden_render instead of --mse")
    ap.add_argument("--android", action="store_true",
                    help="run the renderer on an Android device (default renderer: build-android-x86_64/golden_render)")
    ap.add_argument("--serial", help="with --android: the adb serial of the device")
    ap.add_argument("--status-out", type=Path,
                    help="with --android check of the whole corpus: write the app's Style support status file here "
                         "(android/app/src/main/assets/style-status.tsv, #72)")
    ap.add_argument("only", nargs="*", help="limit to these Sets, Styles or corpus/set keys")
    args = ap.parse_args()
    if args.templates and args.mode == "capture":
        ap.error("--templates is only for check: goldens are captured from the pack")
    if args.font_store and not args.templates:
        ap.error("--font-store needs --templates")

    if (args.renderer or args.android) and args.mode == "capture":
        ap.error("goldens are captured from the desktop reference; --renderer and --android are only for check")
    if args.android and not args.renderer:
        args.renderer = REPO / "build-android-x86_64" / "golden_render"
    if args.status_out and not (args.android and args.mode == "check" and not args.only and not args.fixture_only):
        ap.error("--status-out needs an --android check of the whole corpus")
    if args.renderer and not args.renderer.exists():
        ap.error(f"no renderer at {args.renderer}: build the golden_render target first")

    sets = collect(args)
    images = args.goldens / "images"
    imported = "-imported" if args.templates else ""
    imported += "-android" if args.android else "-native" if args.renderer else ""
    run_root = images if args.mode == "capture" else args.goldens / f"check-run{imported}"

    if args.renderer:
        results = render_native(args, sets, run_root, images)
        if args.status_out:
            write_status(args.status_out, results, args.templates or args.pack / "data")
        return finish(args, results, imported)

    def work(gs: GoldenSet) -> SetResult:
        out = golden_dir(images, gs) if args.mode == "capture" else run_root / gs.corpus / gs.name
        res = render(gs, out, args.mse, args.timeout)
        if args.mode == "check":
            compare(res, golden_dir(images, gs), out)
        return res

    results: list[SetResult] = []
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for n, res in enumerate(pool.map(work, sets), 1):
            results.append(res)
            mark = "" if args.mode == "capture" else ("  dump ok" if res.dump_ok else "  DUMP DIFFERS")
            print(f"[{n}/{len(sets)}] {res.render_status:8} {res.gs.key}{mark}", flush=True)

    return finish(args, results, imported)


def render_native(args, sets: list[GoldenSet], run_root: Path, images: Path) -> list[SetResult]:
    """Render with the native renderer, here or on Android, then compare (check only)."""
    import native
    profs = native.profiles(args.goldens, args.pack, args.templates, args.font_store)
    jobs = [native.Job(profs["fixture" if gs.corpus == "fixture" else "pack"], gs.path, run_root / gs.corpus / gs.name)
            for gs in sets]
    for j in jobs:
        if j.out_dir.exists():
            shutil.rmtree(j.out_dir)
        j.out_dir.mkdir(parents=True)
    if args.android:
        native.render_android(args.renderer, jobs, args.jobs, args.serial)
    else:
        native.render_local(args.renderer, jobs, args.jobs)
    results = []
    for n, (gs, j) in enumerate(zip(sets, jobs), 1):
        status, output = native.read_result(j.out_dir)
        cards = sorted((p.stem for p in j.out_dir.glob("*.json")), key=int)
        res = SetResult(gs, status, output, cards)
        compare(res, golden_dir(images, gs), j.out_dir)
        results.append(res)
        print(f"[{n}/{len(sets)}] {res.render_status:8} {gs.key}{'  dump ok' if res.dump_ok else '  DUMP DIFFERS'}", flush=True)
    return results


def finish(args, results: list[SetResult], imported: str) -> int:
    suffix = ("-fixture" if args.fixture_only else "") + imported
    write_report(args.goldens / f"report-{args.mode}{suffix}.md", results, args.mode)
    write_styles(args.goldens / f"styles{suffix}.tsv", results, args.mode)
    print(f"report: {args.goldens / f'report-{args.mode}{suffix}.md'}")
    if args.mode == "check":
        bad = [r for r in results if not r.dump_ok]
        for r in bad:
            for d in r.dump_diffs.values():
                print("\n".join(d.splitlines()[:30]))
        print(f"layout dump: {'PASS' if not bad else f'FAIL, {len(bad)} Sets differ'}")
        return 1 if bad else 0
    return 0


if __name__ == "__main__":
    sys.exit(main())
