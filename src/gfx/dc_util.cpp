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
#include <gfx/canvas.hpp>

// ----------------------------------------------------------------------------- : Drawing helpers

void draw_checker(RotatedDC& dc, const RealRect& rect) {
  dc.SetPen(Pen::none());
  dc.SetBrush(Color(255,255,255));
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


void draw_checkbox_portable(Canvas& dc, const IntRect& rect, bool checked, bool enabled) {
  if (checked) {
    // a tick, drawn like wxDC::DrawCheckMark does it
    int x1 = rect.x + 1, y1 = rect.y + 1, width = rect.width - 2, height = rect.height - 2;
    int x2 = x1 + width, y2 = y1 + height;
    // the pen width is calibrated to give 3 for width == height == 10
    dc.setPen(Pen(dc.getTextColor(), (width + height + 1) / 7));
    int x3 = x1 + (4 * width) / 10, // x of the tick bottom
        y3 = y1 + height / 2;       // y of the left tick branch
    dc.drawLine(x1, y3, x3, y2);
    dc.drawLine(x3, y2, x2, y1);
  }
  dc.setPen(ui_color(enabled ? UIColor::WINDOW_TEXT : UIColor::GRAY_TEXT));
  dc.setBrush(Brush::none());
  dc.drawRectangle(rect.x, rect.y, rect.width, rect.height);
}
