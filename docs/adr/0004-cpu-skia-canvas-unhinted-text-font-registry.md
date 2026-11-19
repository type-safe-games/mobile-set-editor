# Card rendering uses a CPU-only Skia `Canvas` with unhinted text and an MSE font registry

The Android `Canvas` (ADR 0003) is Skia, built from source CPU-only (no GPU, PDF or SVG), rendering off-screen into a buffer matching MSE's RGB + alpha `Image`; the UI only ever receives finished card bitmaps, so the backend is independent of the UI stack. Skia's `SkCodec` decodes PNG and JPEG. We deliberately bypass most of what makes Skia attractive elsewhere:

- **Text stays MSE's.** The `Canvas` text primitive only rasterises aliased glyphs into a coverage buffer at a requested scale; MSE's own 4x downsample, blur and repeated-blit bold (`gfx/resample_text.cpp`) stay, so stroke weight and bold look match desktop.
- **Font names never reach the backend.** An MSE-owned font registry resolves family + bold/italic (case-insensitive, with fallback) to font files from installed Templates; the `Canvas` takes resolved faces. The desktop reference uses the same registry, so both sides render the same files.
- **Text is unhinted with linear advances**, for measurement and rasterisation alike, as part of the `Canvas` contract. The wx golden-image harness pins hinting off with a checked-in fontconfig file.
- **Bitwise raster ops** are not a Skia capability; they always use the CPU fallback over read-back, with Windows semantics.

The fidelity bar is: layout (line breaks, shrink-to-fit size, overflow) identical to the Linux wx-backed `Canvas` on the golden corpus; pixels within a perceptual tolerance; Windows/Wine is a spot check, never a gate. MSE measures text itself, so layout parity reduces to equal glyph advances, which is why font files and hinting are pinned rather than left to each backend.

Cairo+Pango (the reference's own stack) was the runner-up and remains the fallback if advances can't be made to match; it lost on a long glib/fontconfig chain with no Android build or F-Droid precedent, and PNG-only decoding. Blend2D has no aliased fill and an unverified arm64 JIT under Android's sandbox; Android Canvas forces JNI per call and has no headless desktop tests; Qt is large, has no F-Droid recipe and is LGPL-3.

## Consequences

- Changing the hinting rule or the font registry's resolution invalidates every golden image.
- The F-Droid recipe must build `gn` (or Skia via our own build files, as OsmAnd does) from source rather than letting `git-sync-deps` download it.
- Native Skia anti-aliased text is a possible later optimisation, accepted only if golden images still pass.
