//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gfx/color.hpp>

// ----------------------------------------------------------------------------- : Named colors

/// The colors of wxWidgets 3.2's colour database (wxColourDatabase::Initialize), which named
/// colors in templates were looked up in before the core became wx-free (#43)
static const struct { const wchar_t* name; unsigned char r, g, b; } named_colors[] = {
  {L"AQUAMARINE", 112, 219, 147},
  {L"BLACK", 0, 0, 0},
  {L"BLUE", 0, 0, 255},
  {L"BLUE VIOLET", 159, 95, 159},
  {L"BROWN", 165, 42, 42},
  {L"CADET BLUE", 95, 159, 159},
  {L"CORAL", 255, 127, 0},
  {L"CORNFLOWER BLUE", 66, 66, 111},
  {L"CYAN", 0, 255, 255},
  {L"DARK GREY", 47, 47, 47},
  {L"DARK GREEN", 47, 79, 47},
  {L"DARK OLIVE GREEN", 79, 79, 47},
  {L"DARK ORCHID", 153, 50, 204},
  {L"DARK SLATE BLUE", 107, 35, 142},
  {L"DARK SLATE GREY", 47, 79, 79},
  {L"DARK TURQUOISE", 112, 147, 219},
  {L"DIM GREY", 84, 84, 84},
  {L"FIREBRICK", 142, 35, 35},
  {L"FOREST GREEN", 35, 142, 35},
  {L"GOLD", 204, 127, 50},
  {L"GOLDENROD", 219, 219, 112},
  {L"GREY", 128, 128, 128},
  {L"GREEN", 0, 255, 0},
  {L"GREEN YELLOW", 147, 219, 112},
  {L"INDIAN RED", 79, 47, 47},
  {L"KHAKI", 159, 159, 95},
  {L"LIGHT BLUE", 191, 216, 216},
  {L"LIGHT GREY", 192, 192, 192},
  {L"LIGHT STEEL BLUE", 143, 143, 188},
  {L"LIME GREEN", 50, 204, 50},
  {L"LIGHT MAGENTA", 255, 119, 255},
  {L"MAGENTA", 255, 0, 255},
  {L"MAROON", 142, 35, 107},
  {L"MEDIUM AQUAMARINE", 50, 204, 153},
  {L"MEDIUM GREY", 100, 100, 100},
  {L"MEDIUM BLUE", 50, 50, 204},
  {L"MEDIUM FOREST GREEN", 107, 142, 35},
  {L"MEDIUM GOLDENROD", 234, 234, 173},
  {L"MEDIUM ORCHID", 147, 112, 219},
  {L"MEDIUM SEA GREEN", 66, 111, 66},
  {L"MEDIUM SLATE BLUE", 127, 0, 255},
  {L"MEDIUM SPRING GREEN", 127, 255, 0},
  {L"MEDIUM TURQUOISE", 112, 219, 219},
  {L"MEDIUM VIOLET RED", 219, 112, 147},
  {L"MIDNIGHT BLUE", 47, 47, 79},
  {L"NAVY", 35, 35, 142},
  {L"ORANGE", 204, 50, 50},
  {L"ORANGE RED", 255, 0, 127},
  {L"ORCHID", 219, 112, 219},
  {L"PALE GREEN", 143, 188, 143},
  {L"PINK", 255, 192, 203},
  {L"PLUM", 234, 173, 234},
  {L"PURPLE", 176, 0, 255},
  {L"RED", 255, 0, 0},
  {L"SALMON", 111, 66, 66},
  {L"SEA GREEN", 35, 142, 107},
  {L"SIENNA", 142, 107, 35},
  {L"SKY BLUE", 50, 153, 204},
  {L"SLATE BLUE", 0, 127, 255},
  {L"SPRING GREEN", 0, 255, 127},
  {L"STEEL BLUE", 35, 107, 142},
  {L"TAN", 219, 147, 112},
  {L"THISTLE", 216, 191, 216},
  {L"TURQUOISE", 173, 234, 234},
  {L"VIOLET", 79, 47, 79},
  {L"VIOLET RED", 204, 50, 153},
  {L"WHEAT", 216, 216, 191},
  {L"WHITE", 255, 255, 255},
  {L"YELLOW", 255, 255, 0},
  {L"YELLOW GREEN", 153, 204, 50},
};

/// Look up a color name as wxColourDatabase::Find did: case-insensitively, with "gray" also as "grey"
static optional<Color> find_named_color(const String& name) {
  String upper = name;
  for (Char& c : upper) c = toUpper(c);
  String alt = upper;
  bool has_alt = false;
  for (size_t pos = alt.find(L"GRAY") ; pos != String::npos ; pos = alt.find(L"GRAY", pos + 4)) {
    alt.replace(pos, 4, L"GREY");
    has_alt = true;
  }
  for (auto const& c : named_colors) {
    if (upper == c.name) return Color(c.r, c.g, c.b);
  }
  if (has_alt) {
    for (auto const& c : named_colors) {
      if (alt == c.name) return Color(c.r, c.g, c.b);
    }
  }
  return nullopt;
}

// ----------------------------------------------------------------------------- : Parsing etc.

template <> void Reader::handle(Color& out) {
  String const& str = getValue();
  auto col = parse_color(str);
  if (!col.has_value()) {
    out = Color();
    warning(_("Not a valid color value: ") + str);
  } else {
    out = *col;
  }
}

template <> void Writer::handle(const Color& col) {
  handle(format_color(col));
}

