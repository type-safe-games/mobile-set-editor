# Mobile Set Editor

An Android port of Magic Set Editor: designing custom trading-card sets on a phone, rendered with MSE's existing templates.

## Language

**Game**:
A template defining a card game's fields and rules (e.g. `magic`), packaged as a `.mse-game` directory.
_Avoid_: Ruleset

**Style**:
A template defining how a Game's cards look (frame art, layout, fonts), packaged as a `.mse-style` directory. A card renders with exactly one Style.
_Avoid_: Stylesheet, frame, skin

**Template**:
Any installable MSE package a Set depends on: a Game, a Style, a symbol font, an export template, etc.
_Avoid_: Plugin, add-on

**Template pack**:
A distributable bundle of Templates installed together, e.g. the Full Magic Pack.
_Avoid_: Template bundle

**Template import**:
The only way Templates reach the device: the user picks a zip they obtained themselves, and the app installs the Templates and loose fonts found inside it. The app never bundles or downloads Templates.
_Avoid_: Download, install from store

**Set**:
A user's collection of Cards for one Game, saved as an `.mse-set` file.
_Avoid_: Project, deck

**Card**:
One entry in a Set: values for the Game's card fields, rendered through a Style.

**Working copy**:
The app's private, continuously saved copy of an open Set, which edits go to; the user's `.mse-set` file changes only when they save or export to it.
_Avoid_: Draft, cache, autosave file

**Linked card**:
A Card joined to another Card in the same Set, such as the two faces of a double-faced card; editing either keeps the link.
_Avoid_: Other side, back card

**Supported MSE version**:
The newest `mse version` the app accepts from a Template; Templates declaring a newer one are refused at Template import.
_Avoid_: App version

**Set symbol**:
The expansion symbol shown on a Set's Cards, edited as an `.mse-symbol`.
_Avoid_: Expansion symbol, icon

### Rendering

**Golden image**:
A reference rendering of a Card, produced by the desktop build, that the port's rendering of the same Card must match in layout.
_Avoid_: Snapshot, baseline

**Test fixture**:
A tiny self-authored, freely licensed Game and Style used only to test rendering in public; never shipped to users, and not a starter template.
_Avoid_: Starter style, sample template

**Font registry**:
The app's own catalogue of the fonts shipped inside installed Templates and in the Shared font store, which decides which font file a Style's font name means.
_Avoid_: Font manager, system fonts

**Shared font store**:
The fonts a Template import found outside any Template (e.g. the core Magic fonts a pack ships loose), kept so every Style can use them.
_Avoid_: System fonts

**Golden corpus**:
The Cards whose Golden images the port is checked against: one generated Card per Style (broad tier) plus hand-authored Sets for core Styles (deep tier).
_Avoid_: Test set, reference set

**Layout dump**:
The core's printed account of a rendered Card's layout (field regions, text line breaks and positions), compared exactly against a Golden image's.
_Avoid_: Layout snapshot

**Unsupported style**:
A Style that fails its Golden image check in this version of the app; it still installs, but is flagged in the style picker.
_Avoid_: Broken style, disabled style

**Template bug**:
An error in a Template's own scripts or images that the reference desktop editor shows too; recorded with its upstream report, and not counted against the Style's support.
_Avoid_: Upstream bug, pack bug

