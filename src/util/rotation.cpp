//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/rotation.hpp>
#include <gfx/gfx.hpp>
#include <data/font.hpp>

// ----------------------------------------------------------------------------- : Rotation

Rotation::Rotation(Radians angle, const RealRect& rect, double zoom, double stretch, RotationFlags flags)
  : angle(constrain_radians(angle))
  , size(rect.size())
  , origin(rect.position())
  , zoomX(zoom * stretch)
  , zoomY(zoom)
{
  if (stretch != 1.0) {
    size.width /= stretch;
  }
  // set origin
  if (flags & ROTATION_ATTACH_TOP_LEFT) {
    origin -= boundingBoxCorner(size);
  }
}

void Rotation::setStretch(double s) {
  size.width *= s * getStretch();
  zoomX = zoomY * s;
}


RealPoint Rotation::tr(const RealPoint& p) const {
  double s = sin(angle), c = cos(angle);
  double x = p.x * zoomX, y = p.y * zoomY;
  return RealPoint(c * x + s * y + origin.x,
                  -s * x + c * y + origin.y);
}
RealPoint Rotation::trPixel(const RealPoint& p) const {
  double s = sin(angle), c = cos(angle);
  double x = p.x * zoomX + 0.5, y = p.y * zoomY + 0.5;
  return RealPoint(c * x + s * y + origin.x - 0.5,
                  -s * x + c * y + origin.y - 0.5);
}
RealPoint Rotation::trNoZoom(const RealPoint& p) const {
  double s = sin(angle), c = cos(angle);
  double x = p.x, y = p.y;
  return RealPoint(c * x + s * y + origin.x,
                  -s * x + c * y + origin.y);
}
RealPoint Rotation::trPixelNoZoom(const RealPoint& p) const {
  double s = sin(angle), c = cos(angle);
  double x = p.x + 0.5, y = p.y + 0.5;
  return RealPoint(c * x + s * y + origin.x - 0.5,
                  -s * x + c * y + origin.y - 0.5);
}

RealSize Rotation::trSizeToBB(const RealSize& size) const {
  if (is_straight(angle)) {
    if (is_sideways(angle)) {
      return RealSize(size.height * zoomY, size.width * zoomX);
    } else {
      return RealSize(size.width * zoomX, size.height * zoomY);
    }
  } else {
    double s = sin(angle), c = cos(angle);
    double x = size.width * zoomX, y = size.height * zoomY;
    return RealSize(fabs(c * x) + fabs(s * y), fabs(s * x) + fabs(c * y));
  }
}

RealRect Rotation::trRectToBB(const RealRect& r) const {
  const bool special_case_optimization = true;
  double x = r.x     * zoomX, y = r.y      * zoomY;
  double w = r.width * zoomX, h = r.height * zoomY;
  if (special_case_optimization && is_rad0(angle)) {
    return RealRect(origin.x + x, origin.y + y, w, h);
  } else if (special_case_optimization && is_rad180(angle)) {
    return RealRect(origin.x - x - w, origin.y - y - h, w, h);
  } else if (special_case_optimization && is_rad90(angle)) {
    return RealRect(origin.x + y, origin.y - x - w, h, w);
  } else if (special_case_optimization && is_rad270(angle)) {
    return RealRect(origin.x - y - h, origin.y + x, h, w);
  } else {
    double s = sin(angle), c = cos(angle);
    RealRect result(c * x + s * y + origin.x,
                   -s * x + c * y + origin.y,
                   0,0);
    if (c > 0) {
      result.width  += c * w;
      result.height += c * h;
    } else {
      result.x      += c * w;
      result.width  -= c * w;
      result.y      += c * h;
      result.height -= c * h;
    }
    if (s > 0) {
      result.width  += s * h;
      result.y      -= s * w;
      result.height += s * w;
    } else {
      result.x      += s * h;
      result.width  -= s * h;
      result.height -= s * w;
    }
    return result;
  }
}

