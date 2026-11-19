---
status: accepted
---

# Two-tier golden corpus, checked by layout dump, with per-Style support status

ADR 0004 requires the port to produce **the same layout** as the Linux wx reference build, while pixels only have to look the same. The Full Magic Pack has 366 `magic` Styles and no sample Sets. We test **every** Style with generated cards, and check a smaller set of core Styles in depth. The pass/fail check compares the core's layout, not pixels.

## Decisions

- **Tier 1, broad.** A script generates one fixture Set per Style, with every card field filled with standard text, and renders one Golden image per Style.
- **Tier 2, deep.** Hand-authored Sets for about 20 core Styles: m15 normal, saga, planeswalker, adventure, flip, DFC/transform, split, token, leveler, class and battle. They exercise long and reflowing text, symbols, Linked cards and slider fields.
- **Layout dump is the gate.** The core prints its layout: each field's region, each text line's break position, origin and width. The dump must match the reference exactly, so it doesn't depend on which renderer drew the card.
- **Pixel diff is advisory.** An SSIM-style threshold flags rendering problems that leave layout unchanged, such as a wrong combine mode or a missing mask. It never blocks a change.
- **Style support status replaces a named fallback list.** A Style that fails Tier 1 is marked *unsupported in this version*. Template import still installs it, and the style picker flags it. The number of supported Styles grows as fixes land.
- **Where the goldens run.** At first, through a local script run before merging. Once the wx-removal steps start, a Gitea Actions runner on the home server runs the same script. Public CI only uses the Test fixture (ADR 0008).

## Considered Options

- **Hand-picked Styles only**: most of the 366 Styles could break without anyone noticing.
- **Pixel diff as the gate**: anti-aliasing differs between wx and Skia, so the threshold either passes real layout drift or fails on harmless pixel differences.
- **A core Style list chosen in advance as the fallback**: a judgement call instead of a measurement.

## Consequences

- The core needs a layout-dump hook that the `--cli` mode can call. That is a milestone-1 build item, needed before the Golden images are frozen from the 2.6 merge+shim commit (ADR 0007).
