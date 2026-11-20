//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

/** @file gfx/dc_util.hpp
 *  Small drawing helpers shared by the renderers and the gui.
 */

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>

class RotatedDC;
class RealRect;
class Canvas;

// ----------------------------------------------------------------------------- : Drawing helpers

/// Draw a checkerboard pattern
void draw_checker(RotatedDC& dc, const RealRect&);

/// Draw a checkbox without the native renderer (as used on cards)
void draw_checkbox_portable(Canvas& dc, const wxRect& rect, bool checked, bool enabled = true);