int parse_hex(Char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}
int parse_hex(Char c1, Char c2) {
  int x1 = parse_hex(c1);
  int x2 = parse_hex(c2);
  if (x1 >= 0 && x2 >= 0) return 16*x1 + x2;
  return -1;
}

optional<Color> parse_color(const String& v) {
  UInt r,g,b,a;
  if (swscanf(v.c_str(),_("rgb(%u,%u,%u)"),&r,&g,&b)) {
    return Color(r, g, b);
  } else if (swscanf(v.c_str(),_("rgba(%u,%u,%u,%u)"),&r,&g,&b,&a)) {
    return Color(r, g, b, a);
  } else if (v.size() > 0 && v[0] == '#') {
    if (v.size() == 4) {
      int r = parse_hex(v[1]), g = parse_hex(v[2]), b = parse_hex(v[3]);
      if (r >= 0 && g >= 0 && b >= 0) return Color(17 * r, 17 * g, 17 * b);
    } else if (v.size() == 5) {
      int r = parse_hex(v[1]), g = parse_hex(v[2]), b = parse_hex(v[3]), a = parse_hex(v[4]);
      if (r >= 0 && g >= 0 && b >= 0 && a >= 0) return Color(17 * r, 17 * g, 17 * b, 17 * a);
    } else if (v.size() == 7) {
      int r = parse_hex(v[1], v[2]), g = parse_hex(v[3],v[4]), b = parse_hex(v[5],v[6]);
      if (r >= 0 && g >= 0 && b >= 0) return Color(r, g, b);
    } else if (v.size() == 9) {
      int r = parse_hex(v[1], v[2]), g = parse_hex(v[3],v[4]), b = parse_hex(v[5],v[6]), a = parse_hex(v[7],v[8]);
      if (r >= 0 && g >= 0 && b >= 0 && a >= 0) return Color(r, g, b, a);
    }
    return nullopt;
  } else if (v == _("transparent")) {
    return Color(0,0,0,0);
  } else if (v == _("silver")) {
    return Color(200,200,200);
  } else if (v == _("gold")) {
    return Color(200,180,0);
  } else {
    // Try to find a named color
    return find_named_color(v);
  }
}

function<Color(UIColor)> ui_color_hook;

Color ui_color(UIColor which) {
  if (ui_color_hook) return ui_color_hook(which);
  switch (which) {
    case UIColor::WINDOW:        return Color(255,255,255);
    case UIColor::WINDOW_TEXT:   return Color(0,0,0);
    case UIColor::GRAY_TEXT:     return Color(128,128,128);
    case UIColor::APP_WORKSPACE: return Color(171,171,171);
  }
  return Color(0,0,0);
}

String format_color(Color col) {
  if (col.Alpha() == 255) {
    return format_string(_("rgb(%u,%u,%u)"), col.Red(), col.Green(), col.Blue());
  } else {
    return format_string(_("rgba(%u,%u,%u,%u)"), col.Red(), col.Green(), col.Blue(), col.Alpha());
  }
}

// ----------------------------------------------------------------------------- : Color utility functions

Color lerp(Color a, Color b, double t) {
  return Color(static_cast<int>( a.Red()   + (b.Red()   - a.Red()  ) * t ),
               static_cast<int>( a.Green() + (b.Green() - a.Green()) * t ),
               static_cast<int>( a.Blue()  + (b.Blue()  - a.Blue() ) * t ),
               static_cast<int>( a.Alpha() + (b.Alpha() - a.Alpha()) * t ));
}


int hsl2rgbp(double t1, double t2, double t3) {
  // adjust t3 to [0...1)
  if      (t3 < 0.0) t3 += 1;
  else if (t3 > 1.0) t3 -= 1;
  // determine color
  if (6.0 * t3 < 1) return (int)(255 * (t1 + (t2-t1) * 6.0 * t3)             );
  if (2.0 * t3 < 1) return (int)(255 * (t2)                                  );
  if (3.0 * t3 < 2) return (int)(255 * (t1 + (t2-t1) * 6.0 * (2.0/3.0 - t3)) );
  else              return (int)(255 * (t1)                                  );
}
Color hsl2rgb(double h, double s, double l) {
  double t2 = l < 0.5 ? l * (1.0 + s) :
                        l * (1.0 - s) + s;
  double t1 = 2.0 * l - t2;
  return Color(
    hsl2rgbp(t1, t2, h + 1.0/3.0),
    hsl2rgbp(t1, t2, h)          ,
    hsl2rgbp(t1, t2, h - 1.0/3.0)
  );
}


Color darken(Color c) {
  return Color(
    c.r * 8 / 10,
    c.g * 8 / 10,
    c.b * 8 / 10,
    c.a
  );
}

Color saturate(Color c, double amount) {
  double l = (c.r + c.g + c.b) / 3;
  return Color(
    col(static_cast<int>( (c.r - amount * l) / (1 - amount) )),
    col(static_cast<int>( (c.g - amount * l) / (1 - amount) )),
    col(static_cast<int>( (c.b - amount * l) / (1 - amount) )),
    c.a
  );
}


void fill_image(Image& image, RGB x) {
  RGB* pos = (RGB*)image.GetData();
  RGB* end = pos + image.GetWidth() * image.GetHeight();
  if (x.r == x.g && x.r == x.b) {
    // optimization: use memset
    memset(pos, x.r, (end-pos) * sizeof(*pos));
  } else {
    // fill the image
    while (pos != end) {
      *pos++ = x;
    }
  }
}
