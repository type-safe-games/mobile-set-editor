// Mobile Set Editor - the image codec over Skia
// SPDX-License-Identifier: GPL-3.0-or-later

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gfx/skia/skia_image_codec.hpp>
#include <gfx/skia/skia_codec.hpp>

// ----------------------------------------------------------------------------- : Conversion

namespace {

/// The decoded pixels as an Image: an alpha plane only if some pixel isn't opaque (as wx)
Image to_image(const mse_skia::DecodedImage& decoded) {
  Image image(decoded.width, decoded.height, false);
  if (!image.IsOk()) return image;
  const size_t n = (size_t)decoded.width * decoded.height;
  const Byte* src = decoded.rgba.data();
  bool opaque = true;
  for (size_t i = 0 ; i < n ; ++i) {
    if (src[4*i+3] != 255) { opaque = false; break; }
  }
  Byte* rgb = image.GetData();
  Byte* alpha = nullptr;
  if (!opaque) {
    image.SetAlpha();
    alpha = image.GetAlpha();
  }
  for (size_t i = 0 ; i < n ; ++i) {
    rgb[3*i]   = src[4*i];
    rgb[3*i+1] = src[4*i+1];
    rgb[3*i+2] = src[4*i+2];
    if (alpha) alpha[i] = src[4*i+3];
  }
  if (!decoded.palette.empty()) image.SetPalette(decoded.palette);
  if (decoded.has_description) {
    // tEXt is Latin-1 by the PNG spec; we write UTF-8, so read that when it is valid
    image.SetMetadata(decode_utf8_or_latin1(decoded.description.data(), decoded.description.size()));
  }
  return image;
}

/// The image as unpremultiplied RGBA; a mask colour becomes transparency (as the wx PNG encoder)
vector<Byte> to_rgba(const Image& image, bool& has_alpha) {
  const size_t n = (size_t)image.GetWidth() * image.GetHeight();
  vector<Byte> rgba(n * 4);
  const Byte* rgb = image.GetData();
  const Byte* alpha = image.GetAlpha();
  const bool mask = image.HasMask();
  const Byte mr = image.GetMaskRed(), mg = image.GetMaskGreen(), mb = image.GetMaskBlue();
  has_alpha = alpha || mask;
  for (size_t i = 0 ; i < n ; ++i) {
    Byte r = rgb[3*i], g = rgb[3*i+1], b = rgb[3*i+2];
    Byte a = alpha ? alpha[i] : 255;
    if (mask && r == mr && g == mg && b == mb) a = 0;
    rgba[4*i] = r; rgba[4*i+1] = g; rgba[4*i+2] = b; rgba[4*i+3] = a;
  }
  return rgba;
}

// ----------------------------------------------------------------------------- : SkiaImageCodec

class SkiaImageCodec : public ImageCodec {
public:
  bool decode(const void* data, size_t size, Image& out) override {
    mse_skia::DecodedImage decoded;
    if (!mse_skia::decode(data, size, decoded)) return false;
    Image image = to_image(decoded);
    if (!image.IsOk()) return false;
    out = image;
    return true;
  }

  bool canEncode(const String& extension) override {
    return extension == _("png") || extension == _("jpg") || extension == _("jpeg") || extension == _("jpe");
  }

  bool encode(const Image& image, const String& extension, OutputStream& out) override {
    if (!image.IsOk() || !canEncode(extension)) return false;
    bool has_alpha;
    vector<Byte> rgba = to_rgba(image, has_alpha);
    std::vector<std::uint8_t> encoded;
    bool ok;
    if (extension == _("png")) {
      std::string description;
      if (image.HasMetadata()) description = encode_utf8(image.GetMetadata());
      ok = mse_skia::encode_png(rgba.data(), image.GetWidth(), image.GetHeight(), has_alpha,
                                image.HasMetadata() ? &description : nullptr, encoded);
    } else {
      // libjpeg's default quality, which the wx encoder also uses unless the image has the option
      int quality = image.HasOption(_("quality")) ? image.GetOptionInt(_("quality")) : 75;
      ok = mse_skia::encode_jpeg(rgba.data(), image.GetWidth(), image.GetHeight(), quality, encoded);
    }
    return ok && out.write(encoded.data(), encoded.size());
  }
};

} // namespace

unique_ptr<ImageCodec> make_skia_image_codec() {
  return make_unique<SkiaImageCodec>();
}