CanvasRegion Rotation::trRectToRegion(const RealRect& r) const {
  if (is_straight(angle)) {
    return trRectToBB(r).toRect();
  } else {
    vector<wxPoint> points = {trPixel(RealPoint(r.left(),  r.top()   ))
                             ,trPixel(RealPoint(r.left(),  r.bottom()))
                             ,trPixel(RealPoint(r.right(), r.bottom()))
                             ,trPixel(RealPoint(r.right(), r.top()   ))};
    return CanvasRegion(move(points));
  }
}

RealPoint Rotation::trInv(const RealPoint& p) const {
  double s = sin(angle), c = cos(angle);
  double x = p.x - origin.x, y = p.y - origin.y;
  return RealPoint((c * x - s * y) / zoomX,
                   (s * x + c * y) / zoomY);
}
RealSize Rotation::trInv(const RealSize& x) const {
  double s = sin(angle), c = cos(angle);
  return RealSize((c * x.width - s * x.height) / zoomX,
                  (s * x.width + c * x.height) / zoomY);
}

RealPoint Rotation::boundingBoxCorner(const RealSize& size) const {
  // This function is a bit tricky,
  // I derived it by drawing the four cases.
  // Two succeeding cases must agree where they overlap (0,90,180,270 degrees)
  double s = sin(angle), c = cos(angle);
  double w = size.width * zoomX, h = size.height * zoomY;
  if (angle <= rad90)  return RealPoint(0,            -w * s);
  if (angle <= rad180) return RealPoint(w * c,         h * c - w * s);
  if (angle <= rad270) return RealPoint(w * c + h * s, h * c);
  else                 return RealPoint(h * s,         0);
}

// ----------------------------------------------------------------------------- : Rotater

Rotater::Rotater(Rotation& rot, const Rotation& by)
  : old(rot)
  , rot(rot)
{
  // apply rotation
  rot.origin = rot.tr(by.origin);
  rot.size   = by.size;
  rot.angle  = constrain_radians(rot.angle + by.angle);
  // zooming is not really correct if rot.zoomX != rot.zoomY
  rot.zoomX *= by.zoomX;
  rot.zoomY *= by.zoomY;
}

Rotater::~Rotater() {
  rot = old; // restore
}

// ----------------------------------------------------------------------------- : RotatedDC

RotatedDC::RotatedDC(Canvas& canvas, Radians angle, const RealRect& rect, double zoom, RenderQuality quality, RotationFlags flags)
  : Rotation(angle, rect, zoom, 1.0, flags)
  , canvas(canvas), quality(quality)
{}

RotatedDC::RotatedDC(Canvas& canvas, const Rotation& rotation, RenderQuality quality)
  : Rotation(rotation)
  , canvas(canvas), quality(quality)
{}

// ----------------------------------------------------------------------------- : RotatedDC : Drawing

void RotatedDC::DrawText(const String& text, const RealPoint& pos, int blur_radius, Color stroke_color, int stroke_radius, double stretch) {
  DrawText(text, pos, canvas.getTextColor(), blur_radius, stroke_color, stroke_radius, stretch);
}

void RotatedDC::DrawText(const String& text, const RealPoint& pos, Color color, int blur_radius, Color stroke_color, int stroke_radius, double stretch) {
  if (text.empty()) return;
  if (color.Alpha() == 0) return;
  if (quality >= QUALITY_AA) {
    RealRect r(pos, GetTextExtent(text));
    RealRect r_ext = trRectToBB(r);
    RealPoint pos2 = tr(pos);
    stretch *= getStretch();
    if (fabs(stretch - 1) > 1e-6) {
      r.width *= stretch;
      RealRect r_ext2 = trRectToBB(r);
      pos2.x += r_ext2.x - r_ext.x;
      pos2.y += r_ext2.y - r_ext.y;
      r_ext.x = r_ext2.x;
      r_ext.y = r_ext2.y;
    }
    draw_resampled_text(canvas, text, pos2, r_ext, angle, color, blur_radius, stroke_color, stroke_radius, stretch);
  } else if (quality >= QUALITY_SUB_PIXEL) {
    RealPoint p_ext = tr(pos)*text_scaling;
    canvas.setTextColor(color);
    canvas.drawText(text, (int) p_ext.x, (int) p_ext.y, rad_to_deg(angle), text_scaling);
  } else {
    RealPoint p_ext = tr(pos);
    canvas.setTextColor(color);
    canvas.drawText(text, (int) p_ext.x, (int) p_ext.y, rad_to_deg(angle));
  }
}

