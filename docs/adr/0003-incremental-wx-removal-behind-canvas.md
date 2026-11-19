# wx is removed incrementally, behind a `Canvas` seam

wxWidgets is removed from the core in place, one `prec.hpp` alias at a time, with the desktop Linux build kept compiling and tested at every step (as a headless test host, not a product). The order is: GUI leakage, type-only swaps, zip and DateTime, `String`, `Image`, and finally `DC`/text. The only new abstraction is an MSE-owned `Canvas` interface under `RotatedDC` (fills, aliased polygons, blit, read-back, bitwise raster ops as a declared capability, rotated text, and text measurement with file-loaded fonts), implemented first over wx and then by the Android rendering backend.

A clean extraction into a new wx-free library was rejected because nothing would run for a long stretch, and the `.mse-set` round-trip promise (ADR 0001) needs a working build to check against. Abstracting every wx service up front was rejected because zip, dates and streams only ever get one implementation; the drawing seam is the one place where two really exist.

## Consequences

- Golden images from the wx-backed `Canvas` are the fidelity reference for the Android backend.
- The rendering-backend choice is narrowed to "what implements `Canvas`" (plus image codecs).
