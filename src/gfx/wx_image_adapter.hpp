//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

/** @file gfx/wx_image_adapter.hpp
 *  WX BOUNDARY: conversion between MSE's Image and wxImage / wxBitmap.
 *
 *  The wx canvas (gfx/wx_canvas.hpp) converts images to bitmaps to draw them, and bitmaps that
 *  were drawn on back to images; the GUI does the same for its own drawing. Together with the wx
 *  image codec (gfx/wx_image_codec.hpp) and the wx canvas, this is the only place in the core
 *  that uses wxImage.
 *  The conversions are exact: pixels, alpha, mask colour, palette and options all survive.
 */

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <wx/image.h>

#if !wxCHECK_VERSION(3,3,0)
  // wx 3.3 reads/writes a PNG tEXt "Description" chunk through this image option.
  // On 3.2 the PNG handler ignores it, so the metadata (Image::GetMetadata) still flows between
  // in-memory images but is not saved to or loaded from PNG files.
  #define wxIMAGE_OPTION_PNG_DESCRIPTION wxS("PngDescription")
#endif

// ----------------------------------------------------------------------------- : Conversion

/// Convert an image to a wxImage
wxImage to_wx_image(const Image& image);
/// Convert a wxImage to an image
Image from_wx_image(const wxImage& image);

/// Convert an image to a bitmap (as wxBitmap(wxImage))
wxBitmap to_bitmap(const Image& image);
/// Convert a bitmap to an image (as wxBitmap::ConvertToImage)
Image from_bitmap(const wxBitmap& bitmap);