void RotatedDC::DrawTextWithShadowOrStroke(const String& text, const FontRef& font, const RealPoint& pos, double scale, double stretch) {
  double s_scale = scale * canvas.getFontPointSize() / text_scaling / 15.;
  if (font.hasShadow() && !font.hasStroke()) {
    RealSize shadow_displacement = trInvS(RealSize(font.shadow_displacement_x, font.shadow_displacement_y) * s_scale);
    DrawText(text, pos + shadow_displacement, font.shadow_color, lround(font.shadow_blur * s_scale), Color(0,0,0), 0, stretch);
  }
  DrawText(text, pos, font.color, lround(font.stroke_blur * s_scale), font.stroke_color, lround(font.stroke_radius * s_scale), stretch);
}

void RotatedDC::DrawPlainImage(const Image& image, const RealPoint& pos) {
  if (is_rad0(angle)) {
    RealPoint p_ext = tr(pos);
    canvas.drawImage(image, to_int(p_ext.x), to_int(p_ext.y), true);
  } else {
    DrawImage(image, pos);
  }
}
void RotatedDC::DrawImage(const Image& image, const RealPoint& pos, ImageCombine combine) {
  Image rotated = rotate_image(image, angle);
  DrawPreRotatedImage(rotated, RealRect(pos,trInvS(RealSize(image))), combine);
}
void RotatedDC::DrawPreRotatedPlainImage(const Image& image, const RealRect& rect) {
  RealPoint p_ext = tr(rect.position()) + boundingBoxCorner(rect.size());
  canvas.drawImage(image, to_int(p_ext.x), to_int(p_ext.y), true);
}
void RotatedDC::DrawPreRotatedImage (const Image& image, const RealRect& rect, ImageCombine combine) {
  RealPoint p_ext = tr(rect.position()) + boundingBoxCorner(rect.size());
  draw_combine_image(canvas, to_int(p_ext.x), to_int(p_ext.y), image, combine);
}

void RotatedDC::DrawLine  (const RealPoint& p1,  const RealPoint& p2) {
  wxPoint p1_ext = tr(p1), p2_ext = tr(p2);
  canvas.drawLine(p1_ext.x, p1_ext.y, p2_ext.x, p2_ext.y);
}

void RotatedDC::DrawRectangle(const RealRect& r) {
  if (is_straight(angle)) {
    wxRect r_ext = trRectToBB(r);
    canvas.drawRectangle(r_ext.x, r_ext.y, r_ext.width, r_ext.height);
  } else {
    wxPoint points[4] = {trPixel(RealPoint(r.left(),  r.top()   ))
                        ,trPixel(RealPoint(r.left(),  r.bottom()))
                        ,trPixel(RealPoint(r.right(), r.bottom()))
                        ,trPixel(RealPoint(r.right(), r.top()   ))};
    canvas.drawPolygon(4,points);
  }
}

void RotatedDC::DrawRoundedRectangle(const RealRect& r, double radius) {
  if (is_straight(angle)) {
    wxRect r_ext = trRectToBB(r);
    canvas.drawRoundedRectangle(r_ext.x, r_ext.y, r_ext.width, r_ext.height, trS(radius));
  } else {
    // TODO
    DrawRectangle(r);
  }
}

void RotatedDC::DrawInvertRectangle(const RealRect& r) {
  wxRect r_ext = trRectToBB(r);

  Image img = canvas.readBack(r_ext.x, r_ext.y, r_ext.width, r_ext.height);
  Byte* data = img.GetData();
  for (size_t i = 0, n = 3 * (size_t)img.GetWidth() * img.GetHeight() ; i < n ; ++i) {
    data[i] = 255 - data[i];
  }

  canvas.drawImage(img, r_ext.x, r_ext.y, false);
}

void RotatedDC::DrawCircle(const RealPoint& center, double radius) {
  wxPoint p = tr(center);
  canvas.drawCircle(p.x + 1, p.y + 1, int(trS(radius)));
}

