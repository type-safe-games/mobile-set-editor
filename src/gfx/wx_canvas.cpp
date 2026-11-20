//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gfx/wx_canvas.hpp>
#include <gfx/wx_image_adapter.hpp>
#include <util/rotation.hpp>
#include <util/error.hpp>
#include <wx/fontenum.h>
#if defined(__WXGTK__) && defined(MSE_HAVE_PANGOFC)
  #include <pango/pangocairo.h>
  #include <pango/pangofc-fontmap.h>
#endif

// ----------------------------------------------------------------------------- : Conversion

static wxColour to_wx(const Color& c) {
  return wxColour(c.r, c.g, c.b, c.a);
}

static wxPen to_wx(const Pen& pen) {
  switch (pen.style) {
    case PenStyle::TRANSPARENT:
      return *wxTRANSPARENT_PEN;
    case PenStyle::DOT: {
      #if defined(__WXMSW__)
        // on windows, wxDOT is not actually dotted, so use a custom style to achieve that
        static wxDash dashes_dotted[] = { 0,2 };
        wxPen wx_pen(to_wx(pen.color), pen.width, wxPENSTYLE_USER_DASH);
        wx_pen.SetDashes(2, dashes_dotted);
        return wx_pen;
      #else
        return wxPen(to_wx(pen.color), pen.width, wxPENSTYLE_DOT);
      #endif
    }
    default:
      return wxPen(to_wx(pen.color), pen.width, wxPENSTYLE_SOLID);
  }
}

static wxBrush to_wx(const Brush& brush) {
  if (brush.transparent) return *wxTRANSPARENT_BRUSH;
  return wxBrush(to_wx(brush.color));
}

static wxRasterOperationMode to_wx(RasterOp op) {
  switch (op) {
    case RasterOp::AND:        return wxAND;
    case RasterOp::OR:         return wxOR;
    case RasterOp::XOR:        return wxXOR;
    case RasterOp::AND_INVERT: return wxAND_INVERT;
    case RasterOp::INVERT:     return wxINVERT;
    default:                   return wxCOPY;
  }
}

static RasterOp from_wx(wxRasterOperationMode op) {
  switch (op) {
    case wxAND:        return RasterOp::AND;
    case wxOR:         return RasterOp::OR;
    case wxXOR:        return RasterOp::XOR;
    case wxAND_INVERT: return RasterOp::AND_INVERT;
    case wxINVERT:     return RasterOp::INVERT;
    default:           return RasterOp::COPY;
  }
}

// ----------------------------------------------------------------------------- : Fonts

// Construct a wxFont without going through the legacy 6-argument wxFont(...)
// That constructor silently treats a pointSize of exactly 70 as a request for wxDEFAULT
static wxFont make_font(int point_size, wxFontFamily family, wxFontStyle style, wxFontWeight weight, bool underlined, const String& face) {
  wxFont font;
  font.SetFamily(family);
  font.SetStyle(style);
  font.SetWeight(weight);
  font.SetUnderlined(underlined);
  if (!face.empty()) font.SetFaceName(face);
  font.SetPointSize(point_size);
  return font;
}

static wxFontFamily to_wx(CanvasFont::Family family) {
  switch (family) {
    case CanvasFont::Family::SWISS:    return wxFONTFAMILY_SWISS;
    case CanvasFont::Family::TELETYPE: return wxFONTFAMILY_TELETYPE;
    default:                           return wxFONTFAMILY_DEFAULT;
  }
}

// The font is selected by its family name through the platform (fontconfig/Pango on GTK, GDI on
// Windows), exactly as before the Canvas seam, and not by the face the font registry resolved
// (CanvasFont::face). wx can't load a face from a file: wxFont::AddPrivateFont only adds the file
// to fontconfig's set, and selection stays by family name. Selecting by the resolved face's family
// name changes which font some Styles get (they ask for a full name, like "Arsenal Regular",
// which fontconfig doesn't match and replaces with a system font), and so the layout of golden
// images (ADR 0004).
wxFont to_wx_font(const CanvasFont& f) {
  if (f.system) {
    wxFont font = *wxNORMAL_FONT;
    if (f.system_size_scale > 0) {
      font.SetPointSize(int(f.system_size_scale * font.GetPointSize()));
    } else if (f.point_size > 0) {
      font.SetPointSize(f.point_size);
    }
    if (f.bold) font.SetWeight(wxFONTWEIGHT_BOLD);
    if (f.strikethrough) font.MakeStrikethrough();
    return font;
  }
  wxFontStyle  style  = f.italic ? wxFONTSTYLE_ITALIC : wxFONTSTYLE_NORMAL;
  wxFontWeight weight = f.bold   ? wxFONTWEIGHT_BOLD  : wxFONTWEIGHT_NORMAL;
  if (!f.request_pixel_height && f.system_size_scale <= 0) {
    // a generic font, as constructed by GUI code
    wxFont font(f.point_size, to_wx(f.family), style, weight, f.underline, f.family_name);
    if (f.strikethrough) font.MakeStrikethrough();
    return font;
  }
  // a Style font (FontRef::toCanvasFont)
  int point_size = f.system_size_scale > 0 ? int(f.system_size_scale * wxNORMAL_FONT->GetPointSize()) : f.point_size;
  wxFont font = make_font(point_size, to_wx(f.family), style, weight, f.underline, f.family_name);
  if (f.request_pixel_height) {
    #if defined(__WXMSW__) || defined(__WXGTK__)
      // make it independent of screen dpi, always use 96 dpi
      // Note: wx refuses a negative height (assert, no change), so on wxGTK 3.2 this is a no-op
      // and the size is just point_size; kept so the platforms behave as before.
      font.SetPixelSize(wxSize(0, -(int)(f.pixel_height + 0.5)));
    #endif
  }
  if (f.strikethrough) font.MakeStrikethrough();
  return font;
}

