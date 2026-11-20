//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gfx/canvas.hpp>
#include <data/format/image_encoding.hpp>
#include <gfx/gfx.hpp>
#include <util/reflect.hpp>
#include <algorithm>

using namespace std;

// ----------------------------------------------------------------------------- : Reflection for combining modes

String combine_to_string(const ImageCombine& combine) {
  switch (combine) {
    case COMBINE_NORMAL:              return _("normal");
    case COMBINE_ADD:                 return _("add");
    case COMBINE_SUBTRACT:            return _("subtract");
    case COMBINE_AVERAGE:             return _("average");
    case COMBINE_STAMP:               return _("stamp");
    case COMBINE_DIFFERENCE:          return _("difference");
    case COMBINE_NEGATION:            return _("negation");
    case COMBINE_MULTIPLY:            return _("multiply");
    case COMBINE_DARKEN:              return _("darken");
    case COMBINE_LIGHTEN:             return _("lighten");
    case COMBINE_COLOR_DODGE:         return _("color dodge");
    case COMBINE_COLOR_BURN:          return _("color burn");
    case COMBINE_SCREEN:              return _("screen");
    case COMBINE_OVERLAY:             return _("overlay");
    case COMBINE_HARD_LIGHT:          return _("hard light");
    case COMBINE_SOFT_LIGHT:          return _("soft light");
    case COMBINE_REFLECT:             return _("reflect");
    case COMBINE_GLOW:                return _("glow");
    case COMBINE_FREEZE:              return _("freeze");
    case COMBINE_HEAT:                return _("heat");
    case COMBINE_AND:                 return _("and");
    case COMBINE_OR:                  return _("or");
    case COMBINE_XOR:                 return _("xor");
    case COMBINE_SHADOW:              return _("shadow");
    case COMBINE_LIGHTMAP_ABSOLUTE:   return _("lightmap absolute");
    case COMBINE_LIGHTMAP_RELATIVE:   return _("lightmap relative");
    case COMBINE_SYMMETRIC_OVERLAY:   return _("symmetric overlay");
    case COMBINE_BRIGHTNESS_TO_ALPHA: return _("brightness to alpha");
    case COMBINE_DARKNESS_TO_ALPHA:   return _("darkness to alpha");
    case COMBINE_GREATER_THAN_5:      return _("greater than 5");
    case COMBINE_GREATER_THAN_10:     return _("greater than 10");
    case COMBINE_GREATER_THAN_15:     return _("greater than 15");
    case COMBINE_GREATER_THAN_20:     return _("greater than 20");
    case COMBINE_GREATER_THAN_25:     return _("greater than 25");
    case COMBINE_GREATER_THAN_30:     return _("greater than 30");
    case COMBINE_GREATER_THAN_35:     return _("greater than 35");
    case COMBINE_GREATER_THAN_40:     return _("greater than 40");
    case COMBINE_GREATER_THAN_45:     return _("greater than 45");
    case COMBINE_GREATER_THAN_50:     return _("greater than 50");
    case COMBINE_GREATER_THAN_55:     return _("greater than 55");
    case COMBINE_GREATER_THAN_60:     return _("greater than 60");
    case COMBINE_GREATER_THAN_65:     return _("greater than 65");
    case COMBINE_GREATER_THAN_70:     return _("greater than 70");
    case COMBINE_GREATER_THAN_75:     return _("greater than 75");
    case COMBINE_GREATER_THAN_80:     return _("greater than 80");
    case COMBINE_GREATER_THAN_85:     return _("greater than 85");
    case COMBINE_GREATER_THAN_90:     return _("greater than 90");
    case COMBINE_GREATER_THAN_95:     return _("greater than 95");
    case COMBINE_GREATER_THAN_100:    return _("greater than 100");
    case COMBINE_GREATER_THAN_105:    return _("greater than 105");
    case COMBINE_GREATER_THAN_110:    return _("greater than 110");
    case COMBINE_GREATER_THAN_115:    return _("greater than 115");
    case COMBINE_GREATER_THAN_120:    return _("greater than 120");
    case COMBINE_GREATER_THAN_125:    return _("greater than 125");
    case COMBINE_GREATER_THAN_130:    return _("greater than 130");
    case COMBINE_GREATER_THAN_135:    return _("greater than 135");
    case COMBINE_GREATER_THAN_140:    return _("greater than 140");
    case COMBINE_GREATER_THAN_145:    return _("greater than 145");
    case COMBINE_GREATER_THAN_150:    return _("greater than 150");
    case COMBINE_GREATER_THAN_155:    return _("greater than 155");
    case COMBINE_GREATER_THAN_160:    return _("greater than 160");
    case COMBINE_GREATER_THAN_165:    return _("greater than 165");
    case COMBINE_GREATER_THAN_170:    return _("greater than 170");
    case COMBINE_GREATER_THAN_175:    return _("greater than 175");
    case COMBINE_GREATER_THAN_180:    return _("greater than 180");
    case COMBINE_GREATER_THAN_185:    return _("greater than 185");
    case COMBINE_GREATER_THAN_190:    return _("greater than 190");
    case COMBINE_GREATER_THAN_195:    return _("greater than 195");
    case COMBINE_GREATER_THAN_200:    return _("greater than 200");
    case COMBINE_GREATER_THAN_205:    return _("greater than 205");
    case COMBINE_GREATER_THAN_210:    return _("greater than 210");
    case COMBINE_GREATER_THAN_215:    return _("greater than 215");
    case COMBINE_GREATER_THAN_220:    return _("greater than 220");
    case COMBINE_GREATER_THAN_225:    return _("greater than 225");
    case COMBINE_GREATER_THAN_230:    return _("greater than 230");
    case COMBINE_GREATER_THAN_235:    return _("greater than 235");
    case COMBINE_GREATER_THAN_240:    return _("greater than 240");
    case COMBINE_GREATER_THAN_245:    return _("greater than 245");
    case COMBINE_GREATER_THAN_250:    return _("greater than 250");
    case COMBINE_SMALLER_THAN_5:      return _("smaller than 5");
    case COMBINE_SMALLER_THAN_10:     return _("smaller than 10");
    case COMBINE_SMALLER_THAN_15:     return _("smaller than 15");
    case COMBINE_SMALLER_THAN_20:     return _("smaller than 20");
    case COMBINE_SMALLER_THAN_25:     return _("smaller than 25");
    case COMBINE_SMALLER_THAN_30:     return _("smaller than 30");
    case COMBINE_SMALLER_THAN_35:     return _("smaller than 35");
    case COMBINE_SMALLER_THAN_40:     return _("smaller than 40");
    case COMBINE_SMALLER_THAN_45:     return _("smaller than 45");
    case COMBINE_SMALLER_THAN_50:     return _("smaller than 50");
    case COMBINE_SMALLER_THAN_55:     return _("smaller than 55");
    case COMBINE_SMALLER_THAN_60:     return _("smaller than 60");
    case COMBINE_SMALLER_THAN_65:     return _("smaller than 65");
    case COMBINE_SMALLER_THAN_70:     return _("smaller than 70");
    case COMBINE_SMALLER_THAN_75:     return _("smaller than 75");
    case COMBINE_SMALLER_THAN_80:     return _("smaller than 80");
    case COMBINE_SMALLER_THAN_85:     return _("smaller than 85");
    case COMBINE_SMALLER_THAN_90:     return _("smaller than 90");
    case COMBINE_SMALLER_THAN_95:     return _("smaller than 95");
    case COMBINE_SMALLER_THAN_100:    return _("smaller than 100");
    case COMBINE_SMALLER_THAN_105:    return _("smaller than 105");
    case COMBINE_SMALLER_THAN_110:    return _("smaller than 110");
    case COMBINE_SMALLER_THAN_115:    return _("smaller than 115");
    case COMBINE_SMALLER_THAN_120:    return _("smaller than 120");
    case COMBINE_SMALLER_THAN_125:    return _("smaller than 125");
    case COMBINE_SMALLER_THAN_130:    return _("smaller than 130");
    case COMBINE_SMALLER_THAN_135:    return _("smaller than 135");
    case COMBINE_SMALLER_THAN_140:    return _("smaller than 140");
    case COMBINE_SMALLER_THAN_145:    return _("smaller than 145");
    case COMBINE_SMALLER_THAN_150:    return _("smaller than 150");
    case COMBINE_SMALLER_THAN_155:    return _("smaller than 155");
    case COMBINE_SMALLER_THAN_160:    return _("smaller than 160");
    case COMBINE_SMALLER_THAN_165:    return _("smaller than 165");
    case COMBINE_SMALLER_THAN_170:    return _("smaller than 170");
    case COMBINE_SMALLER_THAN_175:    return _("smaller than 175");
    case COMBINE_SMALLER_THAN_180:    return _("smaller than 180");
    case COMBINE_SMALLER_THAN_185:    return _("smaller than 185");
    case COMBINE_SMALLER_THAN_190:    return _("smaller than 190");
    case COMBINE_SMALLER_THAN_195:    return _("smaller than 195");
    case COMBINE_SMALLER_THAN_200:    return _("smaller than 200");
    case COMBINE_SMALLER_THAN_205:    return _("smaller than 205");
    case COMBINE_SMALLER_THAN_210:    return _("smaller than 210");
    case COMBINE_SMALLER_THAN_215:    return _("smaller than 215");
    case COMBINE_SMALLER_THAN_220:    return _("smaller than 220");
    case COMBINE_SMALLER_THAN_225:    return _("smaller than 225");
    case COMBINE_SMALLER_THAN_230:    return _("smaller than 230");
    case COMBINE_SMALLER_THAN_235:    return _("smaller than 235");
    case COMBINE_SMALLER_THAN_240:    return _("smaller than 240");
    case COMBINE_SMALLER_THAN_245:    return _("smaller than 245");
    case COMBINE_SMALLER_THAN_250:    return _("smaller than 250");
    default:                          return _("normal");
  }
}

