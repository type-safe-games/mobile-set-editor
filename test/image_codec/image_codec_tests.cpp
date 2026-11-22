// Mobile Set Editor - tests of the Skia image codec against the wx one (#41)
// SPDX-License-Identifier: GPL-3.0-or-later
//
//   image_codec_tests <images dir> [<pack dir>]
//
// Every PNG and JPEG under the images dir (and, if given, under a template pack) is decoded with
// both codecs. PNGs must give identical Images, JPEGs the same size and pixels within a small
// tolerance (the libjpeg-turbo versions differ). Then round trips through the Skia encoders.

#include <util/prec.hpp>
#include <gfx/image_codec.hpp>
#include <gfx/wx_image_codec.hpp>
#include <gfx/skia/skia_image_codec.hpp>
#include <wx/init.h>
#include <wx/image.h>
#include <wx/log.h>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace fs = std::filesystem;

// ----------------------------------------------------------------------------- : Checking

static int failures = 0;

#define CHECK(cond) do { if (!(cond)) { ++failures; std::fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_MSG(cond, ...) do { if (!(cond)) { ++failures; std::fprintf(stderr, "%s:%d: ", __FILE__, __LINE__); std::fprintf(stderr, __VA_ARGS__); std::fputc('\n', stderr); } } while (0)

