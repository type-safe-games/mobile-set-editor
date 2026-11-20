#!/usr/bin/env python3
"""Write the Tier 2 golden Sets: hand-authored cards for about 20 core magic Styles (ADR 0010).

Tier 1 fills every Style with the same generic card. Tier 2 checks the core Styles in depth: long
text that reflows, inline mana and tap symbols, keywords with reminder text, Linked cards for
faces, and slider styling fields where a Style has them. The card content below is authored for
this project; the Sets it writes use the private pack, so they go beside it in
templates/goldens/tier2 (ADR 0008), with an index.md of what each Set exercises.

After writing, each Set is loaded and every card rendered through `magicseteditor --cli`.
"""

from __future__ import annotations

import argparse
import sys
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import DEFAULT_GOLDENS, DEFAULT_MSE, DEFAULT_PACK, mse_env, mse_string, run_cli  # noqa: E402

T = "<sym>T</sym>"
LONG_RULES = (
    "Flying\n"
    "Ward <sym>2</sym>\n"
    f"{T}, Pay <sym>1</sym><sym>U</sym>: Draw two cards, then discard a card unless you control an "
    "artifact. Activate only as a sorcery and only if you have no cards in hand.\n"
    "Whenever another creature you control dies, put a +1/+1 counter on this creature. If that "
    "creature was legendary, put two +1/+1 counters on it instead."
)


@dataclass
class Card:
    fields: dict[str, str]
    uid: str | None = None
    stylesheet: str | None = None  # per-card Style, when it differs from the Set's
    styling: dict[str, str] = field(default_factory=dict)  # per-card styling options (sliders...)
    links: list[tuple[str, str]] = field(default_factory=list)  # (other uid, relation of the other card)


@dataclass
class GoldenSet:
    name: str
    stylesheet: str
    exercises: str
    cards: list[Card]


def creature(name: str, cost: str, sub: str, rules: str, p: str, t: str, **extra: str) -> dict[str, str]:
    d = {
        "name": name,
        "casting cost": cost,
        "super type": "<word-list-type>Creature</word-list-type>",
        "sub type": f"<word-list-race>{sub}</word-list-race>",
        "rule text": rules,
        "power": p,
        "toughness": t,
        "rarity": "rare",
    }
    d.update(extra)
    return d


def spell(name: str, cost: str, kind: str, rules: str, **extra: str) -> dict[str, str]:
    d = {
        "name": name,
        "casting cost": cost,
        "super type": f"<word-list-type>{kind}</word-list-type>",
        "rule text": rules,
        "rarity": "uncommon",
    }
    d.update(extra)
    return d


FLAVOR = "<i-flavor>The longer the text, the more ways a renderer can disagree about where it breaks.</i-flavor>"