IMPLEMENT_REFLECTION_ENUM(ImageCombine) {
  VALUE_N("normal",              COMBINE_NORMAL);
  VALUE_N("add",                 COMBINE_ADD);
  VALUE_N("subtract",            COMBINE_SUBTRACT);
  VALUE_N("average",             COMBINE_AVERAGE);
  VALUE_N("stamp",               COMBINE_STAMP);
  VALUE_N("difference",          COMBINE_DIFFERENCE);
  VALUE_N("negation",            COMBINE_NEGATION);
  VALUE_N("multiply",            COMBINE_MULTIPLY);
  VALUE_N("darken",              COMBINE_DARKEN);
  VALUE_N("lighten",             COMBINE_LIGHTEN);
  VALUE_N("color dodge",         COMBINE_COLOR_DODGE);
  VALUE_N("color burn",          COMBINE_COLOR_BURN);
  VALUE_N("screen",              COMBINE_SCREEN);
  VALUE_N("overlay",             COMBINE_OVERLAY);
  VALUE_N("hard light",          COMBINE_HARD_LIGHT);
  VALUE_N("soft light",          COMBINE_SOFT_LIGHT);
  VALUE_N("reflect",             COMBINE_REFLECT);
  VALUE_N("glow",                COMBINE_GLOW);
  VALUE_N("freeze",              COMBINE_FREEZE);
  VALUE_N("heat",                COMBINE_HEAT);
  VALUE_N("and",                 COMBINE_AND);
  VALUE_N("or",                  COMBINE_OR);
  VALUE_N("xor",                 COMBINE_XOR);
  VALUE_N("shadow",              COMBINE_SHADOW);
  VALUE_N("lightmap absolute",   COMBINE_LIGHTMAP_ABSOLUTE);
  VALUE_N("lightmap relative",   COMBINE_LIGHTMAP_RELATIVE);
  VALUE_N("symmetric overlay",   COMBINE_SYMMETRIC_OVERLAY);
  VALUE_N("brightness to alpha", COMBINE_BRIGHTNESS_TO_ALPHA);
  VALUE_N("darkness to alpha",   COMBINE_DARKNESS_TO_ALPHA);
  VALUE_N("greater than 5",      COMBINE_GREATER_THAN_5);
  VALUE_N("greater than 10",     COMBINE_GREATER_THAN_10);
  VALUE_N("greater than 15",     COMBINE_GREATER_THAN_15);
  VALUE_N("greater than 20",     COMBINE_GREATER_THAN_20);
  VALUE_N("greater than 25",     COMBINE_GREATER_THAN_25);
  VALUE_N("greater than 30",     COMBINE_GREATER_THAN_30);
  VALUE_N("greater than 35",     COMBINE_GREATER_THAN_35);
  VALUE_N("greater than 40",     COMBINE_GREATER_THAN_40);
  VALUE_N("greater than 45",     COMBINE_GREATER_THAN_45);
  VALUE_N("greater than 50",     COMBINE_GREATER_THAN_50);
  VALUE_N("greater than 55",     COMBINE_GREATER_THAN_55);
  VALUE_N("greater than 60",     COMBINE_GREATER_THAN_60);
  VALUE_N("greater than 65",     COMBINE_GREATER_THAN_65);
  VALUE_N("greater than 70",     COMBINE_GREATER_THAN_70);
  VALUE_N("greater than 75",     COMBINE_GREATER_THAN_75);
  VALUE_N("greater than 80",     COMBINE_GREATER_THAN_80);
  VALUE_N("greater than 85",     COMBINE_GREATER_THAN_85);
  VALUE_N("greater than 90",     COMBINE_GREATER_THAN_90);
  VALUE_N("greater than 95",     COMBINE_GREATER_THAN_95);
  VALUE_N("greater than 100",    COMBINE_GREATER_THAN_100);
  VALUE_N("greater than 105",    COMBINE_GREATER_THAN_105);
  VALUE_N("greater than 110",    COMBINE_GREATER_THAN_110);
  VALUE_N("greater than 115",    COMBINE_GREATER_THAN_115);
  VALUE_N("greater than 120",    COMBINE_GREATER_THAN_120);
  VALUE_N("greater than 125",    COMBINE_GREATER_THAN_125);
  VALUE_N("greater than 130",    COMBINE_GREATER_THAN_130);
  VALUE_N("greater than 135",    COMBINE_GREATER_THAN_135);
  VALUE_N("greater than 140",    COMBINE_GREATER_THAN_140);
  VALUE_N("greater than 145",    COMBINE_GREATER_THAN_145);
  VALUE_N("greater than 150",    COMBINE_GREATER_THAN_150);
  VALUE_N("greater than 155",    COMBINE_GREATER_THAN_155);
  VALUE_N("greater than 160",    COMBINE_GREATER_THAN_160);
  VALUE_N("greater than 165",    COMBINE_GREATER_THAN_165);
  VALUE_N("greater than 170",    COMBINE_GREATER_THAN_170);
  VALUE_N("greater than 175",    COMBINE_GREATER_THAN_175);
  VALUE_N("greater than 180",    COMBINE_GREATER_THAN_180);
  VALUE_N("greater than 185",    COMBINE_GREATER_THAN_185);
  VALUE_N("greater than 190",    COMBINE_GREATER_THAN_190);
  VALUE_N("greater than 195",    COMBINE_GREATER_THAN_195);
  VALUE_N("greater than 200",    COMBINE_GREATER_THAN_200);
  VALUE_N("greater than 205",    COMBINE_GREATER_THAN_205);
  VALUE_N("greater than 210",    COMBINE_GREATER_THAN_210);
  VALUE_N("greater than 215",    COMBINE_GREATER_THAN_215);
  VALUE_N("greater than 220",    COMBINE_GREATER_THAN_220);
  VALUE_N("greater than 225",    COMBINE_GREATER_THAN_225);
  VALUE_N("greater than 230",    COMBINE_GREATER_THAN_230);
  VALUE_N("greater than 235",    COMBINE_GREATER_THAN_235);
  VALUE_N("greater than 240",    COMBINE_GREATER_THAN_240);
  VALUE_N("greater than 245",    COMBINE_GREATER_THAN_245);
  VALUE_N("greater than 250",    COMBINE_GREATER_THAN_250);
  VALUE_N("smaller than 5",      COMBINE_SMALLER_THAN_5);
  VALUE_N("smaller than 10",     COMBINE_SMALLER_THAN_10);
  VALUE_N("smaller than 15",     COMBINE_SMALLER_THAN_15);
  VALUE_N("smaller than 20",     COMBINE_SMALLER_THAN_20);
  VALUE_N("smaller than 25",     COMBINE_SMALLER_THAN_25);
  VALUE_N("smaller than 30",     COMBINE_SMALLER_THAN_30);
  VALUE_N("smaller than 35",     COMBINE_SMALLER_THAN_35);
  VALUE_N("smaller than 40",     COMBINE_SMALLER_THAN_40);
  VALUE_N("smaller than 45",     COMBINE_SMALLER_THAN_45);
  VALUE_N("smaller than 50",     COMBINE_SMALLER_THAN_50);
  VALUE_N("smaller than 55",     COMBINE_SMALLER_THAN_55);
  VALUE_N("smaller than 60",     COMBINE_SMALLER_THAN_60);
  VALUE_N("smaller than 65",     COMBINE_SMALLER_THAN_65);
  VALUE_N("smaller than 70",     COMBINE_SMALLER_THAN_70);
  VALUE_N("smaller than 75",     COMBINE_SMALLER_THAN_75);
  VALUE_N("smaller than 80",     COMBINE_SMALLER_THAN_80);
  VALUE_N("smaller than 85",     COMBINE_SMALLER_THAN_85);
  VALUE_N("smaller than 90",     COMBINE_SMALLER_THAN_90);
  VALUE_N("smaller than 95",     COMBINE_SMALLER_THAN_95);
  VALUE_N("smaller than 100",    COMBINE_SMALLER_THAN_100);
  VALUE_N("smaller than 105",    COMBINE_SMALLER_THAN_105);
  VALUE_N("smaller than 110",    COMBINE_SMALLER_THAN_110);
  VALUE_N("smaller than 115",    COMBINE_SMALLER_THAN_115);
  VALUE_N("smaller than 120",    COMBINE_SMALLER_THAN_120);
  VALUE_N("smaller than 125",    COMBINE_SMALLER_THAN_125);
  VALUE_N("smaller than 130",    COMBINE_SMALLER_THAN_130);
  VALUE_N("smaller than 135",    COMBINE_SMALLER_THAN_135);
  VALUE_N("smaller than 140",    COMBINE_SMALLER_THAN_140);
  VALUE_N("smaller than 145",    COMBINE_SMALLER_THAN_145);
  VALUE_N("smaller than 150",    COMBINE_SMALLER_THAN_150);
  VALUE_N("smaller than 155",    COMBINE_SMALLER_THAN_155);
  VALUE_N("smaller than 160",    COMBINE_SMALLER_THAN_160);
  VALUE_N("smaller than 165",    COMBINE_SMALLER_THAN_165);
  VALUE_N("smaller than 170",    COMBINE_SMALLER_THAN_170);
  VALUE_N("smaller than 175",    COMBINE_SMALLER_THAN_175);
  VALUE_N("smaller than 180",    COMBINE_SMALLER_THAN_180);
  VALUE_N("smaller than 185",    COMBINE_SMALLER_THAN_185);
  VALUE_N("smaller than 190",    COMBINE_SMALLER_THAN_190);
  VALUE_N("smaller than 195",    COMBINE_SMALLER_THAN_195);
  VALUE_N("smaller than 200",    COMBINE_SMALLER_THAN_200);
  VALUE_N("smaller than 205",    COMBINE_SMALLER_THAN_205);
  VALUE_N("smaller than 210",    COMBINE_SMALLER_THAN_210);
  VALUE_N("smaller than 215",    COMBINE_SMALLER_THAN_215);
  VALUE_N("smaller than 220",    COMBINE_SMALLER_THAN_220);
  VALUE_N("smaller than 225",    COMBINE_SMALLER_THAN_225);
  VALUE_N("smaller than 230",    COMBINE_SMALLER_THAN_230);
  VALUE_N("smaller than 235",    COMBINE_SMALLER_THAN_235);
  VALUE_N("smaller than 240",    COMBINE_SMALLER_THAN_240);
  VALUE_N("smaller than 245",    COMBINE_SMALLER_THAN_245);
  VALUE_N("smaller than 250",    COMBINE_SMALLER_THAN_250);
}