void RotatedDC::DrawEllipse(const RealPoint& center, const RealSize& size) {
  wxPoint c_ext = tr(center - size/2);
  wxSize  s_ext = trSizeToBB(size);
  canvas.drawEllipse(c_ext.x, c_ext.y, s_ext.x, s_ext.y);
}
void RotatedDC::DrawEllipticArc(const RealPoint& center, const RealSize& size, Radians start, Radians end) {
  wxPoint c_ext = tr(center - size/2);
  wxSize  s_ext = trSizeToBB(size);
  canvas.drawEllipticArc(c_ext.x, c_ext.y, s_ext.x, s_ext.y, rad_to_deg(start + angle), rad_to_deg(end + angle));
}
void RotatedDC::DrawEllipticSpoke(const RealPoint& center, const RealSize& size, Radians angle) {
  wxPoint c_ext = tr(center - size/2);
  wxSize  s_ext = trSizeToBB(size);
  Radians rot_angle = angle + this->angle;
  Radians sin_angle = sin(rot_angle), cos_angle = cos(rot_angle);
  // position of center and of point on the boundary can vary because of rounding errors,
  // this code matches DrawEllipticArc (at least on windows xp).
  canvas.drawLine(
    c_ext.x + int(       0.5 * (s_ext.x + cos_angle) ), // center
    c_ext.y + int(       0.5 * (s_ext.y - sin_angle) ),
    c_ext.x + int( 0.5 + 0.5 * (s_ext.x-1) * (1 + cos_angle) ), // boundary
    c_ext.y + int( 0.5 + 0.5 * (s_ext.y-1) * (1 - sin_angle) )
  );
}

// ----------------------------------------------------------------------------- : Forwarded properties

void RotatedDC::SetPen(const Pen& pen)                { canvas.setPen(pen); }
void RotatedDC::SetBrush(const Brush& brush)          { canvas.setBrush(brush); }
void RotatedDC::SetTextForeground(const Color& color) { canvas.setTextColor(color); }
void RotatedDC::SetRasterOp(RasterOp op)              { canvas.setRasterOp(op); }

void RotatedDC::SetFont(CanvasFont font) {
  if (quality == QUALITY_LOW && zoomX == 1 && zoomY == 1) {
    canvas.setFont(font);
  } else {
    int point_size = font.point_size;
    if (font.system_size_scale > 0 || (font.system && point_size == 0)) {
      // the size depends on the platform's default font, ask the canvas
      canvas.setFont(font);
      point_size = canvas.getFontPointSize();
      font.system_size_scale = 0;
    }
    if (quality == QUALITY_LOW) {
      font.point_size = (int)  trY(point_size);
    } else {
      font.point_size = (int) (trY(point_size) * text_scaling);
    }
    canvas.setFont(font);
  }
}
void RotatedDC::SetFont(const FontRef& font, double scale) {
  canvas.setFont(font.toCanvasFont(trS(scale) * (quality == QUALITY_LOW ? 1 : text_scaling)));
}

double RotatedDC::getFontSizeStep() const {
  if (quality == QUALITY_LOW) {
    return 1;
  } else {
    return 1. / text_scaling;
  }
}

RealSize RotatedDC::GetTextExtent(const String& text) const {
  int w, h;
  canvas.getTextExtent(text, &w, &h);
  if (quality == QUALITY_LOW) {
    return RealSize(w / zoomX, h / zoomY);
  } else {
    return RealSize(w / (zoomX * text_scaling), h / (zoomY * text_scaling));
  }
}
double RotatedDC::GetCharHeight() const {
  int h = canvas.getCharHeight();
  if (quality == QUALITY_LOW) {
    return h / zoomY;
  } else {
    return h / (zoomY * text_scaling);
  }
}

void RotatedDC::SetClippingRegion(const RealRect& rect) {
  canvas.setClipRegion(trRectToRegion(rect));
}
void RotatedDC::DestroyClippingRegion() {
  canvas.destroyClipRegion();
}

// ----------------------------------------------------------------------------- : Other

Image RotatedDC::GetBackground(const RealRect& r) {
  wxRect wr = trRectToBB(r);
  return canvas.readBack(wr.x, wr.y, wr.width, wr.height);
}