SETS: list[GoldenSet] = [
    GoldenSet("m15-normal", "m15", "creature with long reflowing rules, keywords with reminder text, inline mana/tap symbols; instant; artifact; land", [
        Card(creature("Archivist of Long Sentences", "2UU", "Human Wizard", LONG_RULES, "2", "3", **{"flavor text": FLAVOR})),
        Card(spell("Crackling Overload", "XRR", "Instant",
                   "Crackling Overload deals X damage to any target. If X is 5 or more, it also deals 2 damage to each "
                   "opponent and you may cast it again for <sym>R</sym><sym>R</sym> from your graveyard. Then exile it.\n"
                   "Cycling <sym>2</sym>")),
        Card(spell("Clockwork Abacus", "3", "Artifact",
                   f"{T}: Add one mana of any color.\n<sym>2</sym>, {T}, Sacrifice Clockwork Abacus: Draw a card.",
                   **{"flavor text": "<i-flavor>It counts what you can't.</i-flavor>"})),
        Card({"name": "Murmuring Estuary", "super type": "<word-list-type>Land</word-list-type>",
              "rule text": f"Murmuring Estuary enters tapped.\n{T}: Add <sym>G</sym> or <sym>U</sym>.", "rarity": "common"}),
    ]),
    GoldenSet("m15-transform-dfc", "m15", "transforming double-faced card as two Linked cards (Front Face / Back Face)", [
        Card(creature("Moonlit Apprentice", "1G", "Human Werewolf",
                      "At the beginning of each upkeep, if no spells were cast last turn, transform Moonlit Apprentice.", "2", "2"),
             uid="dfc-front", links=[("dfc-back", "Back Face")]),
        Card(creature("Howling Archivist", "", "Werewolf",
                      "Trample\nAt the beginning of each upkeep, if a player cast two or more spells last turn, "
                      "transform Howling Archivist.", "4", "4", **{"rarity": "rare"}),
             uid="dfc-back", links=[("dfc-front", "Front Face")]),
    ]),
    GoldenSet("m15-saga", "m15-saga", "saga chapters (I–III) with reminder text, slider styling (textbox opacity)", [
        Card({"name": "The Long Retelling", "casting cost": "2W",
              "super type": "<word-list-type>Enchantment</word-list-type>", "sub type": "<word-list-enchantment>Saga</word-list-enchantment>",
              "level 1 text": "Create a 1/1 white Spirit creature token with flying.",
              "level 2 text": "Creatures you control get +1/+0 until end of turn. Draw a card if you control three or more creatures.",
              "level 3 text": "Exile target nonland permanent an opponent controls with mana value 3 or less.",
              "rarity": "rare"},
             styling={"textbox opacity percentage": "60"}),
    ]),
    GoldenSet("m15-planeswalker", "m15-planeswalker", "planeswalker with three loyalty abilities (+, −, ultimate) and long ability text", [
        Card({"name": "Vessa, Keeper of Margins", "casting cost": "2UB",
              "super type": "<word-list-type>Legendary Planeswalker</word-list-type>", "sub type": "<word-list-planeswalker>Vessa</word-list-planeswalker>",
              "rule text": "Draw a card, then discard a card.\n"
                           "Return target creature card with mana value 3 or less from your graveyard to your hand.\n"
                           "You get an emblem with \"Whenever you draw a card, each opponent loses 1 life and you gain 1 life.\"",
              "loyalty": "4", "loyalty cost 1": "+1", "loyalty cost 2": "−2", "loyalty cost 3": "−8", "rarity": "mythic rare"}),
    ]),
    GoldenSet("m15-adventure", "m15-adventure", "creature with an Adventure spell (second name/cost/type/text on one card)", [
        Card(creature("Bramble Courier", "2G", "Elf Scout", "Reach\nWhenever Bramble Courier blocks, untap it.", "3", "3",
                      **{"name 2": "Shortcut Through Thorns", "casting cost 2": "G",
                         "super type 2": "<word-list-type>Sorcery</word-list-type>",
                         "sub type 2": "<word-list-spell>Adventure</word-list-spell>",
                         "rule text 2": "Search your library for a basic land card, put it onto the battlefield tapped, then shuffle."})),
    ]),
    GoldenSet("m15-flip", "m15-flip", "flip card (upside-down second half)", [
        Card(creature("Novice of the Ink", "1U", "Human Monk",
                      "Whenever you cast an instant or sorcery spell, put a ki counter on Novice of the Ink. "
                      "At the beginning of the end step, if there are three or more ki counters on it, flip it.", "1", "2",
                      **{"name 2": "Master of the Brush", "super type 2": "<word-list-type>Legendary Creature</word-list-type>",
                         "sub type 2": "<word-list-race>Human Monk</word-list-race>",
                         "rule text 2": "Instant and sorcery spells you cast cost <sym>1</sym> less to cast.",
                         "power 2": "3", "toughness 2": "4"})),
    ]),
    GoldenSet("m15-split", "m15-split-fusable", "split card with Fuse, slider styling", [
        Card({"name": "Ink", "casting cost": "1U", "super type": "<word-list-type>Instant</word-list-type>",
              "rule text": "Counter target spell unless its controller pays <sym>2</sym>.",
              "name 2": "Ash", "casting cost 2": "2R", "super type 2": "<word-list-type>Instant</word-list-type>",
              "rule text 2": "Ash deals 3 damage to any target.",
              "rule text 3": "Fuse (You may cast one or both halves of this card from your hand.)",
              "rarity": "uncommon"},
             styling={"textbox opacity percentage": "70"} ),
    ]),
    GoldenSet("m15-aftermath", "m15-aftermath", "aftermath card (rotated second half)", [
        Card({"name": "Gather", "casting cost": "1G", "super type": "<word-list-type>Sorcery</word-list-type>",
              "rule text": "Look at the top four cards of your library. Put a land card from among them into your hand and the rest on the bottom.",
              "name 2": "Scatter", "casting cost 2": "3B", "super type 2": "<word-list-type>Sorcery</word-list-type>",
              "rule text 2": "Aftermath (Cast this spell only from your graveyard. Then exile it.)\nEach opponent sacrifices a creature.",
              "rarity": "uncommon"}),
    ]),
    GoldenSet("m15-token", "m15-token", "creature token and a noncreature token with rules text", [
        Card({"name": "Spirit", "super type": "<word-list-type>Token Creature</word-list-type>",
              "sub type": "<word-list-race>Spirit</word-list-race>", "rule text": "Flying", "power": "1", "toughness": "1"}),
        Card({"name": "Clue", "super type": "<word-list-type>Token Artifact</word-list-type>",
              "sub type": "<word-list-artifact>Clue</word-list-artifact>",
              "rule text": f"<sym>2</sym>, Sacrifice this artifact: Draw a card."}),
    ]),
    GoldenSet("m15-leveler", "m15-leveler", "level up creature with two level bands", [
        Card(creature("Apprentice Cartographer", "1U", "Human Wizard", "Level up <sym>1</sym><sym>U</sym>", "1", "1",
                      **{"level 1": "1-3", "level 2": "4+",
                         "level 2 text": "Flying", "level 3 text": "Flying\nWhenever this creature attacks, scry 2.",
                         "power 2": "2", "toughness 2": "2", "power 3": "4", "toughness 3": "4"})),
    ]),
    GoldenSet("class", "genevensis-11-class", "class enchantment with level costs and per-level text", [
        Card({"name": "Lorekeeper Class", "casting cost": "1U",
              "super type": "<word-list-type>Enchantment</word-list-type>", "sub type": "<word-list-enchantment>Class</word-list-enchantment>",
              "level 1 text": "Whenever you draw your second card each turn, scry 1.",
              "level 2 text": "<sym>1</sym><sym>U</sym>: Level 2",
              "level 3 text": "At the beginning of your upkeep, look at the top two cards of your library. Put one into your hand and the other into your graveyard.",
              "level 4 text": "<sym>3</sym><sym>U</sym>: Level 3",
              "level 5 text": "You have no maximum hand size. Whenever you draw a card, each opponent mills a card.",
              "rarity": "rare"}),
    ]),
    GoldenSet("m15-battle", "m15-mainframe-battles", "battle (defense) Linked to its back face, slider styling", [
        Card({"name": "Siege of the Silent Library", "casting cost": "2U",
              "super type": "<word-list-type>Battle</word-list-type>", "sub type": "<word-list-battle>Siege</word-list-battle>",
              "rule text": "(As a Siege enters, choose an opponent to protect it. You and others can attack it. When it's defeated, exile it, then cast it transformed.)\n"
                           "When Siege of the Silent Library enters, draw two cards.",
              "loyalty": "5", "rarity": "rare"},
             uid="battle-front", links=[("battle-back", "Back Face")], styling={"textbox opacity": "80"}),
        Card(spell("Open Shelves", "", "Enchantment", "At the beginning of your end step, draw a card."),
             uid="battle-back", stylesheet="m15", links=[("battle-front", "Front Face")]),
    ]),
    GoldenSet("m15-meld", "m15-meld-3in1", "meld pair plus melded result on one three-part card", [
        Card(creature("Twin Archivist", "2W", "Human Cleric", "When Twin Archivist enters, you gain 3 life.", "2", "3",
                      **{"name 2": "Twin Scribe", "casting cost 2": "2W",
                         "super type 2": "<word-list-type>Creature</word-list-type>", "sub type 2": "<word-list-race>Human Cleric</word-list-race>",
                         "rule text 2": "At the beginning of combat on your turn, if you control both Twin Archivist and Twin Scribe, meld them.",
                         "power 2": "3", "toughness 2": "2",
                         "name 3": "The Bound Volume", "super type 3": "<word-list-type>Legendary Creature</word-list-type>",
                         "sub type 3": "<word-list-race>Avatar</word-list-race>",
                         "rule text 3": "Flying, lifelink\nWhenever The Bound Volume attacks, draw a card for each card in your hand.",
                         "power 3": "7", "toughness 3": "7"})),
    ]),
    GoldenSet("m15-textless", "m15-textless", "textless full-art creature", [
        Card(creature("Silent Colossus", "6", "Golem", "", "8", "8", **{"super type": "<word-list-type>Artifact Creature</word-list-type>"})),
    ]),
    GoldenSet("m15-mainframe-planeswalker", "m15-mainframe-planeswalker", "mainframe planeswalker with loyalty abilities", [
        Card({"name": "Oren, Margin Walker", "casting cost": "3R",
              "super type": "<word-list-type>Legendary Planeswalker</word-list-type>", "sub type": "<word-list-planeswalker>Oren</word-list-planeswalker>",
              "rule text": "Oren deals 2 damage to any target.\nExile the top two cards of your library. You may play them this turn.\n"
                           "Oren deals 10 damage to each opponent and each creature they control.",
              "loyalty": "5", "loyalty cost 1": "+1", "loyalty cost 2": "−2", "loyalty cost 3": "−9", "rarity": "mythic rare"}),
    ]),
    GoldenSet("m15-mainframe-tokens", "m15-mainframe-tokens", "mainframe token with reminder-style rules", [
        Card({"name": "Treasure", "super type": "<word-list-type>Token Artifact</word-list-type>",
              "sub type": "<word-list-artifact>Treasure</word-list-artifact>",
              "rule text": f"{T}, Sacrifice this artifact: Add one mana of any color."}),
    ]),
    GoldenSet("new", "new", "pre-M15 modern frame: creature, sorcery", [
        Card(creature("Archivist of Long Sentences", "2UU", "Human Wizard", LONG_RULES, "2", "3", **{"flavor text": FLAVOR})),
        Card(spell("Rummage", "1R", "Sorcery", "Discard a card, then draw two cards.\nFlashback <sym>3</sym><sym>R</sym>")),
    ]),
    GoldenSet("old", "old", "classic frame: creature, enchantment", [
        Card(creature("Archivist of Long Sentences", "2UU", "Human Wizard", LONG_RULES, "2", "3", **{"flavor text": FLAVOR})),
        Card(spell("Quiet Library", "1W", "Enchantment", "Players can't draw more than one card each turn.")),
    ]),
    GoldenSet("m15-showcase-kaldheim", "m15-showcase-kaldheim", "heavily used showcase frame with long text", [
        Card(creature("Archivist of Long Sentences", "2UU", "Human Wizard", LONG_RULES, "2", "3")),
    ]),
    GoldenSet("m15-full-art-basic", "m15-full-art-basic-land-symbol", "full-art basic land", [
        Card({"name": "Island", "super type": "<word-list-type>Basic Land</word-list-type>",
              "sub type": "<word-list-land>Island</word-list-land>", "rarity": "basic land"}),
    ]),
]


