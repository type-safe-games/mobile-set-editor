// Mobile Set Editor - drawing and text through Skia and HarfBuzz, in plain std types
// SPDX-License-Identifier: GPL-3.0-or-later

// No MSE headers here: this file is C++20 for Skia's sake (see skia_surface.hpp).

#include <gfx/skia/skia_surface.hpp>

#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkData.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkImage.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkRRect.h"
#include "include/core/SkRegion.h"
#include "include/core/SkTypeface.h"
#include "include/effects/SkDashPathEffect.h"
#include "include/ports/SkFontMgr_empty.h"

#include <hb.h>
#include <hb-ot.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
#include <utility>

namespace mse_skia {

// ----------------------------------------------------------------------------- : Typeface

class Typeface {
public:
  sk_sp<SkData>     data;
  sk_sp<SkTypeface> sk;
  hb_face_t*        hb = nullptr;

  ~Typeface() { if (hb) hb_face_destroy(hb); }
};

namespace {

void release_data(void* data) {
  static_cast<SkData*>(data)->unref();
}

SkFontMgr& font_manager() {
  static sk_sp<SkFontMgr> mgr = SkFontMgr_New_Custom_Empty();
  return *mgr;
}

} // namespace

TypefaceP load_typeface(const std::string& path, int index) {
  static std::mutex mutex;
  static std::map<std::pair<std::string, int>, TypefaceP> cache;
  std::lock_guard<std::mutex> lock(mutex);
  auto key = std::make_pair(path, index);
  auto it = cache.find(key);
  if (it != cache.end()) return it->second;
  TypefaceP face;
  sk_sp<SkData> data = SkData::MakeFromFileName(path.c_str());
  if (data) {
    auto tf = std::make_shared<Typeface>();
    tf->data = data;
    tf->sk = font_manager().makeFromData(data, index);
    hb_blob_t* blob = hb_blob_create((const char*)data->data(), (unsigned)data->size(), HB_MEMORY_MODE_READONLY,
                                     SkRef(data.get()), release_data);
    tf->hb = hb_face_create(blob, (unsigned)index);
    hb_blob_destroy(blob);
    if (tf->sk && hb_face_get_glyph_count(tf->hb) > 0) face = tf;
  }
  cache[key] = face; // also remember failures
  return face;
}

// ----------------------------------------------------------------------------- : Text

namespace {

/// Font size in 1/1024 pixels, as Pango gives it to HarfBuzz: points at 96 dpi
int pango_scale(int point_size) {
  return (int)std::llround(point_size * 96.0 / 72.0 * 1024);
}

/// Pango's PANGO_UNITS_ROUND: to whole pixels, in 1/1024 pixel units
std::int64_t units_round(std::int64_t v) {
  return (v + 512) & ~std::int64_t(1023);
}

std::int64_t units_ceil_px(std::int64_t v) {
  return v <= 0 ? -((-v) / 1024) : (v + 1023) / 1024;
}

/// A shaped line: glyphs and their positions in 1/1024 pixels, rounded as Pango does
struct ShapedLine {
  struct Run {
    TypefaceP face;
    std::vector<SkGlyphID> glyphs;
    std::vector<std::int64_t> x, y; ///< pen position plus offset
  };
  std::vector<Run> runs;
  std::int64_t width = 0;
  std::int64_t ascent = 0, descent = 0; ///< whole pixels, of the faces used
};

hb_font_t* make_hb_font(const Typeface& face, int point_size) {
  hb_font_t* hb = hb_font_create(face.hb);
  int scale = pango_scale(point_size);
  hb_font_set_scale(hb, scale, scale);
  return hb;
}

/// ceil(ascender) and ceil(descender) of a face, in whole pixels
void face_extents(const Typeface& face, int point_size, std::int64_t& ascent, std::int64_t& descent) {
  hb_font_t* hb = make_hb_font(face, point_size);
  hb_font_extents_t extents;
  hb_font_get_h_extents(hb, &extents);
  ascent = units_ceil_px(extents.ascender);
  descent = units_ceil_px(-extents.descender);
  hb_font_destroy(hb);
}

/// Decode one UTF-8 character at pos (advancing pos); invalid bytes decode as U+FFFD
std::uint32_t next_char(const std::string& s, size_t& pos) {
  unsigned char c = (unsigned char)s[pos++];
  int extra = c < 0x80 ? 0 : (c >> 5) == 6 ? 1 : (c >> 4) == 14 ? 2 : (c >> 3) == 30 ? 3 : -1;
  if (extra < 0) return 0xFFFD;
  std::uint32_t cp = extra == 0 ? c : extra == 1 ? (c & 0x1F) : extra == 2 ? (c & 0x0F) : (c & 0x07);
  for (int i = 0 ; i < extra ; ++i) {
    if (pos >= s.size() || ((unsigned char)s[pos] >> 6) != 2) return 0xFFFD;
    cp = (cp << 6) | ((unsigned char)s[pos++] & 0x3F);
  }
  return cp;
}

/// Characters that don't make Pango change fonts: space separators, variation selectors, and
/// line and paragraph separators
bool is_neutral(std::uint32_t c) {
  return c == 0x20 || c == 0xA0 || c == 0x1680 || (c >= 0x2000 && c <= 0x200A) || c == 0x202F || c == 0x205F
      || c == 0x3000 || c == 0x2028 || c == 0x2029 || (c >= 0xFE00 && c <= 0xFE0F) || (c >= 0xE0100 && c <= 0xE01EF);
}

ShapedLine shape(const TextFont& font, const std::string& text) {
  ShapedLine line;
  if (!font.face) return line;
  if (text.empty()) {
    // an empty line is as high as the face's lines
    face_extents(*font.face, font.point_size, line.ascent, line.descent);
    return line;
  }
  hb_font_t* primary = make_hb_font(*font.face, font.point_size);
  hb_font_t* fallback = font.fallback ? make_hb_font(*font.fallback, font.point_size) : nullptr;
  // Which face each character uses: the face if it has the character, else the fallback if
  // that has it. As in Pango's itemizer, spaces (and the like) never choose a face: they take
  // the face of the character before them, or at the start, of the first one after them.
  struct Char { size_t start, end; int choice; }; // choice: 0 face, 1 fallback, -1 either
  std::vector<Char> chars;
  for (size_t pos = 0 ; pos < text.size() ;) {
    size_t start = pos;
    std::uint32_t cp = next_char(text, pos);
    hb_codepoint_t glyph;
    int choice = is_neutral(cp) ? -1
               : fallback && !hb_font_get_nominal_glyph(primary, cp, &glyph) && hb_font_get_nominal_glyph(fallback, cp, &glyph) ? 1 : 0;
    chars.push_back(Char{start, pos, choice});
  }
  int current = 0;
  for (const Char& c : chars) if (c.choice >= 0) { current = c.choice; break; }
  struct Span { size_t start, end; bool use_fallback; };
  std::vector<Span> spans;
  for (Char& c : chars) {
    if (c.choice < 0) c.choice = current;
    current = c.choice;
    bool use_fallback = c.choice == 1;
    if (!spans.empty() && spans.back().use_fallback == use_fallback) spans.back().end = c.end;
    else spans.push_back(Span{c.start, c.end, use_fallback});
  }
  std::int64_t pen = 0;
  for (const Span& span : spans) {
    hb_font_t* hb = span.use_fallback ? fallback : primary;
    ShapedLine::Run run;
    run.face = span.use_fallback ? font.fallback : font.face;
    hb_buffer_t* buffer = hb_buffer_create();
    hb_buffer_add_utf8(buffer, text.data(), (int)text.size(), (unsigned)span.start, (int)(span.end - span.start));
    hb_buffer_guess_segment_properties(buffer);
    hb_shape(hb, buffer, nullptr, 0);
    unsigned n = 0;
    hb_glyph_info_t* info = hb_buffer_get_glyph_infos(buffer, &n);
    hb_glyph_position_t* pos = hb_buffer_get_glyph_positions(buffer, &n);
    run.glyphs.resize(n);
    run.x.resize(n);
    run.y.resize(n);
    for (unsigned i = 0 ; i < n ; ++i) {
      run.glyphs[i] = (SkGlyphID)info[i].codepoint;
      run.x[i] = pen + units_round(pos[i].x_offset);
      run.y[i] = -units_round(pos[i].y_offset); // HarfBuzz's y goes up
      pen += units_round(pos[i].x_advance);
    }
    hb_buffer_destroy(buffer);
    // the line is as high as the faces it uses
    std::int64_t a, d;
    face_extents(*run.face, font.point_size, a, d);
    line.ascent = std::max(line.ascent, a);
    line.descent = std::max(line.descent, d);
    line.runs.push_back(std::move(run));
  }
  line.width = pen;
  hb_font_destroy(primary);
  if (fallback) hb_font_destroy(fallback);
  return line;
}

} // namespace

void text_extent(const TextFont& font, const std::string& utf8, int* width, int* height) {
  ShapedLine line = shape(font, utf8);
  *width = (int)(line.width / 1024);
  *height = (int)(line.ascent + line.descent);
}

int line_height(const TextFont& font) {
  if (!font.face) return 0;
  std::int64_t ascent, descent;
  face_extents(*font.face, font.point_size, ascent, descent);
  return (int)(ascent + descent);
}

// ----------------------------------------------------------------------------- : Surface

struct Surface::Impl {
  SkBitmap bitmap;
  std::unique_ptr<SkCanvas> canvas;
  bool alpha;
  RGBA pen_color;
  int pen_width = 1;
  PenStyle pen_style = PenStyle::SOLID;
  RGBA brush_color;
  bool brush_none = false;
  Composite composite = Composite::OVER;
  bool clipped = false;

