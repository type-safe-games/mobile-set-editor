// Mobile Set Editor - the Canvas over Skia
// SPDX-License-Identifier: GPL-3.0-or-later

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gfx/skia/skia_canvas.hpp>
#include <gfx/skia/skia_surface.hpp>
#include <gfx/font_registry.hpp>
#include <util/error.hpp>

int skia_system_font_point_size = 10;

namespace {

using mse_skia::RGBA;

RGBA to_rgba(const Color& c) {
  RGBA out;
  out.r = c.r; out.g = c.g; out.b = c.b; out.a = c.a;
  return out;
}

mse_skia::Composite composite_for(RasterOp op) {
  switch (op) {
    case RasterOp::COPY: return mse_skia::Composite::OVER;
    case RasterOp::XOR:  return mse_skia::Composite::XOR;
    default:             return mse_skia::Composite::NOTHING; // as wxGTK (ADR 0004)
  }
}

/// An image as straight RGBA; the mask colour (if any) is transparent, as when wxGTK draws it
vector<std::uint8_t> image_rgba(const Image& image) {
  const size_t n = (size_t)image.GetWidth() * image.GetHeight();
  vector<std::uint8_t> rgba(n * 4);
  const Byte* rgb = image.GetData();
  const Byte* alpha = image.GetAlpha();
  const bool mask = image.HasMask();
  const Byte mr = image.GetMaskRed(), mg = image.GetMaskGreen(), mb = image.GetMaskBlue();
  for (size_t i = 0 ; i < n ; ++i) {
    Byte r = rgb[3*i], g = rgb[3*i+1], b = rgb[3*i+2];
    rgba[4*i] = r; rgba[4*i+1] = g; rgba[4*i+2] = b;
    rgba[4*i+3] = mask && r == mr && g == mg && b == mb ? 0 : alpha ? alpha[i] : 255;
  }
  return rgba;
}

// ----------------------------------------------------------------------------- : SkiaCanvas

class SkiaCanvas : public OffscreenCanvas {
public:
  SkiaCanvas(int width, int height, bool alpha) : surface(width, height, alpha) {}

  mse_skia::Surface surface;

  IntSize getSize() const override { return IntSize(surface.width(), surface.height()); }

  // --------------------------------------------------- : Paint state

  void setPen(const Pen& pen) override {
    mse_skia::PenStyle style = pen.style == PenStyle::TRANSPARENT ? mse_skia::PenStyle::NONE
                             : pen.style == PenStyle::DOT ? mse_skia::PenStyle::DOT : mse_skia::PenStyle::SOLID;
    surface.setPen(to_rgba(pen.color), pen.width, style);
  }
  void setBrush(const Brush& brush) override {
    surface.setBrush(to_rgba(brush.color), brush.transparent);
  }
  void setRasterOp(RasterOp op) override {
    raster_op = op;
    surface.setComposite(composite_for(op));
  }
  RasterOp getRasterOp() const override { return raster_op; }
  bool supportsRasterOp(RasterOp op) const override { return op == RasterOp::COPY; }

  // --------------------------------------------------- : Shapes

  void clear(const Color& color) override {
    setPen(Pen::none());
    setBrush(Brush(color));
    surface.drawRectangle(0, 0, surface.width(), surface.height());
  }
  void drawLine(int x1, int y1, int x2, int y2) override { surface.drawLine(x1, y1, x2, y2); }
  void drawRectangle(int x, int y, int w, int h) override { surface.drawRectangle(x, y, w, h); }
  void drawRoundedRectangle(int x, int y, int w, int h, double radius) override { surface.drawRoundedRectangle(x, y, w, h, radius); }
  void drawPolygon(int n, const IntPoint points[]) override {
    std::vector<mse_skia::Point> pts(n);
    for (int i = 0 ; i < n ; ++i) { pts[i].x = points[i].x; pts[i].y = points[i].y; }
    surface.drawPolygon(pts);
  }
  void drawCircle(int x, int y, int radius) override { surface.drawEllipse(x - radius, y - radius, 2 * radius, 2 * radius); }
  void drawEllipse(int x, int y, int w, int h) override { surface.drawEllipse(x, y, w, h); }
  void drawEllipticArc(int x, int y, int w, int h, double start, double end) override { surface.drawEllipticArc(x, y, w, h, start, end); }

