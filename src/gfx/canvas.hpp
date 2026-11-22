//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

/** @file gfx/canvas.hpp
 *  The drawing surface the core renders on (ADR 0003).
 *
 *  Everything the core draws goes through a Canvas: RotatedDC wraps one and adds rotation and
 *  zoom, the symbol renderer and the text rasteriser use one directly. The primitives are the
 *  ones MSE actually needs: fills and outlines of rectangles, polygons and ellipses with the
 *  current pen and brush, drawing an Image, reading pixels back into an Image, blits between
 *  canvases, clipping, bitwise raster operations as a declared capability, and text: measuring
 *  and drawing (possibly rotated) text in a font.
 *
 *  Text is drawn with aliased glyphs; MSE's own anti-aliasing (draw at 4x on an offscreen canvas,
 *  read back, downsample, blur and stroke: gfx/resample_text.cpp) stays in the core.
 *  Fonts are described by CanvasFont, which carries the face the font registry
 *  (gfx/font_registry.hpp) resolved for it.
 *
 *  Coordinates are device pixels. Geometry still uses wx's plain value types
 *  (wxPoint, wxSize, wxRect); replacing those is a separate, mechanical cleanup.
 *
 *  Implementations: gfx/wx_canvas.hpp (the desktop, over a wxDC) and gfx/skia/skia_canvas.hpp
 *  (CPU-only Skia, for Android; on the desktop with the CMake option MSE_SKIA_CANVAS).
 *  Offscreen canvases come from the installed CanvasFactory, like images come from the
 *  installed ImageCodec.
 */

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gfx/color.hpp>

DECLARE_SHARED_POINTER_TYPE(FontFace);

// ----------------------------------------------------------------------------- : Paint state

enum class PenStyle {
  SOLID,
  DOT,         ///< a dotted line, one pixel on, one off
  TRANSPARENT, ///< no outline
};

/// How outlines and lines are drawn
struct Pen {
  Color    color = Color(0,0,0);
  int      width = 1;
  PenStyle style = PenStyle::SOLID;

  Pen() {}
  Pen(Color color, int width = 1, PenStyle style = PenStyle::SOLID) : color(color), width(width), style(style) {}
  /// No outline
  static Pen none() { return Pen(Color(0,0,0), 1, PenStyle::TRANSPARENT); }
};

/// How shapes are filled
struct Brush {
  Color color = Color(0,0,0);
  bool  transparent = false; ///< no fill

  Brush() {}
  Brush(Color color) : color(color) {}
  /// No fill
  static Brush none() { Brush b; b.transparent = true; return b; }
};

/// Bitwise combination of what is drawn (source) with what is already there (destination)
enum class RasterOp {
  COPY,       ///< source
  AND,        ///< source & destination
  OR,         ///< source | destination
  XOR,        ///< source ^ destination
  AND_INVERT, ///< ~source & destination
  INVERT,     ///< ~destination
};

// ----------------------------------------------------------------------------- : Clipping regions

/// A region of device pixels: a rectangle or polygon, minus other rectangles or polygons
class CanvasRegion {
public:
  struct Part {
    bool            subtract = false; ///< remove this part from the region instead of adding it
    bool            is_rect  = true;
    wxRect          rect;
    vector<wxPoint> polygon;          ///< when !is_rect, filled with the odd-even rule
  };

  CanvasRegion(const wxRect& rect);
  CanvasRegion(vector<wxPoint> polygon);

  /// Remove another region from this one
  void subtract(const CanvasRegion& other);

  const vector<Part>& getParts() const { return parts; }

private:
  vector<Part> parts;
};

// ----------------------------------------------------------------------------- : Fonts

/// A font as the core asks a canvas to use it
/** Built by FontRef::toCanvasFont for Style fonts. The face is what the font registry resolved
 *  the family, weight and style to; a canvas may use it directly (the Skia canvas will), and must
 *  produce text with the same advances as that face at the given size.
 */
struct CanvasFont {
  enum class Family {
    DEFAULT,
    SWISS,    ///< a sans-serif font
    TELETYPE, ///< a fixed width font
  };
  String     family_name;               ///< the requested family name, as written in the Style (may be empty)
  Family     family = Family::DEFAULT;  ///< the generic family, used when family_name is not available
  bool       system = false;            ///< use the platform's default GUI font (family_name and family are ignored)
  int        point_size = 0;            ///< size in whole points
  double     system_size_scale = 0;     ///< if > 0: the size is this times the default GUI font's point size, instead of point_size
  bool       request_pixel_height = false; ///< also ask for a height of pixel_height pixels (wx parity only, see wx_canvas.cpp)
  double     pixel_height = 0;
  bool       bold = false;
  bool       italic = false;
  bool       underline = false;
  bool       strikethrough = false;
  FontFaceP  face;                      ///< the face resolved by the font registry, null if it found none

  /// The platform's default GUI font, as it is (point_size = 0 and system_size_scale = 0)
  static CanvasFont systemFont() { CanvasFont f; f.system = true; return f; }
  /// A generic font of the given size
  static CanvasFont generic(int point_size, Family family, bool bold = false, const String& family_name = String());
};

// ----------------------------------------------------------------------------- : Canvas

