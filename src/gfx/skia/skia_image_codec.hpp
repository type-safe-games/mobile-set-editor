// Mobile Set Editor - the image codec over Skia
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

/** @file gfx/skia/skia_image_codec.hpp
 *  An ImageCodec over Skia's SkCodec and encoders (ADR 0004).
 *
 *  It adapts gfx/skia/skia_codec.hpp, which does the Skia calls in plain std types, to Image.
 *  Both are only built when the build links Skia (CMake option MSE_SKIA_CODEC on the desktop).
 *
 *  Decoding gives the same Image as wxWidgets 3.2's handlers, which the Golden images were made
 *  with: RGB plus an alpha plane only when some pixel is not opaque, colours as stored (no gamma
 *  or colour profile is applied), 16 bit channels truncated to their high byte, and the palette
 *  of a palette PNG. JPEG decodes to RGB; CMYK JPEGs are read as Adobe's inverted CMYK. The PNG
 *  tEXt "Description" chunk is read into, and written from, the image's metadata (ADR 0007).
 */

#include <gfx/image_codec.hpp>

/// Make an image codec that uses Skia; it decodes PNG and JPEG and encodes PNG and JPEG
unique_ptr<ImageCodec> make_skia_image_codec();
