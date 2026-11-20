#!/usr/bin/env python3
"""Generate the Tier 1 golden fixture Sets: one Set per `magic` Style, one Card each (ADR 0010).

For every templates/Full-Magic-Pack/data/magic-*.mse-style this writes
<out>/sets/<style>.mse-set/set: a one-card Set whose every field the Style draws holds standard
filler text. It then loads and renders each Set through `magicseteditor --cli` and writes
<out>/report.md: the Styles that fail to load or render (these seed Unsupported style status)
and those that render with warnings.

The Sets are private derived data (ADR 0008), so <out> defaults to templates/goldens/tier1,
beside the pack and outside git.
"""

from __future__ import annotations

import argparse
import re
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import (  # noqa: E402
    DEFAULT_GOLDENS, DEFAULT_MSE, DEFAULT_PACK, block_value, mse_env, mse_string,
    read_mse_file, read_with_includes, run_cli, top_level_blocks,
)

# Filler per base field name (the name without a trailing face number such as " 2").
FILLER = {
    "name": "Generated Test Card",
    "casting cost": "3WU",
    "super type": "<word-list-type>Legendary Creature</word-list-type>",
    "sub type": "<word-list-race>Human Wizard</word-list-race>",
    "rule text": (
        "Flying, vigilance\n"
        "<sym>T</sym>: Draw a card, then discard a card.\n"
        "Whenever this creature attacks, each opponent loses 2 life and you gain 2 life. "
        "This sentence is long so that the rules text has to reflow across several lines."
    ),
    "flavor text": "<i-flavor>Every field on this card holds standard filler text.</i-flavor>",
    "chapter text": "Draw a card.",
    "power": "3",
    "toughness": "4",
    "loyalty": "4",
    "illustrator": "Test Illustrator",
    "copyright": "Test copyright",
    "additional credit": "Test credit",
}
LOYALTY_COSTS = ["+1", "−2", "−7", "+2", "0", "−3"]

# Fields the Style draws that are composed by scripts from editable source fields.
COMPOSITES = {
    "type": ["super type", "sub type"],
    "text": ["rule text", "flavor text"],
    "pt": ["power", "toughness"],
}

# Game text fields that are bookkeeping or script-derived; filling them would override scripts.
SKIP = re.compile(
    r"^(shape|color category|exact color|color identity|color count|full name|alias|cmc|"
    r"reminder type|reminder cost|type|text|pt|partition|card number|custom card number|"
    r"card code text|card code|set language|set code|auto copyright|secondary copyright|"
    r"vorthos box|copyright line|hand mod|life mod|sorted notes|template|sorted time created|"
    r"sorted time modified|custom index|card faces|layout|image uris|special text)( \d+)?$"
)

FACE = re.compile(r"^(.*?)(?: (\d+))?$")


def norm(name: str) -> str:
    return name.strip().replace("_", " ").lower()


def game_text_fields(pack: Path) -> set[str]:
    """Names of the editable, saved text fields of the magic game."""
    text = read_mse_file(pack / "data" / "magic.mse-game" / "card_fields")
    names = set()
    for block in top_level_blocks(text, "card field"):
        if block_value(block, "type") != "text":
            continue
        if block_value(block, "editable") == "false" or block_value(block, "save value") == "false":
            continue
        names.add(norm(block_value(block, "name") or ""))
    return names


def styled_fields(style_file: Path) -> list[str]:
    """Field names the Style positions under `card style:` (its own and its includes'), in order."""
    blocks = top_level_blocks(read_with_includes(style_file, style_file.parent.parent), "card style")
    names = []
    for block in blocks:
        names += [norm(m) for m in re.findall(r"^\t([^\t:\n][^:\n]*):", block, re.M)]
    return list(dict.fromkeys(names))


def filler_for(field: str) -> str:
    base, face = FACE.match(field).groups()
    if base == "loyalty cost":
        return LOYALTY_COSTS[(int(face or 1) - 1) % len(LOYALTY_COSTS)]
    value = FILLER[base]
    if base == "name" and face:
        value = f"Generated Face {face}"
    return value


def card_values(styled: list[str], text_fields: set[str]) -> dict[str, str]:
    values: dict[str, str] = {}
    for field in styled:
        base, face = FACE.match(field).groups()
        suffix = f" {face}" if face else ""
        sources = [s + suffix for s in COMPOSITES.get(base, [])] or [field]
        for src in sources:
            if src in values or src not in text_fields or SKIP.match(src):
                continue
            # Only fields with known filler: generic text in e.g. "level 1" switches a
            # Style into leveler mode, which Tier 2 covers by hand instead.
            if FACE.match(src).group(1) in FILLER or src.startswith("loyalty cost"):
                values[src] = filler_for(src)
    return values


