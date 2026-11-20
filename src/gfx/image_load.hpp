//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

/** @file gfx/image_load.hpp
 *  Loading images from streams, files and the program's resources.
 */

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/io/stream.hpp>

// ----------------------------------------------------------------------------- : Image loading

/// Load an image from a stream
bool image_load_file(Image& image, InputStream& stream);

/// Load an image from a file
bool image_load_file(Image& image, const wxString &name);

/// Load an image from the program's resources
Image load_resource_image(const String& name);
