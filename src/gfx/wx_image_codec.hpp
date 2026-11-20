//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

/** @file gfx/wx_image_codec.hpp
 *  WX BOUNDARY: the ImageCodec implemented with wxWidgets' image handlers (desktop host).
 *  Requires wxInitAllImageHandlers() to have been called.
 */

// ----------------------------------------------------------------------------- : Includes

#include <gfx/image_codec.hpp>

// ----------------------------------------------------------------------------- : WxImageCodec

/// Make an image codec that uses wxImage's handlers
unique_ptr<ImageCodec> make_wx_image_codec();
