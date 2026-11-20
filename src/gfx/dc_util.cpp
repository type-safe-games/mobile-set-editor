//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gfx/dc_util.hpp>
#include <gfx/color.hpp>
#include <util/rotation.hpp>

// ----------------------------------------------------------------------------- : DC related

/// Fill a DC with a single color
void clearDC(DC& dc, const wxBrush& brush) {
  wxSize size = dc.GetSize();
  wxPoint pos = dc.GetDeviceOrigin(); // don't you love undocumented methods?
  dc.SetPen(*wxTRANSPARENT_PEN);
  dc.SetBrush(brush);
  dc.DrawRectangle(-pos.x, -pos.y, size.GetWidth(), size.GetHeight());
}

void clearDC_black(DC& dc) {
  #if !BITMAPS_DEFAULT_BLACK
    // On windows 9x it seems that bitmaps are not black by default
    clearDC(dc, *wxBLACK_BRUSH);
  #endif
}

void draw_checker(RotatedDC& dc, const RealRect& rect) {
  dc.SetPen(*wxTRANSPARENT_PEN);
  dc.SetBrush(*wxWHITE_BRUSH);
  dc.DrawRectangle(rect);
  dc.SetBrush(Color(235,235,235));
  const double checker_size = 10;
  int odd = 0;
  for (double y = 0 ; y < rect.height ; y += checker_size) {
    for (double x = odd * checker_size ; x < rect.width ; x += checker_size * 2) {
      dc.DrawRectangle(RealRect(
                rect.x + x,
                rect.y + y,
                min(checker_size, rect.width  - x),
                min(checker_size, rect.height - y)
              ));
    }
    odd = 1 - odd;
  }
}


void draw_checkbox_portable(DC& dc, const wxRect& rect, bool checked, bool enabled) {
  if (checked) {
    dc.DrawCheckMark(wxRect(rect.x+1,rect.y+1,rect.width-2,rect.height-2));
  }
  dc.SetPen(ui_color(enabled ? UIColor::WINDOW_TEXT : UIColor::GRAY_TEXT));
  dc.SetBrush(*wxTRANSPARENT_BRUSH);
  dc.DrawRectangle(rect.x, rect.y, rect.width, rect.height);
}