def set_file(stylesheet: str, values: dict[str, str]) -> str:
    lines = [
        "mse version: 2.6.0",
        "game: magic",
        f"stylesheet: {stylesheet}",
        # No `set info`: a key the game doesn't declare there hangs MSE 2.6's reader.
        "card:",
        "\trarity: rare",
    ]
    for key, value in values.items():
        if "\n" in value:
            lines.append(f"\t{key}:")
            lines += [f"\t\t{part}" for part in value.split("\n")]
        else:
            lines.append(f"\t{key}: {value}")
    return "\n".join(lines) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--pack", type=Path, default=DEFAULT_PACK, help="template pack root")
    ap.add_argument("--out", type=Path, default=DEFAULT_GOLDENS / "tier1", help="output directory")
    ap.add_argument("--mse", type=Path, default=DEFAULT_MSE, help="magicseteditor binary")
    ap.add_argument("--jobs", type=int, default=8, help="parallel magicseteditor processes")
    ap.add_argument("--timeout", type=float, default=120, help="seconds per Style")
    ap.add_argument("--no-check", action="store_true", help="only write the Sets")
    ap.add_argument("styles", nargs="*", help="only these Styles (e.g. magic-m15)")
    args = ap.parse_args()

    data = args.pack / "data"
    style_dirs = sorted(data.glob("magic-*.mse-style"))
    if args.styles:
        wanted = {s.removesuffix(".mse-style") for s in args.styles}
        style_dirs = [d for d in style_dirs if d.name.removesuffix(".mse-style") in wanted]
    if not style_dirs:
        print(f"no magic-*.mse-style under {data}", file=sys.stderr)
        return 1

    text_fields = game_text_fields(args.pack)
    sets_dir = args.out / "sets"
    sets: list[tuple[str, Path]] = []
    for style_dir in style_dirs:
        style = style_dir.name.removesuffix(".mse-style")
        values = card_values(styled_fields(style_dir / "style"), text_fields)
        set_path = sets_dir / f"{style}.mse-set"
        set_path.mkdir(parents=True, exist_ok=True)
        (set_path / "set").write_text(set_file(style.removeprefix("magic-"), values), encoding="utf-8")
        sets.append((style, set_path))
    print(f"wrote {len(sets)} Sets under {sets_dir}")
    if args.no_check:
        return 0

    env = mse_env(args.pack, args.out)
    check_dir = args.out / "check"
    check_dir.mkdir(parents=True, exist_ok=True)

    def check(item: tuple[str, Path]):
        style, set_path = item
        commands = (
            f"write_image_file(set.cards[0], file: {mse_string(str(check_dir / (style + '.png')))})\n"
            f"write_layout_file(set.cards[0], file: {mse_string(str(check_dir / (style + '.json')))})\n"
        )
        return style, run_cli(args.mse, set_path, commands, env, args.timeout)

    results = []
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for n, (style, result) in enumerate(pool.map(check, sets), 1):
            results.append((style, result))
            print(f"[{n}/{len(sets)}] {result.status:8} {style}", flush=True)

    write_report(args.out / "report.md", results)
    failed = sum(r.status not in ("ok", "warning") for _, r in results)
    print(f"{failed} of {len(results)} Styles failed; report: {args.out / 'report.md'}")
    return 0


def first_lines(output: str, n: int = 3) -> str:
    lines = [l.strip() for l in output.splitlines() if l.strip() and ("ERROR" in l or "WARNING" in l)]
    lines = lines or [l.strip() for l in output.splitlines() if l.strip()]
    return " / ".join(dict.fromkeys(lines[:n])).replace("|", "\\|")[:300]


def write_report(path: Path, results) -> None:
    by_status: dict[str, list] = {}
    for style, r in results:
        by_status.setdefault(r.status, []).append((style, r))
    out = [
        "# Tier 1 Style check",
        "",
        "Each generated Set loaded and rendered through `magicseteditor --cli` "
        "(`write_image_file` + `write_layout_file`).",
        "",
        "| Status | Styles |",
        "|---|---|",
    ]
    for status in ("ok", "warning", "error", "timeout", "crash"):
        out.append(f"| {status} | {len(by_status.get(status, []))} |")
    for status, title in (
        ("crash", "Crashed"),
        ("timeout", "Timed out"),
        ("error", "Failed with errors (candidates for Unsupported style)"),
        ("warning", "Rendered with warnings"),
    ):
        rows = by_status.get(status, [])
        if not rows:
            continue
        out += ["", f"## {title} ({len(rows)})", "", "| Style | First messages |", "|---|---|"]
        out += [f"| {style} | {first_lines(r.output)} |" for style, r in rows]
    path.write_text("\n".join(out) + "\n", encoding="utf-8")


if __name__ == "__main__":
    sys.exit(main())
