// Mobile Set Editor - integer points, sizes and rectangles
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

/** @file util/geometry.hpp
 *
 *  @brief Integer geometry in device pixels: the core's replacements for wxPoint, wxSize and wxRect.
 *
 *  They keep the wx classes' layout, member names and semantics (a size's components are x and y,
 *  a rectangle's right edge is x + width - 1), so the code using them reads as before.
 *
 *  The core is wx-free (#43), but the desktop program passes these to and from wx. So each type
 *  declares conversions from and to its wx counterpart, with the wx classes only forward
 *  declared: the class definitions are the same with and without wx, and the conversions are
 *  defined by the wx backend (gfx/wx_conversions.cpp). A build without wx never calls them.
 */

class wxPoint;
class wxSize;
class wxRect;

/// A point in device pixels
struct IntPoint {
  int x = 0, y = 0;

  constexpr IntPoint() {}
  constexpr IntPoint(int x, int y) : x(x), y(y) {}
  IntPoint(const wxPoint& p);    ///< (defined by the wx backend)
  operator wxPoint() const;      ///< (defined by the wx backend)

  inline bool operator == (const IntPoint& p) const { return x == p.x && y == p.y; }
  inline bool operator != (const IntPoint& p) const { return !(*this == p); }
  inline IntPoint operator + (const IntPoint& p) const { return IntPoint(x + p.x, y + p.y); }
  inline IntPoint operator - (const IntPoint& p) const { return IntPoint(x - p.x, y - p.y); }
  inline IntPoint operator - () const { return IntPoint(-x, -y); }
  inline IntPoint& operator += (const IntPoint& p) { x += p.x; y += p.y; return *this; }
  inline IntPoint& operator -= (const IntPoint& p) { x -= p.x; y -= p.y; return *this; }
};

/// A size in device pixels (the width is x, the height is y, as in wxSize)
struct IntSize {
  int x = 0, y = 0;

  constexpr IntSize() {}
  constexpr IntSize(int width, int height) : x(width), y(height) {}
  IntSize(const wxSize& s);      ///< (defined by the wx backend)
  operator wxSize() const;       ///< (defined by the wx backend)

  inline int GetWidth() const  { return x; }
  inline int GetHeight() const { return y; }
  inline void SetWidth(int w)  { x = w; }
  inline void SetHeight(int h) { y = h; }

  inline bool operator == (const IntSize& s) const { return x == s.x && y == s.y; }
  inline bool operator != (const IntSize& s) const { return !(*this == s); }
  inline IntSize operator + (const IntSize& s) const { return IntSize(x + s.x, y + s.y); }
  inline IntSize operator - (const IntSize& s) const { return IntSize(x - s.x, y - s.y); }
  inline IntSize operator * (int f) const { return IntSize(x * f, y * f); }
  inline IntSize operator / (int f) const { return IntSize(x / f, y / f); }
};

/// A rectangle in device pixels
struct IntRect {
  int x = 0, y = 0, width = 0, height = 0;

  constexpr IntRect() {}
  constexpr IntRect(int x, int y, int width, int height) : x(x), y(y), width(width), height(height) {}
  constexpr IntRect(const IntPoint& pos, const IntSize& size) : x(pos.x), y(pos.y), width(size.x), height(size.y) {}
  IntRect(const wxRect& r);      ///< (defined by the wx backend)
  operator wxRect() const;       ///< (defined by the wx backend)

  inline int GetX() const      { return x; }
  inline int GetY() const      { return y; }
  inline int GetWidth() const  { return width; }
  inline int GetHeight() const { return height; }
  inline int GetLeft() const   { return x; }
  inline int GetTop() const    { return y; }
  inline int GetRight() const  { return x + width - 1; }
  inline int GetBottom() const { return y + height - 1; }
  inline IntPoint GetPosition() const { return IntPoint(x, y); }
  inline IntSize  GetSize() const     { return IntSize(width, height); }
  inline bool IsEmpty() const { return width <= 0 || height <= 0; }

  inline bool Contains(int px, int py) const { return px >= x && py >= y && px < x + width && py < y + height; }
  inline bool Contains(const IntPoint& p) const { return Contains(p.x, p.y); }
  /// Does this rectangle overlap another one?
  inline bool Intersects(const IntRect& r) const {
    return !Intersect(r).IsEmpty();
  }
  /// The overlap of two rectangles (empty if they don't overlap)
  inline IntRect Intersect(const IntRect& r) const {
    int l = x > r.x ? x : r.x, t = y > r.y ? y : r.y;
    int rr = x + width < r.x + r.width ? x + width : r.x + r.width;
    int bb = y + height < r.y + r.height ? y + height : r.y + r.height;
    return rr > l && bb > t ? IntRect(l, t, rr - l, bb - t) : IntRect();
  }

  inline bool operator == (const IntRect& r) const { return x == r.x && y == r.y && width == r.width && height == r.height; }
  inline bool operator != (const IntRect& r) const { return !(*this == r); }
};
