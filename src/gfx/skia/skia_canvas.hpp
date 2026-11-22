// Mobile Set Editor - the Canvas over Skia
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

/** @file gfx/skia/skia_canvas.hpp
 *  The CPU-only Skia Canvas (ADR 0004): offscreen canvases drawing into a Skia raster surface.
 *
 *  It adapts gfx/skia/skia_surface.hpp, which does the Skia and HarfBuzz calls in plain std
 *  types, to the core's Canvas. Fonts are the faces the font registry resolved; font names never
 *  reach Skia. Text measures exactly as Pango does for the desktop reference (see
 *  skia_surface.hpp), and is drawn with aliased glyphs. Raster operations copy wxGTK's results:
 *  only COPY is supported, XOR is XOR compositing and the others draw nothing (ADR 0004).
 *
 *  On the desktop this is the CMake option MSE_SKIA_CANVAS, for headless rendering only: the GUI
 *  still draws on wx canvases, which can't blit from a Skia one.
 */

#include <util/prec.hpp>
#include <gfx/canvas.hpp>

/// Makes Skia offscreen canvases
class SkiaCanvasFactory : public CanvasFactory {
public:
  OffscreenCanvasP create(int width, int height, CanvasFormat format) override;
  OffscreenCanvasP create(const Image& image) override;
};

/// Point size of the "system" font, which the Skia canvas resolves to the fallback family
/// (the GTK default, "Sans 10", in the desktop reference)
extern int skia_system_font_point_size;