/// A surface to draw on
class Canvas {
public:
  virtual ~Canvas() {}

  /// Size in pixels
  virtual wxSize getSize() const = 0;

  // --------------------------------------------------- : Paint state

  virtual void setPen(const Pen& pen) = 0;
  virtual void setBrush(const Brush& brush) = 0;

  /// Combine drawn pixels with the existing ones using a raster operation
  /** Only operations for which supportsRasterOp is true are guaranteed to have the bitwise
   *  meaning; what the others do is up to the implementation (see wx_canvas.cpp).
   */
  virtual void setRasterOp(RasterOp op) = 0;
  virtual RasterOp getRasterOp() const = 0;
  /// Is the raster operation implemented bitwise for shapes and blits?
  virtual bool supportsRasterOp(RasterOp op) const = 0;

  // --------------------------------------------------- : Shapes

  /// Fill the whole canvas with a color (this changes the pen and brush)
  virtual void clear(const Color& color) = 0;
  virtual void drawLine(int x1, int y1, int x2, int y2) = 0;
  virtual void drawRectangle(int x, int y, int w, int h) = 0;
  virtual void drawRoundedRectangle(int x, int y, int w, int h, double radius) = 0;
  virtual void drawPolygon(int n, const wxPoint points[]) = 0;
  virtual void drawCircle(int x, int y, int radius) = 0;
  virtual void drawEllipse(int x, int y, int w, int h) = 0;
  /// Arc of the ellipse in the given bounding box, angles in degrees, counter clockwise
  virtual void drawEllipticArc(int x, int y, int w, int h, double start, double end) = 0;

  // --------------------------------------------------- : Images

  /// Draw an image with its top-left corner at (x,y), blending with its alpha channel.
  /** If use_mask, pixels of the image's mask colour are not drawn. */
  virtual void drawImage(const Image& image, int x, int y, bool use_mask = false) = 0;
  /// Read back a rectangle of pixels (RGB, no alpha)
  virtual Image readBack(int x, int y, int w, int h) = 0;
  /// Copy a rectangle of another canvas onto this one using a raster operation
  virtual void blit(int x, int y, int w, int h, Canvas& source, int source_x, int source_y, RasterOp op = RasterOp::COPY) = 0;

  // --------------------------------------------------- : Clipping

  virtual void setClipRegion(const CanvasRegion& region) = 0;
  virtual void destroyClipRegion() = 0;

  // --------------------------------------------------- : Text

  virtual void setFont(const CanvasFont& font) = 0;
  virtual const CanvasFont& getFont() const = 0;
  /// The size in points of the current font, as the canvas actually uses it
  virtual int getFontPointSize() const = 0;
  virtual void setTextColor(const Color& color) = 0;
  virtual Color getTextColor() const = 0;

  /// Size of a piece of text in the current font
  virtual void getTextExtent(const String& text, int* w, int* h) = 0;
  /// Height of a line of text in the current font
  virtual int getCharHeight() = 0;
  /// Draw text with aliased glyphs in the current font and text color,
  /// with the top-left corner of the text at (x,y), rotated counter clockwise around it.
  /** If subpixel > 1, then (x,y) is in units of 1/subpixel pixels, and the text is drawn at
   *  1/subpixel times the font's size. */
  virtual void drawText(const String& text, int x, int y, double angle_degrees, int subpixel = 1) = 0;
};

// ----------------------------------------------------------------------------- : Offscreen canvases

/// Pixel format of an offscreen canvas
enum class CanvasFormat {
  RGB,  ///< opaque, 24 bits per pixel; the initial contents are unspecified, clear it first
  RGBA, ///< with an alpha channel; starts out transparent
};

/// A canvas drawing into an image in memory
class OffscreenCanvas : public virtual Canvas {
public:
  /// The pixels drawn so far
  virtual Image getImage() = 0;
};
typedef unique_ptr<OffscreenCanvas> OffscreenCanvasP;

/// Makes offscreen canvases, installed by the host
class CanvasFactory {
public:
  virtual ~CanvasFactory() {}
  /// A new canvas of the given size
  virtual OffscreenCanvasP create(int width, int height, CanvasFormat format) = 0;
  /// A new canvas that starts out with the given image (including its alpha channel)
  virtual OffscreenCanvasP create(const Image& image) = 0;
  /// New font files of the shared font store (ADR 0005). A backend that selects fonts by family
  /// name through the platform (the wx Canvas, through fontconfig) registers them there; one that
  /// is handed the font registry's resolved faces ignores them. Returns false if any failed.
  virtual bool addFontFiles(const vector<String>& paths) { return true; }
};

/// The installed factory; throws an InternalError if there is none
CanvasFactory& canvas_factory();
/// Install the factory (by the host, at startup)
void set_canvas_factory(unique_ptr<CanvasFactory> factory);

/// A new offscreen canvas of the given size, from the installed factory
inline OffscreenCanvasP new_canvas(int width, int height, CanvasFormat format = CanvasFormat::RGB) {
  return canvas_factory().create(width, height, format);
}
/// A new offscreen canvas starting out with the given image, from the installed factory
inline OffscreenCanvasP new_canvas(const Image& image) {
  return canvas_factory().create(image);
}