// ----------------------------------------------------------------------------- : WxCanvas

WxCanvas::WxCanvas(wxDC& dc) : dc(dc) {}

wxSize WxCanvas::getSize() const {
  return dc.GetSize();
}

void WxCanvas::setPen(const Pen& pen)       { dc.SetPen(to_wx(pen)); }
void WxCanvas::setBrush(const Brush& brush) { dc.SetBrush(to_wx(brush)); }
void WxCanvas::setRasterOp(RasterOp op)     { dc.SetLogicalFunction(to_wx(op)); }
RasterOp WxCanvas::getRasterOp() const      { return from_wx(dc.GetLogicalFunction()); }

bool WxCanvas::supportsRasterOp(RasterOp op) const {
  #ifdef __WXMSW__
    return true;
  #else
    // wxGTK draws with cairo, which has no bitwise operations: wxXOR becomes cairo's XOR
    // compositing, and AND, OR, AND_INVERT and INVERT draw or blit nothing at all.
    return op == RasterOp::COPY;
  #endif
}

void WxCanvas::clear(const Color& color) {
  wxSize size = dc.GetSize();
  wxPoint pos = dc.GetDeviceOrigin(); // don't you love undocumented methods?
  dc.SetPen(*wxTRANSPARENT_PEN);
  dc.SetBrush(wxBrush(to_wx(color)));
  dc.DrawRectangle(-pos.x, -pos.y, size.GetWidth(), size.GetHeight());
}

void WxCanvas::drawLine(int x1, int y1, int x2, int y2) { dc.DrawLine(x1, y1, x2, y2); }
void WxCanvas::drawRectangle(int x, int y, int w, int h) { dc.DrawRectangle(x, y, w, h); }
void WxCanvas::drawRoundedRectangle(int x, int y, int w, int h, double radius) { dc.DrawRoundedRectangle(x, y, w, h, radius); }
void WxCanvas::drawPolygon(int n, const wxPoint points[]) { dc.DrawPolygon(n, points); }
void WxCanvas::drawCircle(int x, int y, int radius) { dc.DrawCircle(x, y, radius); }
void WxCanvas::drawEllipse(int x, int y, int w, int h) { dc.DrawEllipse(x, y, w, h); }
void WxCanvas::drawEllipticArc(int x, int y, int w, int h, double start, double end) { dc.DrawEllipticArc(x, y, w, h, start, end); }

void WxCanvas::drawImage(const Image& image, int x, int y, bool use_mask) {
  dc.DrawBitmap(to_bitmap(image), x, y, use_mask);
}

Image WxCanvas::readBack(int x, int y, int w, int h) {
  wxBitmap bmp(w, h, 24);
  wxMemoryDC mdc;
  mdc.SelectObject(bmp);
  mdc.Blit(0, 0, w, h, &dc, x, y, wxCOPY);
  mdc.SelectObject(wxNullBitmap);
  return from_bitmap(bmp);
}

void WxCanvas::blit(int x, int y, int w, int h, Canvas& source, int source_x, int source_y, RasterOp op) {
  dc.Blit(x, y, w, h, &wx_dc(source), source_x, source_y, to_wx(op));
}

void WxCanvas::setClipRegion(const CanvasRegion& region) {
  wxRegion wx_region;
  bool first = true;
  for (auto const& part : region.getParts()) {
    wxRegion r = part.is_rect ? wxRegion(part.rect) : wxRegion(part.polygon.size(), part.polygon.data());
    if (first) {
      wx_region = r;
      first = false;
    } else if (part.subtract) {
      wx_region.Subtract(r);
    } else {
      wx_region.Union(r);
    }
  }
  dc.SetDeviceClippingRegion(wx_region);
}

void WxCanvas::destroyClipRegion() {
  dc.DestroyClippingRegion();
}

void WxCanvas::setFont(const CanvasFont& font) {
  this->font = font;
  dc.SetFont(to_wx_font(font));
}
const CanvasFont& WxCanvas::getFont() const {
  return font;
}
int WxCanvas::getFontPointSize() const {
  return dc.GetFont().GetPointSize();
}

