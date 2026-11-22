//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gfx/canvas.hpp>
#include <util/error.hpp>

// ----------------------------------------------------------------------------- : CanvasRegion

CanvasRegion::CanvasRegion(const IntRect& rect) {
  Part p;
  p.rect = rect;
  parts.push_back(p);
}

CanvasRegion::CanvasRegion(vector<IntPoint> polygon) {
  Part p;
  p.is_rect = false;
  p.polygon = move(polygon);
  parts.push_back(p);
}

void CanvasRegion::subtract(const CanvasRegion& other) {
  for (Part p : other.parts) {
    p.subtract = !p.subtract;
    parts.push_back(move(p));
  }
}

// ----------------------------------------------------------------------------- : CanvasFont

CanvasFont CanvasFont::generic(int point_size, Family family, bool bold, const String& family_name) {
  CanvasFont f;
  f.point_size  = point_size;
  f.family      = family;
  f.bold        = bold;
  f.family_name = family_name;
  return f;
}

// ----------------------------------------------------------------------------- : CanvasFactory

static unique_ptr<CanvasFactory> the_canvas_factory;

CanvasFactory& canvas_factory() {
  if (!the_canvas_factory) throw InternalError(_("No canvas factory installed"));
  return *the_canvas_factory;
}

void set_canvas_factory(unique_ptr<CanvasFactory> factory) {
  the_canvas_factory = move(factory);
}
