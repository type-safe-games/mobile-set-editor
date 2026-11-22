// Mobile Set Editor - drawing and text through Skia and HarfBuzz, in plain std types
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

/** @file gfx/skia/skia_surface.hpp
 *  A raster surface over Skia, and text shaped with HarfBuzz, behind an interface of std types.
 *
 *  Like skia_codec.hpp, the implementation is C++20 and sees no MSE headers; the Canvas over it is
 *  gfx/skia/skia_canvas.hpp.
 *
 *  Text measures exactly as Pango does for the desktop reference (wxGTK 3.2, Pango 1.5x, 96 dpi,
 *  hinted metrics), so that the Layout dump of the Golden images is reproduced (#42, ADR 0004):
 *   - the font is a point size at 96 dpi; HarfBuzz shapes at that size in 1/1024 pixel units,
 *     with its default features (kerning, ligatures, ...);
 *   - every glyph's advance and offset is rounded to a whole pixel (Pango's
 *     round_glyph_positions), and a width is the sum of the rounded advances;
 *   - a line is ceil(ascender) + ceil(descender) pixels high, from HarfBuzz's font extents
 *     (FreeType's hinted size metrics round the same way);
 *   - characters missing from the face come from a fallback face, in runs shaped separately.
 */

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace mse_skia {

struct RGBA { std::uint8_t r = 0, g = 0, b = 0, a = 255; };
struct Point { int x = 0, y = 0; };

// ----------------------------------------------------------------------------- : Fonts

/// A face of a font file, loaded once (thread safe)
class Typeface;
typedef std::shared_ptr<Typeface> TypefaceP;

/// The face at an index in a font file, shared by everyone who asks; null if it can't be loaded
TypefaceP load_typeface(const std::string& path, int index);

/// A typeface at a point size, as text is measured and drawn
struct TextFont {
  TypefaceP face;
  TypefaceP fallback;    ///< for characters the face doesn't have (Pango's font fallback), may be null
  int  point_size = 0;
  bool embolden = false; ///< synthetic bold (the face is lighter than asked for)
  bool skew = false;     ///< synthetic oblique (the face is upright where italic was asked for)
  bool underline = false;
  bool strikethrough = false;
};

/// Size of a line of text in whole pixels, as Pango measures it.
/** Characters the face doesn't have are shaped with the fallback face, and a line is as high as
 *  the highest ascent plus the deepest descent of the faces used. */
void text_extent(const TextFont& font, const std::string& utf8, int* width, int* height);
/// Height of a line of text in the face alone, in whole pixels
int line_height(const TextFont& font);

// ----------------------------------------------------------------------------- : Surface

enum class PenStyle { SOLID, DOT, NONE };

/// How drawing combines with what is there, as wxGTK's cairo does for each raster operation
enum class Composite {
  OVER,    ///< normal drawing; blits copy (wxCOPY)
  XOR,     ///< cairo's XOR operator (wxXOR on GTK)
  NOTHING, ///< nothing is drawn (wxAND, wxOR, wxAND_INVERT and wxINVERT on GTK)
};

/// A part of a clipping region
struct ClipPart {
  bool subtract = false;
  bool is_rect = true;
  int x = 0, y = 0, w = 0, h = 0;
  std::vector<Point> polygon; ///< when !is_rect, filled with the odd-even rule
};

/// A raster surface: opaque, or with an alpha channel (premultiplied inside, straight outside)
class Surface {
public:
  /// A new surface; with alpha it starts transparent, otherwise black
  Surface(int width, int height, bool alpha);
  ~Surface();
  Surface(const Surface&) = delete;
  Surface& operator = (const Surface&) = delete;

  int width() const;
  int height() const;
  bool hasAlpha() const;

  /// Replace all pixels with straight RGBA ones (width*height*4 bytes)
  void writePixels(const std::uint8_t* rgba);
  /// Read a rectangle as straight RGBA (w*h*4 bytes); outside the surface is transparent black
  void readPixels(int x, int y, int w, int h, std::uint8_t* rgba) const;

  // Paint state
  void setPen(RGBA color, int width, PenStyle style);
  void setBrush(RGBA color, bool none);
  void setComposite(Composite composite);

  // Shapes, with wx's conventions for rectangles and outlines
  void drawLine(int x1, int y1, int x2, int y2);
  void drawRectangle(int x, int y, int w, int h);
  void drawRoundedRectangle(int x, int y, int w, int h, double radius);
  void drawPolygon(const std::vector<Point>& points);
  void drawEllipse(int x, int y, int w, int h);
  void drawEllipticArc(int x, int y, int w, int h, double start_degrees, double end_degrees);

  /// Draw straight RGBA pixels with their top-left corner at (x,y), blended with their alpha
  void drawPixels(const std::uint8_t* rgba, int w, int h, int x, int y);
  /// Copy a rectangle of another surface here (OVER: replaces the pixels, as cairo's SOURCE)
  void blit(int x, int y, int w, int h, const Surface& source, int source_x, int source_y, Composite composite);

  void setClip(const std::vector<ClipPart>& parts);
  void clearClip();

  /// Draw a line of text with aliased glyphs, its top-left corner at (x,y), rotated counter
  /// clockwise around it; with subpixel > 1, (x,y) and the font size are scaled by 1/subpixel
  void drawText(const TextFont& font, RGBA color, const std::string& utf8, int x, int y, double angle_degrees, int subpixel);

private:
  struct Impl;
  std::unique_ptr<Impl> impl;
};

} // namespace mse_skia
