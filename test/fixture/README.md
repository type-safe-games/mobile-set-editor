# Test fixture

A tiny Game, Style and symbol font that public CI renders instead of the private Full Magic Pack
(ADR 0008, ADR 0010). It is test-only: it never ships in the app and is not a starter template.

## What it exercises

`sets/fixture-cards.mse-set` has three cards that cover:

| Feature            | Where                                                                 |
|--------------------|-----------------------------------------------------------------------|
| Wrapping text      | `rules` on *Long Wrapping Text*: two paragraphs, the second in `<i>`  |
| Symbol-font insert | `<sym>R</sym>` / `<sym>U</sym>` in `rules`, from `fixture-symbols`    |
| Choice field       | `frame` (red / blue / grey), drawn as `choice images`                 |
| Slider field       | `level`, 0 to 5                                                       |
| Image field        | `picture` (`image1.png` on the first card; empty on the others)       |
| Linked card pair   | *Front Face* ↔ *Back Face*, through the game's `Front Face`/`Back Face` card link |
| Card list script   | `number` (the card's position in the set; not drawn), for stale cards after card list changes |

## How the tests use it

`test/tests.cmake` sets `MSE_TEMPLATES_DIR` to `data/` and `FONTCONFIG_FILE` to a generated config
that lists only `fonts/`, so renders don't depend on the machine's installed fonts. Tests:
`fixture-render` (every card renders with no warnings) and `layout-dump`.

To run it by hand from the repo root, after a build:

```sh
export MSE_TEMPLATES_DIR=$PWD/test/fixture/data FONTCONFIG_FILE=$PWD/build/fixture-fonts.conf
echo 'write_image_file(set.cards[0], file:"card.png")' | ./build/magicseteditor --cli -q test/fixture/sets/fixture-cards.mse-set
```

Note: a game must declare any `set info` key that a set uses. An unknown key in `set info`
makes MSE 2.6's reader loop forever instead of warning, so the game declares a `title` set field.

## Licenses

- `fonts/DejaVuSans*.ttf`: DejaVu fonts 2.37, under the Bitstream Vera / Arev fonts license
  (a free, permissive font license), reproduced in `fonts/LICENSE-DejaVu.txt`.
- Everything else under `test/fixture/` (the game, style and symbol-font files, the generated
  PNG images, the set, the command files and this README) was written for this project and is
  dedicated to the public domain under
  [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/).
