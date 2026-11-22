//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

/** @file gfx/wx_canvas.hpp
 *  WX BOUNDARY: the Canvas (gfx/canvas.hpp) implemented over a wxDC.
 *
 *  Every call maps onto the wxDC call the core made before the Canvas seam (#32), so rendering
 *  is unchanged. The desktop host installs WxCanvasFactory for offscreen canvases, and GUI code
 *  that paints on screen wraps its wxDC in a WxCanvas when it calls into core renderers.
 */

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gfx/canvas.hpp>
#include <util/real_point.hpp>

class RotatedDC;

// ----------------------------------------------------------------------------- : WxCanvas

/// A canvas drawing on a wxDC (which it does not own)
class WxCanvas : public virtual Canvas {
public:
  WxCanvas(wxDC& dc);

  /// The underlying wx DC
  wxDC& getDC() const { return dc; }

  IntSize getSize() const override;
  void setPen(const Pen& pen) override;
  void setBrush(const Brush& brush) override;
  void setRasterOp(RasterOp op) override;
  RasterOp getRasterOp() const override;
  bool supportsRasterOp(RasterOp op) const override;

  void clear(const Color& color) override;
  void drawLine(int x1, int y1, int x2, int y2) override;
  void drawRectangle(int x, int y, int w, int h) override;
  void drawRoundedRectangle(int x, int y, int w, int h, double radius) override;
  void drawPolygon(int n, const IntPoint points[]) override;
  void drawCircle(int x, int y, int radius) override;
  void drawEllipse(int x, int y, int w, int h) override;
  void drawEllipticArc(int x, int y, int w, int h, double start, double end) override;

  void drawImage(const Image& image, int x, int y, bool use_mask) override;
  Image readBack(int x, int y, int w, int h) override;
  void blit(int x, int y, int w, int h, Canvas& source, int source_x, int source_y, RasterOp op) override;

  void setClipRegion(const CanvasRegion& region) override;
  void destroyClipRegion() override;

  void setFont(const CanvasFont& font) override;
  const CanvasFont& getFont() const override;
  int getFontPointSize() const override;
  void setTextColor(const Color& color) override;
  Color getTextColor() const override;
  void getTextExtent(const String& text, int* w, int* h) override;
  int getCharHeight() override;
  void drawText(const String& text, int x, int y, double angle_degrees, int subpixel) override;

private:
  wxDC& dc;
  CanvasFont font; ///< the last font set through the canvas
};

// ----------------------------------------------------------------------------- : Offscreen

/// Makes offscreen canvases drawing into a wxBitmap through a wxMemoryDC
class WxCanvasFactory : public CanvasFactory {
public:
  OffscreenCanvasP create(int width, int height, CanvasFormat format) override;
  OffscreenCanvasP create(const Image& image) override;
  bool addFontFiles(const vector<String>& paths) override;
};

// ----------------------------------------------------------------------------- : Helpers for the GUI

/// The wx font for a canvas font
wxFont to_wx_font(const CanvasFont& font);

/// Make font files available to wx by family name (wxFont::AddPrivateFont); false if any failed
bool add_wx_font_files(const vector<String>& paths);

/// The wxDC under a canvas, which must be a WxCanvas
wxDC& wx_dc(Canvas& canvas);
/// The wxDC under a RotatedDC, whose canvas must be a WxCanvas
wxDC& wx_dc(RotatedDC& dc);

/// Draw a bitmap (already zoomed) on a RotatedDC whose canvas is a WxCanvas, with its mask
void draw_bitmap(RotatedDC& dc, const wxBitmap& bitmap, const RealPoint& pos);