// ----------------------------------------------------------------------------- : Combining functions

// Functor for combining functions for a given combining type
template <ImageCombine combine> struct Combine {
  static inline int f(int a, int b);
};

// Give a combining function for enum value 'combine'
#define COMBINE_FUN(combine,fun) \
  template <> int Combine<combine>::f(int a, int b) { return fun; }

// Based on
//  http://www.pegtop.net/delphi/articles/blendmodes/

COMBINE_FUN(COMBINE_NORMAL,              b)
COMBINE_FUN(COMBINE_ADD,                 top(a + b))
COMBINE_FUN(COMBINE_SUBTRACT,            bot(a - b))
COMBINE_FUN(COMBINE_AVERAGE,             (a + b) / 2)
COMBINE_FUN(COMBINE_STAMP,               col(a - 2 * b + 256))
COMBINE_FUN(COMBINE_DIFFERENCE,          abs(a - b))
COMBINE_FUN(COMBINE_NEGATION,            255 - abs(255 - a - b))
COMBINE_FUN(COMBINE_MULTIPLY,            (a * b) / 255)
COMBINE_FUN(COMBINE_DARKEN,              min(a, b))
COMBINE_FUN(COMBINE_LIGHTEN,             max(a, b))
COMBINE_FUN(COMBINE_COLOR_DODGE,         b == 255 ? 255 : top(a * 255 / (255 - b)))
COMBINE_FUN(COMBINE_COLOR_BURN,          b == 0   ? 0   : bot(255 - (255-a) * 255 / b))
COMBINE_FUN(COMBINE_SCREEN,              255 - (((255 - a) * (255 - b)) / 255))
COMBINE_FUN(COMBINE_OVERLAY,             a < 128 ?
                                         (a * b) >> 7 :
                                         255 - (((255 - a) * (255 - b)) >> 7))
