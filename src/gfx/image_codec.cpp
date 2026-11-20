//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gfx/image_codec.hpp>
#include <util/error.hpp>

// ----------------------------------------------------------------------------- : ImageCodec

static unique_ptr<ImageCodec> the_image_codec;

ImageCodec& image_codec() {
  if (!the_image_codec) throw InternalError(_("No image codec installed"));
  return *the_image_codec;
}

void set_image_codec(unique_ptr<ImageCodec> codec) {
  the_image_codec = move(codec);
}

// ----------------------------------------------------------------------------- : Saving

bool image_save_file(const Image& image, const String& filename) {
  // the text after the last '.', or the whole name if there is none
  size_t dot = filename.find_last_of(L'.');
  String extension = to_lower(dot == String::npos ? filename : filename.substr(dot + 1));
  return image_save_file(image, filename, extension);
}

bool image_save_file(const Image& image, const String& filename, const String& extension) {
  if (!image.IsOk()) return false;
  ImageCodec& codec = image_codec();
  if (!codec.canEncode(extension)) return false;
  // encode in memory first, so a failure doesn't leave a broken file behind
  StringOutputStream encoded;
  if (!codec.encode(image, extension, encoded)) return false;
  FileOutputStream out(filename);
  if (!out.ok()) return false;
  bool written = out.write(encoded.str());
  return out.close() && written;
}
