//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/font.hpp>
#include <gfx/font_registry.hpp>

// ----------------------------------------------------------------------------- : Font

FontRef::FontRef()
  : name()
  , size(1)
  , underline(false)
  , strikethrough(false)
  , scale_down_to(100000)
  , max_stretch(1.0)
  , color(Color(0,0,0))
  , shadow_color(Color(0,0,0))
  , shadow_displacement_x(0)
  , shadow_displacement_y(0)
  , shadow_blur(0)
  , stroke_color(Color(0,0,0))
  , stroke_radius(0)
  , stroke_blur(0)
  , separator_color(Color(0,0,0,128))
  , flags(FONT_NORMAL)
{}

bool FontRef::update(Context& ctx) {
  bool changes = false;
  changes |= name                 .update(ctx);
  changes |= italic_name          .update(ctx);
  changes |= size                 .update(ctx);
  changes |= weight               .update(ctx);
  changes |= style                .update(ctx);
  changes |= underline            .update(ctx);
  changes |= strikethrough        .update(ctx);
  changes |= color                .update(ctx);
  changes |= shadow_color         .update(ctx);
  changes |= shadow_displacement_x.update(ctx);
  changes |= shadow_displacement_y.update(ctx);
  changes |= shadow_blur          .update(ctx);
  changes |= stroke_color         .update(ctx);
  changes |= stroke_radius        .update(ctx);
  changes |= stroke_blur          .update(ctx);
  flags = (flags & ~FONT_BOLD & ~FONT_ITALIC)
        | (weight() == _("bold")   ? FONT_BOLD   : FONT_NORMAL)
        | (style()  == _("italic") ? FONT_ITALIC : FONT_NORMAL);
  return changes;
}
void FontRef::initDependencies(Context& ctx, const Dependency& dep) const {
  name                 .initDependencies(ctx, dep);
  italic_name          .initDependencies(ctx, dep);
  size                 .initDependencies(ctx, dep);
  weight               .initDependencies(ctx, dep);
  style                .initDependencies(ctx, dep);
  underline            .initDependencies(ctx, dep);
  strikethrough        .initDependencies(ctx, dep);
  color                .initDependencies(ctx, dep);
  shadow_color         .initDependencies(ctx, dep);
  shadow_displacement_x.initDependencies(ctx, dep);
  shadow_displacement_y.initDependencies(ctx, dep);
  shadow_blur          .initDependencies(ctx, dep);
  stroke_color         .initDependencies(ctx, dep);
  stroke_blur          .initDependencies(ctx, dep);
  stroke_radius        .initDependencies(ctx, dep);
}

FontRefP FontRef::make(int add_flags, bool add_underline, bool add_strikethrough, String const* other_family, Color const* other_color, double const* other_size) const {
  FontRefP f(new FontRef(*this));
  f->flags |= add_flags;
  if (add_flags & FONT_CODE_STRING) {
    f->color = Color(0,0,100);
  }
  if (add_flags & FONT_CODE) {
    f->color = Color(128,0,0);
  }
  if (add_flags & FONT_CODE_KW) {
    f->color = Color(158,100,0);
    f->flags |= FONT_BOLD;
  }
  if (add_flags & FONT_SOFT) {
    f->color = f->separator_color;
    f->shadow_displacement_x = 0; // no shadow
    f->shadow_displacement_y = 0; // no shadow
  }
  if (add_underline) {
    f->underline = true;
  }
  if (add_strikethrough) {
    f->strikethrough = true;
  }
  if (other_color) {
    f->color = *other_color;
  }
  if (other_size) {
    f->size = *other_size;
  }
  if (other_family && !other_family->empty()) {
    f->name = *other_family;
  }
  return f;
}

static const String BOLD_STRING   = _(" Bold");

CanvasFont FontRef::toCanvasFont(double scale) const {
  int size_i = to_int(scale * size);
  CanvasFont font;
  if (name().empty() && !(flags & FONT_CODE)) {
    // the default GUI font, only resized
    font.system = true;
    if (size > 1) {
      font.point_size = size_i;
    } else {
      font.system_size_scale = scale;
    }
    font.strikethrough = strikethrough();
    return font;
  }
  font.bold          = flags & FONT_BOLD;
  font.underline     = underline();
  font.strikethrough = strikethrough();
  bool italic_face = flags & FONT_ITALIC; // is an italic face wanted?
  if (flags & FONT_CODE) {
    font.family      = CanvasFont::Family::TELETYPE;
    font.family_name = _("Courier New");
    italic_face = false;
    if (size_i < 2) {
      font.system_size_scale = 1; // the size of the default GUI font
    } else {
      font.point_size = size_i;
    }
  } else if (!(flags & FONT_FROM_TAG) && (flags & FONT_ITALIC) && !italic_name().empty()) {
    // a separate family for italic text, it is used as it is
    font.family_name = italic_name();
    font.point_size  = size_i;
  } else {
    String familyName = name();
    if(ends_with(familyName, BOLD_STRING)) {
      familyName = familyName.substr(0, familyName.length() - BOLD_STRING.length());
      font.bold = true;
    }
    font.family_name = familyName;
    font.italic      = flags & FONT_ITALIC;
    font.point_size  = size_i;
  }
  if (!font.system_size_scale) {
    // make it independent of screen dpi, always use 96 dpi
    font.request_pixel_height = true;
    font.pixel_height = scale*size*96.0/72.0;
  }
  font.face = font_registry().resolve(font.family_name, font.bold, italic_face);
  return font;
}

IMPLEMENT_REFLECTION_NO_SCRIPT(FontRef) {
  REFLECT(name);
  REFLECT(size);
  REFLECT(weight);
  REFLECT(style);
  REFLECT(underline);
  REFLECT(strikethrough);
  REFLECT(italic_name);
  REFLECT(color);
  REFLECT(scale_down_to);
  REFLECT(max_stretch);
  REFLECT(shadow_color);
  REFLECT(shadow_displacement_x);
  REFLECT(shadow_displacement_y);
  REFLECT(shadow_blur);
  REFLECT(stroke_color);
  REFLECT(stroke_radius);
  REFLECT(stroke_blur);
  REFLECT(separator_color);
}