COMBINE_FUN(COMBINE_HARD_LIGHT,          b < 128 ?
                                         (a * b) >> 7 :
                                         255 - (((255 - a) * (255 - b)) >> 7))
COMBINE_FUN(COMBINE_SOFT_LIGHT,          b)
COMBINE_FUN(COMBINE_REFLECT,             b == 255 ? 255 : top(a * a / (255 - b)))
COMBINE_FUN(COMBINE_GLOW,                a == 255 ? 255 : top(b * b / (255 - a)))
COMBINE_FUN(COMBINE_FREEZE,              b == 0 ? 0 : bot(255 - (255 - a) * (255 - a) / b))
COMBINE_FUN(COMBINE_HEAT,                a == 0 ? 0 : bot(255 - (255 - b) * (255 - b) / a))
COMBINE_FUN(COMBINE_AND,                 a & b)
COMBINE_FUN(COMBINE_OR,                  a | b)
COMBINE_FUN(COMBINE_XOR,                 a ^ b)
COMBINE_FUN(COMBINE_SHADOW,              (b * a * a) / (255 * 255))
COMBINE_FUN(COMBINE_LIGHTMAP_ABSOLUTE,   col(a + b - 128))
COMBINE_FUN(COMBINE_LIGHTMAP_RELATIVE,   b < 128 ?
                                         a * b / 128 :
                                         a + (255 - a) * (b - 128) / 128)