void WxCanvas::setTextColor(const Color& color) {
  dc.SetTextForeground(to_wx(color));
}
Color WxCanvas::getTextColor() const {
  return dc.GetTextForeground();
}

void WxCanvas::getTextExtent(const String& text, int* w, int* h) {
  int tw, th;
  dc.GetTextExtent(text, &tw, &th);
  #ifdef __WXGTK__
    // HACK: Some fonts don't get the descender height set correctly.
    int charHeight = dc.GetCharHeight();
    if (charHeight != th)
      th += th - charHeight;
  #endif
  *w = tw;
  *h = th;
}

int WxCanvas::getCharHeight() {
  int h = dc.GetCharHeight();
  #ifdef __WXGTK__
    // See above HACK
    int extent;
    dc.GetTextExtent(_("H"), 0, &extent);
    if (h != extent)
      h = 2 * extent - h;
  #endif
  return h;
}

void WxCanvas::drawText(const String& text, int x, int y, double angle_degrees, int subpixel) {
  if (subpixel != 1) {
    double usx,usy;
    dc.GetUserScale(&usx, &usy);
    dc.SetUserScale(usx/subpixel, usy/subpixel);
    dc.DrawRotatedText(text, x, y, angle_degrees);
    dc.SetUserScale(usx, usy);
  } else {
    dc.DrawRotatedText(text, x, y, angle_degrees);
  }
}

// ----------------------------------------------------------------------------- : Offscreen

/// The bitmap and memory dc of an offscreen canvas; a base class so they exist before WxCanvas
struct WxMemoryDCHolder {
  wxBitmap   bitmap;
  wxMemoryDC mdc;
  WxMemoryDCHolder(const wxBitmap& bmp) : bitmap(bmp) {
    mdc.SelectObject(bitmap);
  }
};

class WxOffscreenCanvas : private WxMemoryDCHolder, public OffscreenCanvas, public WxCanvas {
public:
  WxOffscreenCanvas(const wxBitmap& bmp) : WxMemoryDCHolder(bmp), WxCanvas(mdc) {}
  ~WxOffscreenCanvas() {
    mdc.SelectObject(wxNullBitmap);
  }
  Image getImage() override {
    mdc.SelectObject(wxNullBitmap);
    Image image = from_bitmap(bitmap);
    mdc.SelectObject(bitmap);
    return image;
  }
};

OffscreenCanvasP WxCanvasFactory::create(int width, int height, CanvasFormat format) {
  wxBitmap bmp(width, height, format == CanvasFormat::RGBA ? 32 : 24);
  if (!bmp.IsOk()) throw InternalError(_("Unable to create bitmap"));
  return make_unique<WxOffscreenCanvas>(bmp);
}

OffscreenCanvasP WxCanvasFactory::create(const Image& image) {
  wxBitmap bmp = to_bitmap(image);
  if (!bmp.IsOk()) throw InternalError(_("Unable to create bitmap"));
  return make_unique<WxOffscreenCanvas>(bmp);
}

bool WxCanvasFactory::addFontFiles(const vector<String>& paths) {
  return add_wx_font_files(paths);
}

bool add_wx_font_files(const vector<String>& paths) {
  if (paths.empty()) return true;
  bool ok = true;
  #if wxUSE_PRIVATE_FONTS
    // fontconfig then finds the files' families by name, as it does for a <dir> in its configuration
    for (const String& path : paths) {
      if (!wxFont::AddPrivateFont(path)) ok = false;
    }
    #if defined(__WXGTK__) && defined(MSE_HAVE_PANGOFC)
      // wxGTK 3.2 gives Pango its fontconfig configuration when the first file is added, and Pango
      // doesn't notice the files added to it after that
      PangoFontMap* font_map = pango_cairo_font_map_get_default();
      if (PANGO_IS_FC_FONT_MAP(font_map)) pango_fc_font_map_config_changed(PANGO_FC_FONT_MAP(font_map));
      wxFontEnumerator::InvalidateCache();
    #endif
  #else
    ok = false;
  #endif
  return ok;
}

// ----------------------------------------------------------------------------- : Helpers

wxDC& wx_dc(Canvas& canvas) {
  WxCanvas* wx_canvas = dynamic_cast<WxCanvas*>(&canvas);
  if (!wx_canvas) throw InternalError(_("Not a wx canvas"));
  return wx_canvas->getDC();
}

wxDC& wx_dc(RotatedDC& dc) {
  return wx_dc(dc.getCanvas());
}

void draw_bitmap(RotatedDC& dc, const wxBitmap& bitmap, const RealPoint& pos) {
  if (is_rad0(dc.getAngle())) {
    RealPoint p_ext = dc.tr(pos);
    wx_dc(dc).DrawBitmap(bitmap, to_int(p_ext.x), to_int(p_ext.y), true);
  } else {
    dc.DrawImage(from_bitmap(bitmap), pos);
  }
}