static std::vector<std::uint8_t> read_file(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

static bool decode(ImageCodec& codec, const std::vector<std::uint8_t>& data, Image& out) {
  return codec.decode(data.data(), data.size(), out);
}

static std::vector<std::uint8_t> encode(ImageCodec& codec, const Image& image, const char* extension) {
  StringOutputStream out;
  if (!codec.encode(image, String(extension, extension + strlen(extension)), out)) return {};
  std::string s = out.str();
  return std::vector<std::uint8_t>(s.begin(), s.end());
}

/// Largest difference of any channel (alpha included) between two images of the same size; -1 if they differ in shape
static int max_difference(const Image& a, const Image& b) {
  if (a.GetWidth() != b.GetWidth() || a.GetHeight() != b.GetHeight() || a.HasAlpha() != b.HasAlpha()) return -1;
  size_t n = (size_t)a.GetWidth() * a.GetHeight();
  int diff = 0;
  for (size_t i = 0 ; i < 3 * n ; ++i) diff = std::max(diff, std::abs(a.GetData()[i] - b.GetData()[i]));
  if (a.HasAlpha()) for (size_t i = 0 ; i < n ; ++i) diff = std::max(diff, std::abs(a.GetAlpha()[i] - b.GetAlpha()[i]));
  return diff;
}

static bool same_image(const Image& a, const Image& b) {
  return max_difference(a, b) == 0 && a.HasMask() == b.HasMask() && a.HasPalette() == b.HasPalette() &&
         (!a.HasPalette() || a.GetPalette() == b.GetPalette());
}

static bool has_extension(const fs::path& p, std::initializer_list<const char*> exts) {
  std::string e = p.extension().string();
  std::transform(e.begin(), e.end(), e.begin(), ::tolower);
  for (const char* x : exts) if (e == x) return true;
  return false;
}

// ----------------------------------------------------------------------------- : Decoding

/// JPEG decoders may round differently; the wx and Skia libjpeg-turbo builds differ in version
static const int jpeg_tolerance = 2;

struct DecodeStats { int files = 0, identical = 0, within_tolerance = 0, wx_only = 0, neither = 0; int worst = 0; };

static void compare_decoding(ImageCodec& wx, ImageCodec& skia, const fs::path& dir, DecodeStats& png, DecodeStats& jpeg) {
  std::vector<fs::path> files;
  for (auto const& entry : fs::recursive_directory_iterator(dir)) {
    if (entry.is_regular_file() && has_extension(entry.path(), {".png", ".jpg", ".jpeg"})) files.push_back(entry.path());
  }
  std::sort(files.begin(), files.end());
  for (const fs::path& file : files) {
    std::vector<std::uint8_t> data = read_file(file);
    Image a, b;
    bool ok_wx = decode(wx, data, a), ok_skia = decode(skia, data, b);
    bool is_png = data.size() >= 4 && data[0] == 0x89 && data[1] == 'P';
    DecodeStats& s = is_png ? png : jpeg;
    ++s.files;
    if (!ok_wx && !ok_skia) { ++s.neither; continue; }
    if (!ok_skia) { ++s.wx_only; CHECK_MSG(false, "only wx decodes %s", file.c_str()); continue; }
    CHECK_MSG(ok_wx, "only Skia decodes %s", file.c_str());
    if (!ok_wx) continue;
    int diff = max_difference(a, b);
    if (diff == 0 && same_image(a, b)) { ++s.identical; continue; }
    if (diff < 0) {
      CHECK_MSG(false, "%s: wx %dx%d%s, Skia %dx%d%s", file.c_str(), a.GetWidth(), a.GetHeight(), a.HasAlpha() ? " alpha" : "",
                b.GetWidth(), b.GetHeight(), b.HasAlpha() ? " alpha" : "");
      continue;
    }
    s.worst = std::max(s.worst, diff);
    if (!is_png && diff <= jpeg_tolerance) { ++s.within_tolerance; continue; }
    CHECK_MSG(false, "%s differs between wx and Skia (max channel difference %d%s)", file.c_str(), diff,
              diff == 0 ? ", palette or mask" : "");
  }
}

static void print_stats(const char* what, const DecodeStats& s) {
  std::printf("%s: %d files, %d identical, %d within tolerance (worst %d), %d decoded by neither\n",
              what, s.files, s.identical, s.within_tolerance, s.worst, s.neither);
}

// ----------------------------------------------------------------------------- : Encoding

static Image test_image(int w, int h, bool alpha) {
  Image image(w, h, false);
  for (int i = 0 ; i < w * h * 3 ; ++i) image.GetData()[i] = (Byte)(i * 7 + i / 5);
  if (alpha) {
    image.SetAlpha();
    for (int i = 0 ; i < w * h ; ++i) image.GetAlpha()[i] = (Byte)(i * 13);
  }
  return image;
}

static void test_png_round_trip(ImageCodec& wx, ImageCodec& skia) {
  for (bool alpha : {false, true}) {
    Image image = test_image(31, 9, alpha);
    std::vector<std::uint8_t> png = encode(skia, image, "png");
    CHECK(!png.empty());
    Image back_skia, back_wx;
    CHECK(decode(skia, png, back_skia));
    CHECK(decode(wx, png, back_wx));
    CHECK_MSG(max_difference(image, back_skia) == 0, "PNG round trip through Skia changed the image (alpha %d)", alpha);
    CHECK_MSG(max_difference(image, back_wx) == 0, "wx reads Skia's PNG differently (alpha %d)", alpha);
  }
  // A mask colour becomes transparency, as when wx encodes
  Image masked = test_image(8, 8, false);
  Byte* p = masked.GetData() + 3 * 10;
  masked.SetMaskColour(p[0], p[1], p[2]);
  Image back;
  CHECK(decode(skia, encode(skia, masked, "png"), back));
  Image via_wx;
  CHECK(decode(wx, encode(wx, masked, "png"), via_wx));
  CHECK(back.HasAlpha());
  CHECK_MSG(max_difference(back, via_wx) == 0, "masked image encodes differently through Skia and wx");
}

static void test_png_description(ImageCodec& wx, ImageCodec& skia, const fs::path& images) {
  // Written by Skia, read back by Skia
  Image image = test_image(5, 4, true);
  String metadata = L"<mse-card-data>[{\"name\":\"Æther — 光\"}]</mse-card-data>";
  image.SetMetadata(metadata);
  std::vector<std::uint8_t> png = encode(skia, image, "png");
  Image back;
  CHECK(decode(skia, png, back));
  CHECK(back.HasMetadata());
  CHECK(back.GetMetadata() == metadata);
  // Pixels decode the same with wx, which ignores the chunk
  Image back_wx;
  CHECK(decode(wx, png, back_wx));
  CHECK(max_difference(back, back_wx) == 0);
  // No metadata, no chunk
  Image plain = test_image(5, 4, false);
  CHECK(decode(skia, encode(skia, plain, "png"), back));
  CHECK(!back.HasMetadata());
  // Written by another encoder (PIL), next to another tEXt chunk; Latin-1 when not UTF-8
  CHECK(decode(skia, read_file(images / "described.png"), back));
  CHECK(back.GetMetadata() == L"<mse-card-data>[x]</mse-card-data>");
}

static void test_jpeg_encode(ImageCodec& wx, ImageCodec& skia) {
  Image image(40, 24, false);
  for (int y = 0 ; y < 24 ; ++y) for (int x = 0 ; x < 40 ; ++x) {
    Byte* p = image.GetData() + 3 * (y * 40 + x);
    p[0] = (Byte)(x * 6); p[1] = (Byte)(y * 10); p[2] = 128;
  }
  image.SetAlpha(); // alpha is dropped
  CHECK(skia.canEncode(L"jpg"));
  CHECK(!skia.canEncode(L"bmp"));
  std::vector<std::uint8_t> jpeg = encode(skia, image, "jpg");
  CHECK(!jpeg.empty() && jpeg[0] == 0xFF && jpeg[1] == 0xD8);
  Image back_skia, back_wx;
  CHECK(decode(skia, jpeg, back_skia));
  CHECK(decode(wx, jpeg, back_wx));
  CHECK(!back_skia.HasAlpha());
  Image opaque = image.Copy();
  opaque.ClearAlpha();
  int diff = max_difference(opaque, back_skia);
  CHECK_MSG(diff >= 0 && diff <= 24, "JPEG round trip off by %d", diff);
  diff = max_difference(back_skia, back_wx);
  CHECK_MSG(diff >= 0 && diff <= jpeg_tolerance, "wx reads Skia's JPEG differently (by %d)", diff);
}

static void test_garbage(ImageCodec& skia) {
  Image untouched = test_image(2, 2, false);
  Image out = untouched;
  std::vector<std::uint8_t> junk = {'n', 'o', 't', ' ', 'a', 'n', ' ', 'i', 'm', 'a', 'g', 'e'};
  CHECK(!decode(skia, junk, out));
  CHECK(out.IsSameAs(untouched));
  std::vector<std::uint8_t> truncated = encode(skia, test_image(30, 30, false), "png");
  truncated.resize(40);
  CHECK(!decode(skia, truncated, out));
}

// ----------------------------------------------------------------------------- : Main

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <images dir> [<pack dir>]\n", argv[0]);
    return 2;
  }
  wxInitializer init;
  wxInitAllImageHandlers();
  wxLog::EnableLogging(false);
  unique_ptr<ImageCodec> wx = make_wx_image_codec(), skia = make_skia_image_codec();
  fs::path images = argv[1];

  DecodeStats png, jpeg;
  compare_decoding(*wx, *skia, images, png, jpeg);
  print_stats("test PNGs", png);
  print_stats("test JPEGs", jpeg);
  CHECK(png.files >= 10 && jpeg.files >= 4);

  test_png_round_trip(*wx, *skia);
  test_png_description(*wx, *skia, images);
  test_jpeg_encode(*wx, *skia);
  test_garbage(*skia);

  if (argc > 2) {
    DecodeStats pack_png, pack_jpeg;
    compare_decoding(*wx, *skia, argv[2], pack_png, pack_jpeg);
    print_stats("pack PNGs", pack_png);
    print_stats("pack JPEGs", pack_jpeg);
  }

  if (failures) std::fprintf(stderr, "%d check(s) failed\n", failures);
  else std::printf("all checks passed\n");
  return failures ? 1 : 0;
}