COMBINE_FUN(COMBINE_SYMMETRIC_OVERLAY,   (Combine<COMBINE_OVERLAY>::f(a, b) + Combine<COMBINE_OVERLAY>::f(b, a)) / 2)
COMBINE_FUN(COMBINE_BRIGHTNESS_TO_ALPHA, ((255 - a) * a + a * b) / 255)
COMBINE_FUN(COMBINE_DARKNESS_TO_ALPHA,   (255 * a + (255 - a) * b) / 255)
COMBINE_FUN(COMBINE_GREATER_THAN_5,      a > 5 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_10,     a > 10 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_15,     a > 15 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_20,     a > 20 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_25,     a > 25 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_30,     a > 30 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_35,     a > 35 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_40,     a > 40 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_45,     a > 45 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_50,     a > 50 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_55,     a > 55 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_60,     a > 60 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_65,     a > 65 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_70,     a > 70 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_75,     a > 75 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_80,     a > 80 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_85,     a > 85 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_90,     a > 90 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_95,     a > 95 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_100,    a > 100 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_105,    a > 105 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_110,    a > 110 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_115,    a > 115 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_120,    a > 120 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_125,    a > 125 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_130,    a > 130 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_135,    a > 135 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_140,    a > 140 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_145,    a > 145 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_150,    a > 150 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_155,    a > 155 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_160,    a > 160 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_165,    a > 165 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_170,    a > 170 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_175,    a > 175 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_180,    a > 180 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_185,    a > 185 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_190,    a > 190 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_195,    a > 195 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_200,    a > 200 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_205,    a > 205 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_210,    a > 210 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_215,    a > 215 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_220,    a > 220 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_225,    a > 225 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_230,    a > 230 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_235,    a > 235 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_240,    a > 240 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_245,    a > 245 ? b : a)
COMBINE_FUN(COMBINE_GREATER_THAN_250,    a > 250 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_5,      a < 5 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_10,     a < 10 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_15,     a < 15 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_20,     a < 20 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_25,     a < 25 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_30,     a < 30 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_35,     a < 35 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_40,     a < 40 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_45,     a < 45 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_50,     a < 50 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_55,     a < 55 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_60,     a < 60 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_65,     a < 65 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_70,     a < 70 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_75,     a < 75 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_80,     a < 80 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_85,     a < 85 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_90,     a < 90 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_95,     a < 95 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_100,    a < 100 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_105,    a < 105 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_110,    a < 110 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_115,    a < 115 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_120,    a < 120 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_125,    a < 125 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_130,    a < 130 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_135,    a < 135 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_140,    a < 140 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_145,    a < 145 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_150,    a < 150 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_155,    a < 155 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_160,    a < 160 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_165,    a < 165 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_170,    a < 170 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_175,    a < 175 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_180,    a < 180 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_185,    a < 185 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_190,    a < 190 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_195,    a < 195 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_200,    a < 200 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_205,    a < 205 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_210,    a < 210 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_215,    a < 215 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_220,    a < 220 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_225,    a < 225 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_230,    a < 230 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_235,    a < 235 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_240,    a < 240 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_245,    a < 245 ? b : a)
COMBINE_FUN(COMBINE_SMALLER_THAN_250,    a < 250 ? b : a)