  // --------------------------------------------------- : Images

  void drawImage(const Image& image, int x, int y, bool use_mask) override {
    if (!image.IsOk()) return;
    // (wxGTK applies the mask whether use_mask is set or not)
    vector<std::uint8_t> rgba = image_rgba(image);
    surface.drawPixels(rgba.data(), image.GetWidth(), image.GetHeight(), x, y);
  }

  Image readBack(int x, int y, int w, int h) override {
    // as a blit into a 24 bit wx bitmap: the pixels over black
    Image image(w, h, false);
    if (!image.IsOk()) return image;
    vector<std::uint8_t> rgba((size_t)w * h * 4);
    surface.readPixels(x, y, w, h, rgba.data());
    Byte* rgb = image.GetData();
    for (size_t i = 0, n = (size_t)w * h ; i < n ; ++i) {
      unsigned a = rgba[4*i+3];
      for (int c = 0 ; c < 3 ; ++c) rgb[3*i+c] = (Byte)((rgba[4*i+c] * a + 127) / 255);
    }
    return image;
  }

  void blit(int x, int y, int w, int h, Canvas& source, int source_x, int source_y, RasterOp op) override {
    if (SkiaCanvas* skia = dynamic_cast<SkiaCanvas*>(&source)) {
      surface.blit(x, y, w, h, skia->surface, source_x, source_y, composite_for(op));
    } else {
      Image part = source.readBack(source_x, source_y, w, h);
      if (composite_for(op) == mse_skia::Composite::NOTHING) return;
      vector<std::uint8_t> rgba = image_rgba(part);
      surface.drawPixels(rgba.data(), w, h, x, y);
    }
  }

  Image getImage() override {
    const int w = surface.width(), h = surface.height();
    Image image(w, h, false);
    if (!image.IsOk()) return image;
    vector<std::uint8_t> rgba((size_t)w * h * 4);
    surface.readPixels(0, 0, w, h, rgba.data());
    Byte* rgb = image.GetData();
    Byte* alpha = nullptr;
    if (surface.hasAlpha()) {
      image.SetAlpha();
      alpha = image.GetAlpha();
    }
    for (size_t i = 0, n = (size_t)w * h ; i < n ; ++i) {
      rgb[3*i] = rgba[4*i]; rgb[3*i+1] = rgba[4*i+1]; rgb[3*i+2] = rgba[4*i+2];
      if (alpha) alpha[i] = rgba[4*i+3];
    }
    return image;
  }

  // --------------------------------------------------- : Clipping

  void setClipRegion(const CanvasRegion& region) override {
    std::vector<mse_skia::ClipPart> parts;
    for (auto const& p : region.getParts()) {
      mse_skia::ClipPart part;
      part.subtract = p.subtract;
      part.is_rect = p.is_rect;
      part.x = p.rect.x; part.y = p.rect.y; part.w = p.rect.width; part.h = p.rect.height;
      for (const IntPoint& pt : p.polygon) part.polygon.push_back(mse_skia::Point{pt.x, pt.y});
      parts.push_back(move(part));
    }
    surface.setClip(parts);
  }
  void destroyClipRegion() override { surface.clearClip(); }

  // --------------------------------------------------- : Text

