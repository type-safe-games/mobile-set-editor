// Mobile Set Editor - conversions between the core's value types and wx's
// SPDX-License-Identifier: GPL-3.0-or-later

// The wx-free core (#43) declares these conversions with the wx classes only forward declared
// (util/geometry.hpp, gfx/color.hpp), so the desktop program can pass core values to wx and back.

#include <util/prec.hpp>
#include <gfx/color.hpp>
#include <util/real_point.hpp>

// ----------------------------------------------------------------------------- : Geometry

IntPoint::IntPoint(const wxPoint& p) : x(p.x), y(p.y) {}
IntPoint::operator wxPoint() const { return wxPoint(x, y); }

IntSize::IntSize(const wxSize& s) : x(s.x), y(s.y) {}
IntSize::operator wxSize() const { return wxSize(x, y); }

IntRect::IntRect(const wxRect& r) : x(r.x), y(r.y), width(r.width), height(r.height) {}
IntRect::operator wxRect() const { return wxRect(x, y, width, height); }

Vector2D::operator wxPoint() const { return wxPoint(IntPoint(*this)); }

RealSize::RealSize(const wxSize& s) : width(s.x), height(s.y) {}
RealSize::operator wxSize() { return wxSize(to_int(width), to_int(height)); }

RealRect::RealRect(const wxRect& r) : RealPoint(r.x, r.y), RealSize(r.width, r.height) {}
RealRect::operator wxRect() const { return wxRect(IntRect(*this)); }

// ----------------------------------------------------------------------------- : Colors

Color::Color(const wxColour& c) : r(c.Red()), g(c.Green()), b(c.Blue()), a(c.Alpha()) {}
Color::operator wxColour() const { return wxColour(r, g, b, a); }

RGB::RGB(const wxColour& c) : r(c.Red()), g(c.Green()), b(c.Blue()) {}
RGB::operator wxColour() const { return wxColour(r, g, b); }
