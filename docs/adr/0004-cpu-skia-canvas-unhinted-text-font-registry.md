# Card rendering uses a CPU-only Skia `Canvas` with unhinted text and an MSE font registry

The Android `Canvas` (ADR 0003) is Skia, built from source CPU-only (no GPU, PDF or SVG), rendering off-screen into a buffer matching MSE's RGB + alpha `Image`; the UI only ever receives finished card bitmaps, so the backend is independent of the UI stack. Skia's `SkCodec` decodes PNG and JPEG. We deliberately bypass most of what makes Skia attractive elsewhere:

- **Text stays MSE's.** The `Canvas` text primitive only rasterises aliased glyphs into a coverage buffer at a requested scale; MSE's own 4x downsample, blur and repeated-blit bold (`gfx/resample_text.cpp`) stay, so stroke weight and bold look match desktop.
- **Font names never reach the backend.** An MSE-owned font registry resolves family + bold/italic (case-insensitive, with fallback) to font files from installed Templates and from the shared font store of loose fonts collected at import (ADR 0005: the core Magic fonts ship in no package); the `Canvas` takes resolved faces. The desktop reference uses the same registry, so both sides render the same files.
- **Text is unhinted with linear advances**, for measurement and rasterisation alike, as part of the `Canvas` contract. The wx golden-image harness pins hinting off with a checked-in fontconfig file.
- **Bitwise raster ops** are not a Skia capability; they always use the CPU fallback over read-back, with Windows semantics.

The fidelity bar is: layout (line breaks, shrink-to-fit size, overflow) identical to the Linux wx-backed `Canvas` on the golden corpus; pixels within a perceptual tolerance; Windows/Wine is a spot check, never a gate. MSE measures text itself, so layout parity reduces to equal glyph advances, which is why font files and hinting are pinned rather than left to each backend.

Cairo+Pango (the reference's own stack) was the runner-up and remains the fallback if advances can't be made to match; it lost on a long glib/fontconfig chain with no Android build or F-Droid precedent, and PNG-only decoding. Blend2D has no aliased fill and an unverified arm64 JIT under Android's sandbox; Android Canvas forces JNI per call and has no headless desktop tests; Qt is large, has no F-Droid recipe and is LGPL-3.

## Consequences

- Changing the hinting rule or the font registry's resolution invalidates every golden image.
- The F-Droid recipe must build `gn` (or Skia via our own build files, as OsmAnd does) from source rather than letting `git-sync-deps` download it.
- Native Skia anti-aliased text is a possible later optimisation, accepted only if golden images still pass.

## Amendment (2026-11-20): the registry resolves the way the reference does

Building the registry (#32) showed that the fontconfig-based desktop reference matches **family names only**. Styles that name a full name such as "Arsenal Regular" or "MyriadPro-Regular" render in the fallback family, Noto Sans, in the golden images. We chose to keep the goldens as they are and make the registry copy that behaviour, rather than resolve the intended faces and re-capture 18 Tier 1 Sets:

- Families match case-insensitively on family and typographic family names, never on the full name.
- An unknown family resolves to the fallback family, Noto Sans. Android has no system fonts, so the app must ship Noto Sans (OFL) in its font store. On desktop the wx `Canvas` still selects by name through fontconfig.
- Known remaining gaps against fontconfig: tie-breaking between faces of equal weight (seen once, ITC Souvenir Medium vs Demi), system fonts the pack doesn't ship (DejaVu, Verdana, Minion Pro), and Type 1 `.pfb` fonts. These are checked when the Skia `Canvas` is compared with the goldens (#42).

## Amendment (2026-11-20): raster ops copy the Linux reference, not Windows

The golden images come from wxGTK, which draws through cairo. There, `AND`, `OR` and `AND_INVERT` draw and blit nothing, and `XOR` is cairo's XOR compositing, not a bitwise op. This affects symbol SUBTRACT, INTERSECTION and DIFFERENCE. The golden images are the fidelity bar, so the Skia `Canvas` copies the reference's behaviour for each raster op, not Windows semantics. The wx `Canvas` declares only `COPY` as supported on GTK (#32). The core already branches on that capability, so on Android the core takes the same code paths as on the Linux reference. The Skia `Canvas` copies wxGTK's results for any op it still receives. Windows/Wine remains a spot check only.