  SkPaint basePaint() const {
    SkPaint paint;
    paint.setAntiAlias(true);
    if (composite == Composite::XOR) paint.setBlendMode(SkBlendMode::kXor);
    return paint;
  }
  bool fill(SkPaint& paint) const {
    if (brush_none || composite == Composite::NOTHING) return false;
    paint = basePaint();
    paint.setColor(SkColorSetARGB(brush_color.a, brush_color.r, brush_color.g, brush_color.b));
    paint.setStyle(SkPaint::kFill_Style);
    return true;
  }
  bool stroke(SkPaint& paint) const {
    if (pen_style == PenStyle::NONE || composite == Composite::NOTHING) return false;
    paint = basePaint();
    paint.setColor(SkColorSetARGB(pen_color.a, pen_color.r, pen_color.g, pen_color.b));
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth((SkScalar)std::max(1, pen_width));
    if (pen_style == PenStyle::DOT) {
      SkScalar w = (SkScalar)std::max(1, pen_width);
      SkScalar intervals[2] = {w, w};
      paint.setPathEffect(SkDashPathEffect::Make(intervals, 0));
    }
    return true;
  }
  /// cairo strokes odd widths on pixel centres; wx shifts the outline by half a pixel for that
  SkScalar strokeOffset() const {
    return pen_style != PenStyle::NONE && std::max(1, pen_width) % 2 == 1 ? 0.5f : 0.f;
  }
  bool hasPen() const { return pen_style != PenStyle::NONE && pen_width > 0; }
};

Surface::Surface(int width, int height, bool alpha) : impl(std::make_unique<Impl>()) {
  impl->alpha = alpha;
  SkImageInfo info = SkImageInfo::MakeN32(std::max(width, 0), std::max(height, 0),
                                          alpha ? kPremul_SkAlphaType : kOpaque_SkAlphaType);
  impl->bitmap.allocPixels(info);
  impl->bitmap.eraseColor(alpha ? SK_ColorTRANSPARENT : SK_ColorBLACK);
  impl->canvas = std::make_unique<SkCanvas>(impl->bitmap);
}

Surface::~Surface() {}

int Surface::width() const { return impl->bitmap.width(); }
int Surface::height() const { return impl->bitmap.height(); }
bool Surface::hasAlpha() const { return impl->alpha; }

void Surface::writePixels(const std::uint8_t* rgba) {
  SkImageInfo info = SkImageInfo::Make(width(), height(), kRGBA_8888_SkColorType,
                                       impl->alpha ? kUnpremul_SkAlphaType : kOpaque_SkAlphaType);
  impl->bitmap.writePixels(SkPixmap(info, rgba, info.minRowBytes()), 0, 0);
}

void Surface::readPixels(int x, int y, int w, int h, std::uint8_t* rgba) const {
  std::fill(rgba, rgba + (size_t)w * h * 4, 0);
  SkImageInfo info = SkImageInfo::Make(w, h, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
  impl->bitmap.readPixels(info, rgba, info.minRowBytes(), x, y);
}

void Surface::setPen(RGBA color, int width, PenStyle style) {
  impl->pen_color = color;
  impl->pen_width = width;
  impl->pen_style = style;
}

void Surface::setBrush(RGBA color, bool none) {
  impl->brush_color = color;
  impl->brush_none = none;
}

void Surface::setComposite(Composite composite) {
  impl->composite = composite;
}

void Surface::drawLine(int x1, int y1, int x2, int y2) {
  SkPaint paint;
  if (!impl->stroke(paint)) return;
  SkScalar o = impl->strokeOffset();
  impl->canvas->drawLine(x1 + o, y1 + o, x2 + o, y2 + o, paint);
}

void Surface::drawRectangle(int x, int y, int w, int h) {
  if (w == 0 || h == 0) return;
  SkPaint paint;
  // wx: the outline is drawn inside the rectangle, one pixel smaller than the fill would be
  if (impl->hasPen()) { w -= 1; h -= 1; }
  SkScalar o = impl->strokeOffset();
  SkRect rect = SkRect::MakeXYWH(x + o, y + o, (SkScalar)w, (SkScalar)h);
  if (impl->fill(paint)) {
    // a rectangle without outline fills whole pixels
    impl->canvas->drawRect(impl->hasPen() ? rect : SkRect::MakeXYWH(x, y, w, h), paint);
  }
  if (impl->stroke(paint)) impl->canvas->drawRect(rect, paint);
}

void Surface::drawRoundedRectangle(int x, int y, int w, int h, double radius) {
  if (radius < 0) radius = -radius * std::min(w, h); // wx: a negative radius is a proportion
  if (impl->hasPen()) { w -= 1; h -= 1; }
  SkScalar o = impl->strokeOffset();
  SkRRect rr = SkRRect::MakeRectXY(SkRect::MakeXYWH(x + o, y + o, (SkScalar)w, (SkScalar)h), (SkScalar)radius, (SkScalar)radius);
  SkPaint paint;
  if (impl->fill(paint)) impl->canvas->drawRRect(rr, paint);
  if (impl->stroke(paint)) impl->canvas->drawRRect(rr, paint);
}

void Surface::drawPolygon(const std::vector<Point>& points) {
  if (points.size() < 2) return;
  SkScalar o = impl->strokeOffset();
  SkPathBuilder builder;
  builder.setFillType(SkPathFillType::kEvenOdd); // wxODDEVEN_RULE, wx's default
  builder.moveTo(points[0].x + o, points[0].y + o);
  for (size_t i = 1 ; i < points.size() ; ++i) builder.lineTo(points[i].x + o, points[i].y + o);
  builder.close();
  SkPath path = builder.detach();
  SkPaint paint;
  if (impl->fill(paint)) impl->canvas->drawPath(path, paint);
  if (impl->stroke(paint)) impl->canvas->drawPath(path, paint);
}

void Surface::drawEllipse(int x, int y, int w, int h) {
  if (impl->hasPen()) { w -= 1; h -= 1; }
  SkScalar o = impl->strokeOffset();
  SkRect oval = SkRect::MakeXYWH(x + o, y + o, (SkScalar)w, (SkScalar)h);
  SkPaint paint;
  if (impl->fill(paint)) impl->canvas->drawOval(oval, paint);
  if (impl->stroke(paint)) impl->canvas->drawOval(oval, paint);
}

void Surface::drawEllipticArc(int x, int y, int w, int h, double start, double end) {
  if (impl->hasPen()) { w -= 1; h -= 1; }
  SkScalar o = impl->strokeOffset();
  SkRect oval = SkRect::MakeXYWH(x + o, y + o, (SkScalar)w, (SkScalar)h);
  // wx: counter clockwise from 3 o'clock; Skia: clockwise
  double sweep = end - start;
  if (sweep <= 0) sweep += 360;
  SkPaint paint;
  if (impl->fill(paint)) impl->canvas->drawArc(oval, (SkScalar)-start, (SkScalar)-sweep, true, paint);
  if (impl->stroke(paint)) impl->canvas->drawArc(oval, (SkScalar)-start, (SkScalar)-sweep, false, paint);
}

void Surface::drawPixels(const std::uint8_t* rgba, int w, int h, int x, int y) {
  if (impl->composite == Composite::NOTHING || w <= 0 || h <= 0) return;
  SkImageInfo info = SkImageInfo::Make(w, h, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
  sk_sp<SkImage> image = SkImages::RasterFromPixmapCopy(SkPixmap(info, rgba, info.minRowBytes()));
  if (!image) return;
  SkPaint paint;
  if (impl->composite == Composite::XOR) paint.setBlendMode(SkBlendMode::kXor);
  impl->canvas->drawImage(image, (SkScalar)x, (SkScalar)y, SkSamplingOptions(), &paint);
}

void Surface::blit(int x, int y, int w, int h, const Surface& source, int source_x, int source_y, Composite composite) {
  if (composite == Composite::NOTHING || w <= 0 || h <= 0) return;
  SkIRect src = SkIRect::MakeXYWH(source_x, source_y, w, h);
  SkBitmap part;
  if (!source.impl->bitmap.extractSubset(&part, src)) return;
  sk_sp<SkImage> image = part.asImage();
  SkPaint paint;
  // wx's wxCOPY blit is cairo's SOURCE: the pixels (and alpha) replace what is there
  paint.setBlendMode(composite == Composite::XOR ? SkBlendMode::kXor : SkBlendMode::kSrc);
  SkIRect visible = SkIRect::MakeXYWH(x, y, part.width(), part.height());
  impl->canvas->save();
  impl->canvas->clipIRect(visible);
  impl->canvas->drawImage(image, (SkScalar)x, (SkScalar)y, SkSamplingOptions(), &paint);
  impl->canvas->restore();
}

void Surface::setClip(const std::vector<ClipPart>& parts) {
  clearClip();
  SkRegion region;
  bool first = true;
  SkRegion bounds(SkIRect::MakeWH(width(), height()));
  for (const ClipPart& part : parts) {
    SkRegion r;
    if (part.is_rect) {
      r.setRect(SkIRect::MakeXYWH(part.x, part.y, part.w, part.h));
    } else if (!part.polygon.empty()) {
      SkPathBuilder builder;
      builder.setFillType(SkPathFillType::kEvenOdd);
      builder.moveTo((SkScalar)part.polygon[0].x, (SkScalar)part.polygon[0].y);
      for (size_t i = 1 ; i < part.polygon.size() ; ++i) builder.lineTo((SkScalar)part.polygon[i].x, (SkScalar)part.polygon[i].y);
      builder.close();
      r.setPath(builder.detach(), bounds);
    }
    if (first) {
      region = r;
      first = false;
    } else {
      region.op(r, part.subtract ? SkRegion::kDifference_Op : SkRegion::kUnion_Op);
    }
  }
  impl->canvas->save();
  impl->canvas->clipRegion(region);
  impl->clipped = true;
}

void Surface::clearClip() {
  if (impl->clipped) {
    impl->canvas->restore();
    impl->clipped = false;
  }
}

void Surface::drawText(const TextFont& font, RGBA color, const std::string& utf8, int x, int y, double angle, int subpixel) {
  if (!font.face || impl->composite == Composite::NOTHING || utf8.empty()) return;
  ShapedLine line = shape(font, utf8);
  SkScalar px = (SkScalar)(pango_scale(font.point_size) / 1024.0);
  SkScalar ascent = (SkScalar)line.ascent;
  SkPaint paint;
  paint.setAntiAlias(false);
  paint.setColor(SkColorSetARGB(color.a, color.r, color.g, color.b));
  if (impl->composite == Composite::XOR) paint.setBlendMode(SkBlendMode::kXor);
  SkCanvas& canvas = *impl->canvas;
  canvas.save();
  if (subpixel > 1) canvas.scale(1.f / subpixel, 1.f / subpixel);
  canvas.translate((SkScalar)x, (SkScalar)y);
  if (angle != 0) canvas.rotate((SkScalar)-angle);
  for (const ShapedLine::Run& run : line.runs) {
    SkFont sk_font(run.face->sk, px);
    sk_font.setEdging(SkFont::Edging::kAlias);
    sk_font.setHinting(SkFontHinting::kNone);
    sk_font.setLinearMetrics(true);
    sk_font.setSubpixel(true);
    if (font.embolden) sk_font.setEmbolden(true);
    if (font.skew) sk_font.setSkewX(-0.2f); // as cairo's (and fontconfig's) synthetic oblique
    std::vector<SkPoint> positions(run.glyphs.size());
    for (size_t i = 0 ; i < positions.size() ; ++i) {
      positions[i] = SkPoint::Make(run.x[i] / 1024.f, ascent + run.y[i] / 1024.f);
    }
    canvas.drawGlyphs(SkSpan<const SkGlyphID>(run.glyphs), SkSpan<const SkPoint>(positions), SkPoint::Make(0, 0), sk_font, paint);
  }
  // underline and strikethrough, at Pango's usual places: below the baseline and at half x-height
  if (font.underline || font.strikethrough) {
    SkScalar thickness = std::max(1.f, std::round(px / 14.f));
    SkScalar w = line.width / 1024.f;
    if (font.underline) {
      canvas.drawRect(SkRect::MakeXYWH(0, ascent + std::round(px / 10.f), w, thickness), paint);
    }
    if (font.strikethrough) {
      canvas.drawRect(SkRect::MakeXYWH(0, ascent - std::round(px * 0.3f), w, thickness), paint);
    }
  }
  canvas.restore();
}

} // namespace mse_skia
