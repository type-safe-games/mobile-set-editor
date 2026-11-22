// Mobile Set Editor - PNG and JPEG through Skia, in plain std types
// SPDX-License-Identifier: GPL-3.0-or-later

// No MSE headers here: this file is C++20 for Skia's sake (see skia_codec.hpp).

#include <gfx/skia/skia_codec.hpp>

#include "include/codec/SkCodec.h"
#include "include/codec/SkJpegDecoder.h"
#include "include/codec/SkPngDecoder.h"
#include "include/core/SkData.h"
#include "include/core/SkDataTable.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkStream.h"
#include "include/encode/SkJpegEncoder.h"
#include "include/encode/SkPngEncoder.h"

#include <cstring>
#include <memory>

namespace mse_skia {

// ----------------------------------------------------------------------------- : PNG chunks

namespace {

const std::uint8_t png_signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};

std::uint32_t read_be32(const std::uint8_t* p) {
  return (std::uint32_t)p[0] << 24 | (std::uint32_t)p[1] << 16 | (std::uint32_t)p[2] << 8 | (std::uint32_t)p[3];
}

/// What we read from a PNG's chunks ourselves, because SkCodec doesn't expose it
struct PngChunks {
  int bit_depth = 0;
  int color_type = -1;
  std::vector<std::uint8_t> palette;
  bool has_description = false;
  std::string description;
};

/// Walk the chunks of a PNG file (up to IEND or the end of the data); false if it isn't a PNG
bool read_png_chunks(const std::uint8_t* data, std::size_t size, PngChunks& out) {
  if (size < 8 || std::memcmp(data, png_signature, 8) != 0) return false;
  std::size_t pos = 8;
  while (pos + 8 <= size) {
    std::uint32_t length = read_be32(data + pos);
    const std::uint8_t* type = data + pos + 4;
    const std::uint8_t* body = data + pos + 8;
    if (length > size - pos - 8) break; // truncated
    if (std::memcmp(type, "IHDR", 4) == 0 && length >= 13) {
      out.bit_depth = body[8];
      out.color_type = body[9];
    } else if (std::memcmp(type, "PLTE", 4) == 0) {
      out.palette.assign(body, body + (length - length % 3));
    } else if (std::memcmp(type, "tEXt", 4) == 0 && !out.has_description) {
      const char* text = (const char*)body;
      const char* nul = (const char*)std::memchr(text, 0, length);
      if (nul && std::string(text, nul) == "Description") {
        out.has_description = true;
        out.description.assign(nul + 1, text + length);
      }
    } else if (std::memcmp(type, "IEND", 4) == 0) {
      break;
    }
    pos += 12 + (std::size_t)length; // length, type, data, crc
  }
  return true;
}

/// An SkWStream into a byte vector
class VectorWStream : public SkWStream {
public:
  explicit VectorWStream(std::vector<std::uint8_t>& out) : out(out) {}
  bool write(const void* buffer, std::size_t size) override {
    const std::uint8_t* bytes = (const std::uint8_t*)buffer;
    out.insert(out.end(), bytes, bytes + size);
    return true;
  }
  std::size_t bytesWritten() const override { return out.size(); }
private:
  std::vector<std::uint8_t>& out;
};

SkPixmap rgba_pixmap(const std::uint8_t* rgba, int width, int height, SkAlphaType alpha_type) {
  SkImageInfo info = SkImageInfo::Make(width, height, kRGBA_8888_SkColorType, alpha_type, nullptr);
  return SkPixmap(info, rgba, info.minRowBytes());
}

} // namespace

// ----------------------------------------------------------------------------- : Decoding

bool decode(const void* data, std::size_t size, DecodedImage& out) {
  sk_sp<SkData> sk_data = SkData::MakeWithoutCopy(data, size);
  PngChunks png;
  const bool is_png = read_png_chunks((const std::uint8_t*)data, size, png);
  std::unique_ptr<SkCodec> codec;
  if (is_png) {
    codec = SkPngDecoder::Decode(sk_data, nullptr);
  } else if (SkJpegDecoder::IsJpeg(data, size)) {
    codec = SkJpegDecoder::Decode(sk_data, nullptr);
  }
  if (!codec) return false;

  // No colour space: Skia then applies no profile or gamma conversion
  SkISize dims = codec->dimensions();
  SkImageInfo info = SkImageInfo::Make(dims, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType, nullptr);
  DecodedImage image;
  image.width = dims.width();
  image.height = dims.height();
  image.rgba.resize(info.computeMinByteSize());
  if (codec->getPixels(info, image.rgba.data(), info.minRowBytes()) != SkCodec::kSuccess) return false;
  if (is_png) {
    if (png.color_type == 3) image.palette = std::move(png.palette);
    image.has_description = png.has_description;
    image.description = std::move(png.description);
  }
  out = std::move(image);
  return true;
}

// ----------------------------------------------------------------------------- : Encoding

bool encode_png(const std::uint8_t* rgba, int width, int height, bool has_alpha,
                const std::string* description, std::vector<std::uint8_t>& out) {
  // Skia writes RGB rather than RGBA when the pixmap is opaque
  SkPixmap pixmap = rgba_pixmap(rgba, width, height, has_alpha ? kUnpremul_SkAlphaType : kOpaque_SkAlphaType);
  SkPngEncoder::Options options;
  if (description) {
    const char* keyword = "Description";
    const void* entries[2] = {keyword, description->c_str()};
    std::size_t sizes[2] = {std::strlen(keyword) + 1, description->size() + 1};
    options.fComments = SkDataTable::MakeCopyArrays(entries, sizes, 2);
  }
  VectorWStream stream(out);
  return SkPngEncoder::Encode(&stream, pixmap, options);
}

bool encode_jpeg(const std::uint8_t* rgba, int width, int height, int quality, std::vector<std::uint8_t>& out) {
  SkPixmap pixmap = rgba_pixmap(rgba, width, height, kOpaque_SkAlphaType);
  SkJpegEncoder::Options options;
  options.fQuality = quality;
  VectorWStream stream(out);
  return SkJpegEncoder::Encode(&stream, pixmap, options);
}

} // namespace mse_skia
