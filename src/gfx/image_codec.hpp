//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

/** @file gfx/image_codec.hpp
 *  Decoding and encoding image files (PNG, JPEG, ...).
 *
 *  The core only sees this interface. The host installs an implementation with set_image_codec:
 *  the desktop host uses wxWidgets' image handlers (gfx/wx_image_codec.hpp), the Android host
 *  will use Skia's SkCodec (ADR 0004).
 */

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/io/stream.hpp>

// ----------------------------------------------------------------------------- : ImageCodec

/// Decodes and encodes image files
class ImageCodec {
public:
  virtual ~ImageCodec() {}

  /// Decode an image file, detecting the format from its contents.
  /** On success the image (with its mask and options, e.g. resolution) is stored in out.
   *  On failure out is left unchanged. */
  virtual bool decode(const void* data, size_t size, Image& out) = 0;

  /// Can images be encoded in the format that goes with a (lower case) file extension, like "png" or "jpg"?
  virtual bool canEncode(const String& extension) = 0;

  /// Encode an image in the format that goes with a (lower case) file extension.
  /** The image's metadata (Image::GetMetadata) goes into the PNG description,
   *  as far as the codec supports it. */
  virtual bool encode(const Image& image, const String& extension, OutputStream& out) = 0;
};

/// The installed codec; throws an InternalError if there is none
ImageCodec& image_codec();
/// Install the codec (by the host, at startup)
void set_image_codec(unique_ptr<ImageCodec> codec);

// ----------------------------------------------------------------------------- : Saving

/// Save an image to a file, the format is determined by the file's extension
bool image_save_file(const Image& image, const String& filename);
/// Save an image to a file in the format that goes with the given (lower case) extension
bool image_save_file(const Image& image, const String& filename, const String& extension);
