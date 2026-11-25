#!/usr/bin/env python3
"""Build a slimmer Template pack from a full one (personal use; the result stays private, ADR 0008).

    tools/pack/thin.py <full-pack-dir> <drop-list> <out-dir> [--zip out.zip]

<full-pack-dir> is an unpacked pack (with data/ and its fonts directory, e.g. the Full Magic Pack).
<drop-list> names what to leave out, one per line; `#` starts a comment. A line is a package
(`magic-tenth.mse-style`, `saga.mse-game`, `english.mse-locale`) or `group: <installer group>`,
which drops every package whose installer group starts with it (`group: magic/Deprecated`).
Every other Style, export template and locale is kept, with everything it needs, transitively:
the packages its `depends on` names, those whose files it reaches by path
("/magic-modules.mse-include/..."), and symbol fonts by name. A dropped package that a kept one
needs is kept anyway, and reported. Packages nothing kept needs (Games, includes, symbol fonts)
are left out; dictionaries and the fonts are kept whole. The output has the pack's layout, so the app imports it like the full pack.
"""
from __future__ import annotations

import argparse
import re
import shutil
import sys
import zipfile
from pathlib import Path

PKG = re.compile(r"[A-Za-z0-9_.\-]+\.mse-(?:game|style|include|symbol-font|export-template|locale|updater)\b")
# the package files that hold a package's description and scripts
TEXT = {".mse-style": "style", ".mse-game": "game", ".mse-include": "include",
        ".mse-symbol-font": "symbol-font", ".mse-export-template": "export-template", ".mse-locale": "locale",
        ".mse-updater": "updater"}


def references(pkg: Path, bare: re.Pattern | None = None) -> set[str]:
    """Other packages this one names: in `depends on` and in paths, in any of its text files, and
    symbol fonts also by their bare name, which is how Styles name them (`symbol font: name:
    magic-mana-small`)"""
    found: set[str] = set()
    for f in pkg.rglob("*"):
        if not f.is_file() or f.suffix.lower() in {".png", ".jpg", ".jpeg", ".gif", ".bmp", ".ttf", ".otf", ".mse-symbol", ".zip"}:
            continue
        try:
            text = f.read_text(encoding="utf-8-sig", errors="replace")
        except OSError:
            continue
        found |= set(PKG.findall(text))
        if bare:
            found |= {m + ".mse-symbol-font" for m in bare.findall(text)}
    found.discard(pkg.name)
    return found


def installer_group(pkg: Path) -> str:
    """The package's `installer group` (as the import picker groups it), or ''"""
    f = pkg / TEXT.get(pkg.suffix, "")
    try:
        for line in f.read_text(encoding="utf-8-sig", errors="replace").splitlines():
            if line.startswith("installer group:"):
                return line.split(":", 1)[1].strip()
    except OSError:
        pass
    return ""


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("pack", type=Path)
    ap.add_argument("drop", type=Path)
    ap.add_argument("out", type=Path)
    ap.add_argument("--zip", type=Path)
    a = ap.parse_args()
    data = a.pack / "data"
    packages = {p.name: p for p in data.iterdir() if p.is_dir() and p.suffix in TEXT}
    lines = {l.split("#")[0].strip() for l in a.drop.read_text().splitlines()} - {""}
    groups = [l[len("group:"):].strip() for l in lines if l.startswith("group:")]
    drop = {l for l in lines if not l.startswith("group:")}
    unknown = sorted(d for d in drop if d not in packages)
    if unknown:
        sys.exit("not in the pack: " + ", ".join(unknown))
    for name, path in packages.items():
        g = installer_group(path)
        if any(g == x or g.startswith(x + "/") for x in groups):
            drop.add(name)

    top = {n for n in packages if n.endswith((".mse-style", ".mse-export-template", ".mse-locale", ".mse-updater"))}
    keep = top - drop
    fonts = sorted((n[: -len(".mse-symbol-font")] for n in packages if n.endswith(".mse-symbol-font")), key=len, reverse=True)
    bare = re.compile(r"(?<![\w.-])(" + "|".join(map(re.escape, fonts)) + r")(?![\w.-])")
    todo, needed_by = list(keep), {}
    while todo:
        name = todo.pop()
        for ref in references(packages[name], bare):
            if ref in packages and ref not in keep:
                keep.add(ref)
                needed_by[ref] = name
                todo.append(ref)
    revived = sorted(n for n in drop if n in keep)

    if a.out.exists():
        shutil.rmtree(a.out)
    (a.out / "data").mkdir(parents=True)
    for name in sorted(keep):
        shutil.copytree(packages[name], a.out / "data" / name)
    # everything in data/ that isn't a package (dictionaries, ...) and beside it (the fonts)
    for p in data.iterdir():
        if p.name not in packages:
            (shutil.copytree if p.is_dir() else shutil.copy2)(p, a.out / "data" / p.name)
    for p in a.pack.iterdir():
        if p.is_dir() and "font" in p.name.lower():
            shutil.copytree(p, a.out / p.name)

    kinds = lambda names: {k: sum(n.endswith(k) for n in names) for k in TEXT}  # noqa: E731
    print(f"kept {len(keep)} packages: " + ", ".join(f"{v} {k[5:]}" for k, v in kinds(keep).items() if v))
    print(f"left out {len(packages) - len(keep)}: " + ", ".join(f"{v} {k[5:]}" for k, v in kinds(set(packages) - keep).items() if v))
    for n in revived:
        print(f"kept although dropped: {n} (needed by {needed_by.get(n, '?')})")
    if a.zip:
        a.zip.unlink(missing_ok=True)
        with zipfile.ZipFile(a.zip, "w", zipfile.ZIP_DEFLATED, compresslevel=1) as z:
            for f in sorted(a.out.rglob("*")):
                if f.is_file():
                    z.write(f, f.relative_to(a.out))
        print(f"{a.zip} ({a.zip.stat().st_size / 2**20:.0f} MiB)")


if __name__ == "__main__":
    main()
