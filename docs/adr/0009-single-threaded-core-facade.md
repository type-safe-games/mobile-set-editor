---
status: accepted
---

# Single-threaded core facade with a Kotlin-owned render cache

The C++ facade (ADR 0006) runs on **one core thread owned by Kotlin**, and every call except template import is made on it. The facade has no locks. Each open Set is a **session**, reached through an opaque handle. Cards cross the boundary by their 2.6 `uid`. Rendering a card in MSE 2.6 is not read-only: drawing runs `Style::update` scripts and writes layout into Style objects that every card of a stylesheet shares. Script contexts and package writes also assert they are on the main thread. So a parallel renderer would first need a card model that can be safely copied, which the core does not have.

## Decisions

- **Edits.** Every field call names a target: a card, the set, a card's style options or the set's style options. Text fields use `replaceRange` in *visible-text* positions, with an `asSymbol` flag. That call records the core's own `typing_action`, so consecutive keystrokes still merge into one undo step. The **core** owns caret re-matching after scripts rewrite the value, selection fixing at `<sep>`, and auto-replace (its settings are passed in rather than read from the global `settings`). The call returns the visible text, the caret and selection, and the spans that can't be edited. All other value types are replaced whole. Undo and redo use the Set's `ActionStack`, and the dirty flag comes from its save point.
- **Rendering.** `renderCard` draws into an Android `Bitmap` that Kotlin allocated, at the scale its size implies, and returns field regions from the same layout pass: field id, rectangle, rotation, z-order and an editable flag. Kotlin hit-tests taps against them. Kotlin owns the render cache (an LRU around the current card). Every action reports the stale card `uid`s, collected from `ScriptValueEvent`s, or "all" after display, style or set-info changes. Pre-renders can be cancelled, and edits go ahead of them in the queue.
- **Faces** are ordinary Linked cards. No call takes a face parameter. `link` and `unlink` are each one undo step, using a `BulkAction` that no longer refreshes a GUI card list.
- **Lifecycle.** A session edits a private **working copy** in app storage with debounced autosave. `saveTo(fd)` writes the user's `.mse-set` through a file descriptor, and the `.bak` rotation goes. A missing template is a structured error, never a dialog.
- **Errors.** Each call returns the messages queued while it ran (severity, text, and card and field when known). No C++ exception crosses JNI, and fatal errors become a failed result.
- **Export.** `exportCardImage` produces the finished PNG in the core, with desktop-equivalent bleed, rotation and `<mse-card-data>` metadata.
- **The single exception is template import** (ADR 0005). It runs off the core thread, writes only to the template store, installs each package atomically, and waits while any session is loading.

## Considered Options

- **A facade guarded by one big lock**, callable from any thread: long renders silently block edits, and callbacks risk deadlock.
- **Worker-thread rendering of frozen card copies**: the fastest way to pre-render for swipes, but the core shares and mutates Style state during drawing.
- **Whole-value replace for text**: drops or duplicates tags that scripts generate (keyword reminders, `<soft>`, `<sep>`).
- **Desktop-style cursor and selection operations**: these need a laid-out `TextViewer` on every keystroke, and the tap-to-edit sheet doesn't need them.
- **A core-side render cache, or returning a fresh buffer each time**: both mean native memory Android cannot account for, or an extra copy per render.
- **A separate mask-accurate hit-test call**: a trip to the core thread on every tap, and no shapes to draw highlights from. It can be added later if a real template needs it.
- **Editing the user's file in place**: a URI write without a temp file is the riskiest moment for a zip, and Android kills apps without warning.

## Consequences

- More GUI code has to leave the core for the wx removal (ADR 0003):
  - `BulkAction`'s `CardListBase` refresh;
  - `MSE2FileFormat::importSet` touching recent files and windows;
  - `StyleSheet::byGameAndName` calling `select_stylesheet`;
  - `data/format/image.cpp` including `gui/util.hpp`;
  - `TextValueEditor`'s caret, selection and auto-replace logic moving into the core;
  - `InsertSymbolMenu` built as data instead of a `wxMenu`.
- The thumbnail, update-check and installer threads have no counterpart on Android. `DynamicArg`'s thread keying stays harmless.
- Parallel rendering is still open for later, but only after the card and Style state can be copied or made immutable.