def value_lines(key: str, value: str, indent: str) -> list[str]:
    if "\n" in value:
        return [f"{indent}{key}:"] + [f"{indent}\t{line}" for line in value.split("\n")]
    return [f"{indent}{key}: {value}"]


def set_file(gs: GoldenSet) -> str:
    lines = ["mse version: 2.6.0", "game: magic", f"stylesheet: {gs.stylesheet}"]
    for card in gs.cards:
        lines.append("card:")
        if card.stylesheet:
            lines.append(f"\tstylesheet: {card.stylesheet}")
        if card.styling:
            lines.append("\thas styling: true")
            lines.append("\tstyling data:")
            for k, v in card.styling.items():
                lines += value_lines(k, v, "\t\t")
        if card.uid:
            lines.append(f"\tuid: {card.uid}")
        for i, (other, relation) in enumerate(card.links, 1):
            lines.append(f"\tlinked_card_{i}: {other}")
            lines.append(f"\tlinked_relation_{i}: {relation}")
        for k, v in card.fields.items():
            if v != "":
                lines += value_lines(k, v, "\t")
    return "\n".join(lines) + "\n"


def write_index(path: Path, results: dict[str, str]) -> None:
    out = ["# Tier 2 golden Sets", "", "Hand-authored Sets for core magic Styles (ADR 0010); "
           "written by `tools/goldens/tier2_sets.py`.", "",
           "| Set | Style | Cards | Exercises | Check |", "|---|---|---|---|---|"]
    for gs in SETS:
        out.append(f"| {gs.name} | magic-{gs.stylesheet} | {len(gs.cards)} | {gs.exercises} | {results.get(gs.name, 'not run')} |")
    path.write_text("\n".join(out) + "\n", encoding="utf-8")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--pack", type=Path, default=DEFAULT_PACK)
    ap.add_argument("--out", type=Path, default=DEFAULT_GOLDENS / "tier2")
    ap.add_argument("--mse", type=Path, default=DEFAULT_MSE)
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("--timeout", type=float, default=180)
    ap.add_argument("--no-check", action="store_true")
    args = ap.parse_args()

    sets_dir = args.out / "sets"
    for gs in SETS:
        p = sets_dir / f"{gs.name}.mse-set"
        p.mkdir(parents=True, exist_ok=True)
        (p / "set").write_text(set_file(gs), encoding="utf-8")
    print(f"wrote {len(SETS)} Sets under {sets_dir}")
    results: dict[str, str] = {}
    if not args.no_check:
        env = mse_env(args.pack, args.out)
        check_dir = args.out / "check"
        check_dir.mkdir(parents=True, exist_ok=True)

        def check(gs: GoldenSet):
            prefix = str(check_dir / gs.name)
            commands = f"for each c in set.cards do write_image_file(c, file: {mse_string(prefix)} + \"-\" + position(of: c, in: set.cards) + \".png\")\n"
            return gs, run_cli(args.mse, sets_dir / f"{gs.name}.mse-set", commands, env, args.timeout)

        with ThreadPoolExecutor(max_workers=args.jobs) as pool:
            for gs, r in pool.map(check, SETS):
                results[gs.name] = r.status
                msgs = [l for l in r.output.splitlines() if "ERROR" in l or "WARNING" in l][:3]
                print(f"{r.status:8} {gs.name} {' / '.join(msgs)}")
    write_index(args.out / "index.md", results)
    bad = [n for n, s in results.items() if s != "ok"]
    print(f"{len(bad)} Sets not ok: {', '.join(bad)}" if bad else "all Sets ok")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