  void setFont(const CanvasFont& font) override {
    this->font = font;
    text_font = mse_skia::TextFont();
    // the size, as wxGTK has it
    if (font.system) {
      text_font.point_size = font.system_size_scale > 0 ? int(font.system_size_scale * skia_system_font_point_size)
                           : font.point_size > 0 ? font.point_size : skia_system_font_point_size;
    } else {
      text_font.point_size = font.system_size_scale > 0 ? int(font.system_size_scale * skia_system_font_point_size) : font.point_size;
    }
    // the face: the one the registry resolved, or else what it resolves the name or fallback to
    FontFaceP face = font.face;
    if (!face) {
      FontRegistry& registry = font_registry();
      String family = font.system || font.family_name.empty() ? registry.fallback_family : font.family_name;
      face = registry.resolve(family, font.bold, font.italic);
    }
    if (face) {
      text_font.face = mse_skia::load_typeface(encode_utf8(face->path), face->index);
      text_font.embolden = font.bold && !face->isBold();
      text_font.skew = font.italic && !face->italic;
    }
    // characters the face doesn't have come from the fallback family, in the requested style
    FontFaceP fallback = font_registry().resolve(font_registry().fallback_family, font.bold, font.italic);
    if (fallback && fallback != face) text_font.fallback = mse_skia::load_typeface(encode_utf8(fallback->path), fallback->index);
    text_font.underline = font.underline;
    text_font.strikethrough = font.strikethrough;
  }
  const CanvasFont& getFont() const override { return font; }
  int getFontPointSize() const override { return text_font.point_size; }
  void setTextColor(const Color& color) override { text_color = color; }
  Color getTextColor() const override { return text_color; }

  /// Size of a line as Pango lays it out
  void lineExtent(const String& line, int* w, int* h) {
    mse_skia::text_extent(text_font, encode_utf8(line), w, h);
  }

  void getTextExtent(const String& text, int* w, int* h) override {
    // a line per newline, as Pango lays it out
    int width = 0, height = 0;
    size_t start = 0;
    while (true) {
      size_t end = text.find(L'\n', start);
      int lw, lh;
      lineExtent(text.substr(start, end == String::npos ? String::npos : end - start), &lw, &lh);
      width = max(width, lw);
      height += lh;
      if (end == String::npos) break;
      start = end + 1;
    }
    // wxGTK's DC measures the character height as the height of "g", and WxCanvas adds the
    // difference to the height when a text (with a taller fallback font) differs from that
    int g_w, char_height;
    lineExtent(_("g"), &g_w, &char_height);
    if (height != char_height) height += height - char_height;
    *w = width;
    *h = height;
  }
  int getCharHeight() override {
    // as WxCanvas: wxGTK's character height, corrected by the height of "H"
    int w, h, h_height;
    lineExtent(_("g"), &w, &h);
    lineExtent(_("H"), &w, &h_height);
    if (h != h_height) h = 2 * h_height - h;
    return h;
  }
  void drawText(const String& text, int x, int y, double angle_degrees, int subpixel) override {
    surface.drawText(text_font, to_rgba(text_color), encode_utf8(text), x, y, angle_degrees, subpixel);
  }

private:
  RasterOp raster_op = RasterOp::COPY;
  CanvasFont font;
  mse_skia::TextFont text_font;
  Color text_color = Color(0,0,0);
};

} // namespace

// ----------------------------------------------------------------------------- : SkiaCanvasFactory

OffscreenCanvasP SkiaCanvasFactory::create(int width, int height, CanvasFormat format) {
  if (width < 0 || height < 0) throw InternalError(_("Unable to create bitmap"));
  return make_unique<SkiaCanvas>(width, height, format == CanvasFormat::RGBA);
}

OffscreenCanvasP SkiaCanvasFactory::create(const Image& image) {
  if (!image.IsOk()) throw InternalError(_("Unable to create bitmap"));
  auto canvas = make_unique<SkiaCanvas>(image.GetWidth(), image.GetHeight(), image.HasAlpha());
  vector<std::uint8_t> rgba = image_rgba(image);
  canvas->surface.writePixels(rgba.data());
  return canvas;
}
