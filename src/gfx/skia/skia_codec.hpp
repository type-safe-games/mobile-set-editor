// Mobile Set Editor - PNG and JPEG through Skia, in plain std types
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

/** @file gfx/skia/skia_codec.hpp
 *  Decoding and encoding with Skia, behind an interface of std types only.
 *
 *  Skia's headers need C++20, and the core (with `using namespace std` and its own
 *  format_string) is C++17. So skia_codec.cpp is the only file that includes Skia headers and no
 *  MSE headers; it is compiled as C++20 in its own library. The ImageCodec over it is
 *  gfx/skia/skia_image_codec.hpp.
 */

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mse_skia {

/// A decoded image: unpremultiplied RGBA, rows top to bottom, no padding
struct DecodedImage {
  int width = 0, height = 0;
  std::vector<std::uint8_t> rgba;
  std::vector<std::uint8_t> palette;  ///< PLTE of a palette PNG (r,g,b triples), else empty
  bool has_description = false;      ///< did a PNG have a tEXt "Description" chunk?
  std::string description;            ///< its text, as stored (bytes)
};

/// Decode a PNG or JPEG file, as stored: no colour profile or gamma is applied
bool decode(const void* data, std::size_t size, DecodedImage& out);

/// Encode unpremultiplied RGBA (width*height*4 bytes) as PNG; RGB when !has_alpha
/** With a description, it is written in a tEXt "Description" chunk. */
bool encode_png(const std::uint8_t* rgba, int width, int height, bool has_alpha,
                const std::string* description, std::vector<std::uint8_t>& out);

/// Encode RGBA (alpha ignored) as a JPEG of the given quality (0-100)
bool encode_jpeg(const std::uint8_t* rgba, int width, int height, int quality, std::vector<std::uint8_t>& out);

} // namespace mse_skia
