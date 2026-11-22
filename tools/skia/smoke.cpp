// Smoke test for the vendored Skia build (#40); not part of MSE.
// Loads a font from a file path through the custom-empty font manager, draws a
// filled polygon and text into a CPU raster surface, and writes a PNG.
//
//   smoke <font.ttf> <out.png>
#include "include/core/SkCanvas.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkStream.h"
#include "include/core/SkSurface.h"
#include "include/core/SkTypeface.h"
#include "include/encode/SkPngEncoder.h"
#include "include/ports/SkFontMgr_empty.h"

#include <cstdio>

int main(int argc, char** argv) {
  if (argc != 3) {
    std::fprintf(stderr, "usage: %s <font.ttf> <out.png>\n", argv[0]);
    return 2;
  }
  sk_sp<SkFontMgr> mgr = SkFontMgr_New_Custom_Empty();
  sk_sp<SkTypeface> face = mgr->makeFromFile(argv[1]);
  if (!face) {
    std::fprintf(stderr, "could not load font %s\n", argv[1]);
    return 1;
  }

  sk_sp<SkSurface> surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(400, 200));
  SkCanvas* canvas = surface->getCanvas();
  canvas->clear(SK_ColorWHITE);

  SkPaint fill;
  fill.setColor(SkColorSetRGB(0x20, 0x60, 0xc0));
  SkPathBuilder star;
  star.moveTo(60, 20).lineTo(80, 80).lineTo(20, 40).lineTo(100, 40).lineTo(40, 80).close();
  canvas->drawPath(star.detach(), fill);

  // Aliased, unhinted glyphs, as the Canvas contract asks (ADR 0004).
  SkFont font(face, 40);
  font.setEdging(SkFont::Edging::kAlias);
  font.setHinting(SkFontHinting::kNone);
  font.setLinearMetrics(true);
  SkPaint ink;
  ink.setColor(SK_ColorBLACK);
  canvas->drawString("Skia smoke", 120, 70, font, ink);
  canvas->drawString("Hamburgefonstiv", 20, 160, font, ink);

  SkPixmap pixels;
  if (!surface->peekPixels(&pixels)) return 1;
  SkFILEWStream out(argv[2]);
  if (!out.isValid() || !SkPngEncoder::Encode(&out, pixels, {})) {
    std::fprintf(stderr, "could not write %s\n", argv[2]);
    return 1;
  }
  return 0;
}