// ----------------------------------------------------------------------------- : Combining

/// Combine image b onto image a using some combining mode.
/// The results are stored in the image A.
template <ImageCombine combine>
void combine_image_do(Image& a, Image b) {
  UInt size = a.GetWidth() * a.GetHeight() * 3;
  Byte* dataA = a.GetData(), * dataB = b.GetData();
  // for each pixel: apply function
  for (UInt i = 0; i < size; ++i) {
    dataA[i] = Combine<combine>::f(dataA[i], dataB[i]);
  }
}

void combine_image(Image& a, const Image& b, ImageCombine combine) {
  // Images must have same size
  int width = a.GetWidth(), height = a.GetHeight();
  if (b.GetWidth() != width || b.GetHeight() != height) {
    throw Error(_ERROR_3_("blending different sizes", _("combine_blend"), int_to_string(width) + _("x") + int_to_string(height), int_to_string(b.GetWidth()) + _("x") + int_to_string(b.GetHeight())));
  }
  // Copy alpha channel?
  if (b.HasAlpha()) {
    if (!a.HasAlpha()) a.InitAlpha();
    memcpy(a.GetAlpha(), b.GetAlpha(), width * height);
  }
  // Combine image data, by dispatching to combineImageDo
  switch(combine) {
    #define DISPATCH(comb) case comb: combine_image_do<comb>(a,b); return
    case COMBINE_DEFAULT:
    case COMBINE_NORMAL: a = b; return; // no need to do a per pixel operation
    DISPATCH(COMBINE_ADD);
    DISPATCH(COMBINE_SUBTRACT);
    DISPATCH(COMBINE_AVERAGE);
    DISPATCH(COMBINE_STAMP);
    DISPATCH(COMBINE_DIFFERENCE);
    DISPATCH(COMBINE_NEGATION);
    DISPATCH(COMBINE_MULTIPLY);
    DISPATCH(COMBINE_DARKEN);
    DISPATCH(COMBINE_LIGHTEN);
    DISPATCH(COMBINE_COLOR_DODGE);
    DISPATCH(COMBINE_COLOR_BURN);
    DISPATCH(COMBINE_SCREEN);
    DISPATCH(COMBINE_OVERLAY);
    DISPATCH(COMBINE_HARD_LIGHT);
    DISPATCH(COMBINE_SOFT_LIGHT);
    DISPATCH(COMBINE_REFLECT);
    DISPATCH(COMBINE_GLOW);
    DISPATCH(COMBINE_FREEZE);
    DISPATCH(COMBINE_HEAT);
    DISPATCH(COMBINE_AND);
    DISPATCH(COMBINE_OR);
    DISPATCH(COMBINE_XOR);
    DISPATCH(COMBINE_SHADOW);
    DISPATCH(COMBINE_LIGHTMAP_ABSOLUTE);
    DISPATCH(COMBINE_LIGHTMAP_RELATIVE);
    DISPATCH(COMBINE_SYMMETRIC_OVERLAY);
    DISPATCH(COMBINE_BRIGHTNESS_TO_ALPHA);
    DISPATCH(COMBINE_DARKNESS_TO_ALPHA);
    DISPATCH(COMBINE_GREATER_THAN_5);
    DISPATCH(COMBINE_GREATER_THAN_10);
    DISPATCH(COMBINE_GREATER_THAN_15);
    DISPATCH(COMBINE_GREATER_THAN_20);
    DISPATCH(COMBINE_GREATER_THAN_25);
    DISPATCH(COMBINE_GREATER_THAN_30);
    DISPATCH(COMBINE_GREATER_THAN_35);
    DISPATCH(COMBINE_GREATER_THAN_40);
    DISPATCH(COMBINE_GREATER_THAN_45);
    DISPATCH(COMBINE_GREATER_THAN_50);
    DISPATCH(COMBINE_GREATER_THAN_55);
    DISPATCH(COMBINE_GREATER_THAN_60);
    DISPATCH(COMBINE_GREATER_THAN_65);
    DISPATCH(COMBINE_GREATER_THAN_70);
    DISPATCH(COMBINE_GREATER_THAN_75);
    DISPATCH(COMBINE_GREATER_THAN_80);
    DISPATCH(COMBINE_GREATER_THAN_85);
    DISPATCH(COMBINE_GREATER_THAN_90);
    DISPATCH(COMBINE_GREATER_THAN_95);
    DISPATCH(COMBINE_GREATER_THAN_100);
    DISPATCH(COMBINE_GREATER_THAN_105);
    DISPATCH(COMBINE_GREATER_THAN_110);
    DISPATCH(COMBINE_GREATER_THAN_115);
    DISPATCH(COMBINE_GREATER_THAN_120);
    DISPATCH(COMBINE_GREATER_THAN_125);
    DISPATCH(COMBINE_GREATER_THAN_130);
    DISPATCH(COMBINE_GREATER_THAN_135);
    DISPATCH(COMBINE_GREATER_THAN_140);
    DISPATCH(COMBINE_GREATER_THAN_145);
    DISPATCH(COMBINE_GREATER_THAN_150);
    DISPATCH(COMBINE_GREATER_THAN_155);
    DISPATCH(COMBINE_GREATER_THAN_160);
    DISPATCH(COMBINE_GREATER_THAN_165);
    DISPATCH(COMBINE_GREATER_THAN_170);
    DISPATCH(COMBINE_GREATER_THAN_175);
    DISPATCH(COMBINE_GREATER_THAN_180);
    DISPATCH(COMBINE_GREATER_THAN_185);
    DISPATCH(COMBINE_GREATER_THAN_190);
    DISPATCH(COMBINE_GREATER_THAN_195);
    DISPATCH(COMBINE_GREATER_THAN_200);
    DISPATCH(COMBINE_GREATER_THAN_205);
    DISPATCH(COMBINE_GREATER_THAN_210);
    DISPATCH(COMBINE_GREATER_THAN_215);
    DISPATCH(COMBINE_GREATER_THAN_220);
    DISPATCH(COMBINE_GREATER_THAN_225);
    DISPATCH(COMBINE_GREATER_THAN_230);
    DISPATCH(COMBINE_GREATER_THAN_235);
    DISPATCH(COMBINE_GREATER_THAN_240);
    DISPATCH(COMBINE_GREATER_THAN_245);
    DISPATCH(COMBINE_GREATER_THAN_250);
    DISPATCH(COMBINE_SMALLER_THAN_5);
    DISPATCH(COMBINE_SMALLER_THAN_10);
    DISPATCH(COMBINE_SMALLER_THAN_15);
    DISPATCH(COMBINE_SMALLER_THAN_20);
    DISPATCH(COMBINE_SMALLER_THAN_25);
    DISPATCH(COMBINE_SMALLER_THAN_30);
    DISPATCH(COMBINE_SMALLER_THAN_35);
    DISPATCH(COMBINE_SMALLER_THAN_40);
    DISPATCH(COMBINE_SMALLER_THAN_45);
    DISPATCH(COMBINE_SMALLER_THAN_50);
    DISPATCH(COMBINE_SMALLER_THAN_55);
    DISPATCH(COMBINE_SMALLER_THAN_60);
    DISPATCH(COMBINE_SMALLER_THAN_65);
    DISPATCH(COMBINE_SMALLER_THAN_70);
    DISPATCH(COMBINE_SMALLER_THAN_75);
    DISPATCH(COMBINE_SMALLER_THAN_80);
    DISPATCH(COMBINE_SMALLER_THAN_85);
    DISPATCH(COMBINE_SMALLER_THAN_90);
    DISPATCH(COMBINE_SMALLER_THAN_95);
    DISPATCH(COMBINE_SMALLER_THAN_100);
    DISPATCH(COMBINE_SMALLER_THAN_105);
    DISPATCH(COMBINE_SMALLER_THAN_110);
    DISPATCH(COMBINE_SMALLER_THAN_115);
    DISPATCH(COMBINE_SMALLER_THAN_120);
    DISPATCH(COMBINE_SMALLER_THAN_125);
    DISPATCH(COMBINE_SMALLER_THAN_130);
    DISPATCH(COMBINE_SMALLER_THAN_135);
    DISPATCH(COMBINE_SMALLER_THAN_140);
    DISPATCH(COMBINE_SMALLER_THAN_145);
    DISPATCH(COMBINE_SMALLER_THAN_150);
    DISPATCH(COMBINE_SMALLER_THAN_155);
    DISPATCH(COMBINE_SMALLER_THAN_160);
    DISPATCH(COMBINE_SMALLER_THAN_165);
    DISPATCH(COMBINE_SMALLER_THAN_170);
    DISPATCH(COMBINE_SMALLER_THAN_175);
    DISPATCH(COMBINE_SMALLER_THAN_180);
    DISPATCH(COMBINE_SMALLER_THAN_185);
    DISPATCH(COMBINE_SMALLER_THAN_190);
    DISPATCH(COMBINE_SMALLER_THAN_195);
    DISPATCH(COMBINE_SMALLER_THAN_200);
    DISPATCH(COMBINE_SMALLER_THAN_205);
    DISPATCH(COMBINE_SMALLER_THAN_210);
    DISPATCH(COMBINE_SMALLER_THAN_215);
    DISPATCH(COMBINE_SMALLER_THAN_220);
    DISPATCH(COMBINE_SMALLER_THAN_225);
    DISPATCH(COMBINE_SMALLER_THAN_230);
    DISPATCH(COMBINE_SMALLER_THAN_235);
    DISPATCH(COMBINE_SMALLER_THAN_240);
    DISPATCH(COMBINE_SMALLER_THAN_245);
    DISPATCH(COMBINE_SMALLER_THAN_250);
  }

  //transfer metadata
  a.SetMetadata(metadata_merge(a, b));
}

void draw_combine_image(Canvas& dc, UInt x, UInt y, const Image& img, ImageCombine combine) {
  if (combine <= COMBINE_NORMAL) {
    dc.drawImage(img, x, y);
  } else {
    // Capture the current image in the target rectangle
    Image source = dc.readBack(x, y, img.GetWidth(), img.GetHeight());
    // Combine and draw
    combine_image(source, img, combine);
    dc.drawImage(source, x, y);
  }
}
