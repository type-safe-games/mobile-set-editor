//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gfx/image.hpp>
#include <climits>
#include <cmath>
#include <cstring>
#include <cwctype>

// The operations below reproduce wxWidgets 3.2's wxImage (src/common/image.cpp) bit for bit,
// including its sharing and unsharing of data between copies, so that rendering doesn't change.

const String Image::IMAGE_OPTION_METADATA = L"PngDescription";

// ----------------------------------------------------------------------------- : Basics

bool Image::Create(int width, int height, bool clear) {
  data.reset();
  if (width <= 0 || height <= 0) return false;
  // wx refuses images whose RGB plane doesn't fit in an int
  if ((unsigned long long)width * height * 3 > INT_MAX) return false;
  auto d = make_shared<Data>();
  d->width  = width;
  d->height = height;
  d->rgb.resize((size_t)width * height * 3); // (always zeroed, wx leaves it unspecified if !clear)
  data = d;
  (void)clear;
  return true;
}

bool Image::IsOk() const {
  return data && !data->rgb.empty() && data->width && data->height;
}

int Image::GetWidth() const {
  return IsOk() ? data->width : 0;
}
int Image::GetHeight() const {
  return IsOk() ? data->height : 0;
}

Byte* Image::GetData() const {
  return IsOk() ? data->rgb.data() : nullptr;
}
Byte* Image::GetAlpha() const {
  return IsOk() && !data->alpha.empty() ? data->alpha.data() : nullptr;
}

void Image::AllocExclusive() {
  if (!data) {
    data = make_shared<Data>();
  } else if (data.use_count() > 1) {
    // wxImage::CloneRefData: everything is copied (an invalid image can't be cloned)
    if (data->rgb.empty()) data.reset();
    else data = make_shared<Data>(*data);
  }
}

Image Image::Copy() const {
  Image image;
  if (!IsOk()) return image;
  image.data = make_shared<Data>(*data);
  return image;
}

// ----------------------------------------------------------------------------- : Alpha

void Image::SetAlpha() {
  if (!IsOk()) return;
  AllocExclusive();
  data->alpha.assign((size_t)data->width * data->height, 0);
}

void Image::SetAlpha(int x, int y, Byte alpha) {
  if (!HasAlpha()) return;
  if (x < 0 || y < 0 || x >= data->width || y >= data->height) return;
  AllocExclusive();
  data->alpha[(size_t)y * data->width + x] = alpha;
}

void Image::InitAlpha() {
  if (!IsOk() || HasAlpha()) return;
  SetAlpha();
  Byte* alpha = data->alpha.data();
  const size_t len = data->alpha.size();
  if (data->has_mask) {
    // use the mask to initialize the alpha channel
    const Byte mr = data->mask_r, mg = data->mask_g, mb = data->mask_b;
    const Byte* src = data->rgb.data();
    for (size_t i = 0 ; i < len ; ++i, src += 3) {
      alpha[i] = (src[0] == mr && src[1] == mg && src[2] == mb) ? 0 : 255;
    }
    data->has_mask = false;
  } else {
    memset(alpha, 255, len);
  }
}

void Image::ClearAlpha() {
  if (!HasAlpha()) return;
  AllocExclusive();
  data->alpha.clear();
  data->alpha.shrink_to_fit();
}

// ----------------------------------------------------------------------------- : Mask

bool Image::HasMask() const {
  return IsOk() && data->has_mask;
}
void Image::SetMaskColour(Byte r, Byte g, Byte b) {
  if (!IsOk()) return;
  AllocExclusive();
  data->mask_r = r;
  data->mask_g = g;
  data->mask_b = b;
  data->has_mask = true;
}
void Image::SetMask(bool mask) {
  if (!IsOk()) return;
  AllocExclusive();
  data->has_mask = mask;
}
Byte Image::GetMaskRed()   const { return IsOk() ? data->mask_r : 0; }
Byte Image::GetMaskGreen() const { return IsOk() ? data->mask_g : 0; }
Byte Image::GetMaskBlue()  const { return IsOk() ? data->mask_b : 0; }

// ----------------------------------------------------------------------------- : Palette

bool Image::HasPalette() const {
  return IsOk() && data->has_palette;
}
const vector<Byte>& Image::GetPalette() const {
  static const vector<Byte> none;
  return IsOk() ? data->palette : none;
}
void Image::SetPalette(const vector<Byte>& rgb_triples) {
  if (!IsOk()) return;
  AllocExclusive();
  data->has_palette = true;
  data->palette = rgb_triples;
}

// ----------------------------------------------------------------------------- : Options

static bool option_name_equal(const String& a, const String& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0 ; i < a.size() ; ++i) {
    if (towlower(a[i]) != towlower(b[i])) return false;
  }
  return true;
}

void Image::SetOption(const String& name, const String& value) {
  AllocExclusive();
  if (!data) return;
  for (auto& opt : data->options) {
    if (option_name_equal(opt.first, name)) {
      opt.first  = name;
      opt.second = value;
      return;
    }
  }
  data->options.emplace_back(name, value);
}
void Image::SetOption(const String& name, int value) {
  SetOption(name, int_to_string(value));
}

String Image::GetOption(const String& name) const {
  if (data) {
    for (auto& opt : data->options) {
      if (option_name_equal(opt.first, name)) return opt.second;
    }
  }
  return String();
}
int Image::GetOptionInt(const String& name) const {
  // as wxAtoi
  return (int)wcstol(GetOption(name).c_str(), nullptr, 10);
}
bool Image::HasOption(const String& name) const {
  if (data) {
    for (auto& opt : data->options) {
      if (option_name_equal(opt.first, name)) return true;
    }
  }
  return false;
}
const vector<pair<String,String>>& Image::GetOptions() const {
  static const vector<pair<String,String>> none;
  return data ? data->options : none;
}

// ----------------------------------------------------------------------------- : Sub images and pasting

Image Image::GetSubImage(const IntRect& rect) const {
  Image image;
  if (!IsOk()) return image;
  if (!(rect.GetLeft() >= 0 && rect.GetTop() >= 0 && rect.GetRight() <= GetWidth() && rect.GetBottom() <= GetHeight())) {
    return image; // invalid subimage size
  }
  const int subwidth  = rect.GetWidth();
  const int subheight = rect.GetHeight();
  image.Create(subwidth, subheight, false);
  const Byte* src_data  = GetData();
  const Byte* src_alpha = GetAlpha();
  Byte* subdata = image.GetData();
  Byte* subalpha = nullptr;
  if (!subdata) return image;
  if (src_alpha) {
    image.SetAlpha();
    subalpha = image.GetAlpha();
  }
  if (data->has_mask) {
    image.SetMaskColour(data->mask_r, data->mask_g, data->mask_b);
  }
  const int width = GetWidth();
  const int pixsoff = rect.GetLeft() + width * rect.GetTop();
  src_data += 3 * pixsoff;
  if (src_alpha) src_alpha += pixsoff;
  for (long j = 0 ; j < subheight ; ++j) {
    memcpy(subdata, src_data, 3 * subwidth);
    subdata  += 3 * subwidth;
    src_data += 3 * width;
    if (subalpha) {
      memcpy(subalpha, src_alpha, subwidth);
      subalpha  += subwidth;
      src_alpha += width;
    }
  }
  return image;
}

void Image::Paste(const Image& image, int x, int y, ImageAlphaBlend alphaBlend) {
  if (!IsOk() || !image.IsOk()) return;
  AllocExclusive();

  int xx = 0;
  int yy = 0;
  int width  = image.GetWidth();
  int height = image.GetHeight();
  if (x < 0) {
    xx = -x;
    width += x;
  }
  if (y < 0) {
    yy = -y;
    height += y;
  }
  const int this_width  = data->width;
  const int this_height = data->height;
  if ((x+xx)+width > this_width)   width  = this_width  - (x+xx);
  if ((y+yy)+height > this_height) height = this_height - (y+yy);
  if (width < 1) return;
  if (height < 1) return;

  bool copiedPixels = false;

  // copy the data using memcpy() if there is no alpha compositing, and the masks are compatible
  if (alphaBlend == IMAGE_ALPHA_BLEND_OVER &&
      (!image.HasMask() ||
      ((HasMask() &&
       (GetMaskRed()  ==image.GetMaskRed()) &&
       (GetMaskGreen()==image.GetMaskGreen()) &&
       (GetMaskBlue() ==image.GetMaskBlue())))) ) {
    const Byte* source_data = image.GetData() + 3*(xx + yy*image.GetWidth());
    int source_step = image.GetWidth()*3;
    Byte* target_data = GetData() + 3*((x+xx) + (y+yy)*this_width);
    int target_step = this_width*3;
    for (int j = 0 ; j < height ; j++) {
      memcpy(target_data, source_data, width*3);
      source_data += source_step;
      target_data += target_step;
    }
    copiedPixels = true;
  }

  // Copy over the alpha channel from the original image
  if (image.HasAlpha()) {
    if (!HasAlpha()) InitAlpha();
    const Byte* alpha_source_data = image.GetAlpha() + xx + yy * image.GetWidth();
    const int source_step = image.GetWidth();
    Byte* alpha_target_data = GetAlpha() + (x + xx) + (y + yy) * this_width;
    const int target_step = this_width;
    switch (alphaBlend) {
      case IMAGE_ALPHA_BLEND_OVER: {
        // Copy just the alpha values.
        for (int j = 0 ; j < height ; j++, alpha_source_data += source_step, alpha_target_data += target_step) {
          memcpy(alpha_target_data, alpha_source_data, width);
        }
        break;
      }
      case IMAGE_ALPHA_BLEND_COMPOSE: {
        const Byte* source_data = image.GetData() + 3 * (xx + yy * image.GetWidth());
        Byte* target_data = GetData() + 3 * ((x + xx) + (y + yy) * this_width);
        // Combine the alpha values but also apply alpha blending to the pixels themselves
        for (int j = 0 ; j < height ; j++,
             alpha_source_data += source_step,
             alpha_target_data += target_step,
             source_data += 3 * source_step,
             target_data += 3 * target_step) {
          for (int i = 0 ; i < width ; i++) {
            float source_alpha = alpha_source_data[i] / 255.0f;
            float light_left = (alpha_target_data[i] / 255.0f) * (1.0f - source_alpha);
            float result_alpha = source_alpha + light_left;
            alpha_target_data[i] = (Byte)((result_alpha * 255) + 0.5f);
            if (result_alpha <= 0) {
              int c = 3 * i;
              target_data[c++] = 0;
              target_data[c++] = 0;
              target_data[c] = 0;
              continue;
            }
            for (int c = 3 * i ; c < 3 * (i + 1) ; c++) {
              target_data[c] =
                (Byte)(((source_data[c] * source_alpha +
                    target_data[c] * light_left) /
                  result_alpha) + 0.5f);
            }
          }
        }
        copiedPixels = true;
        break;
      }
    }
  }

  // If we hadn't copied them yet we must take the mask of the image being pasted into account.
  if (!copiedPixels) {
    const Byte* source_data = image.GetData() + 3 * (xx + yy * image.GetWidth());
    int source_step = image.GetWidth() * 3;
    Byte* target_data = GetData() + 3 * ((x + xx) + (y + yy) * this_width);
    int target_step = this_width * 3;
    Byte* alpha_target_data = nullptr;
    const int target_alpha_step = this_width;
    if (HasAlpha()) {
      alpha_target_data = GetAlpha() + (x + xx) + (y + yy) * this_width;
    }
    if (!image.HasMask()) {
      // Copy all pixels
      for (int j = 0 ; j < height ; j++) {
        memcpy(target_data, source_data, width * 3);
        source_data += source_step;
        target_data += target_step;
        // Make all the copied pixels fully opaque
        if (alpha_target_data) {
          memset(alpha_target_data, 255, width);
          alpha_target_data += target_alpha_step;
        }
      }
    } else {
      // Copy all 'non masked' pixels
      Byte r = image.GetMaskRed();
      Byte g = image.GetMaskGreen();
      Byte b = image.GetMaskBlue();
      for (int j = 0 ; j < height ; j++) {
        for (int i = 0 ; i < width * 3 ; i += 3) {
          if ((source_data[i] != r) || (source_data[i + 1] != g) || (source_data[i + 2] != b)) {
            // Copy the non masked pixel
            memcpy(target_data + i, source_data + i, 3);
            if (alpha_target_data) alpha_target_data[i / 3] = 255; // Make the copied pixel fully opaque
          }
        }
        source_data += source_step;
        target_data += target_step;
        if (alpha_target_data) alpha_target_data += target_alpha_step;
      }
    }
  }
}

// ----------------------------------------------------------------------------- : Scaling

Image Image::Scale(int width, int height) const {
  Image image;
  if (!IsOk()) return image;
  // can't scale to/from 0 size
  if (width <= 0 || height <= 0) return image;
  const long old_height = data->height, old_width = data->width;
  if (old_height <= 0 || old_width <= 0) return image;
  // same size: no need to do anything
  if (old_width == width && old_height == height) return *this;
  // nearest neighbour
  if (old_width % width == 0 && old_width >= width &&
      old_height % height == 0 && old_height >= height) {
    return ShrinkBy(old_width / width, old_height / height);
  }
  image = ResampleNearest(width, height);
  // If the original image has a mask, apply the mask to the new image
  if (data->has_mask) {
    image.SetMaskColour(data->mask_r, data->mask_g, data->mask_b);
  }
  // (wx also scales the cursor hotspot options here, MSE has no cursors)
  return image;
}

Image Image::ShrinkBy(int xFactor, int yFactor) const {
  if (xFactor == 1 && yFactor == 1) return *this;
  Image image;
  if (!IsOk()) return image;
  if (xFactor <= 0 || yFactor <= 0) return image;
  const long old_height = data->height, old_width = data->width;
  if (old_height <= 0 || old_width <= 0) return image;
  const long width  = old_width / xFactor;
  const long height = old_height / yFactor;
  image.Create(width, height, false);
  Byte* target_data = image.GetData();
  if (!target_data) return image;

  bool hasMask = false;
  Byte maskRed = 0, maskGreen = 0, maskBlue = 0;
  const Byte* source_data = data->rgb.data();
  const Byte* source_alpha = nullptr;
  Byte* target_alpha = nullptr;
  if (data->has_mask) {
    hasMask = true;
    maskRed   = data->mask_r;
    maskGreen = data->mask_g;
    maskBlue  = data->mask_b;
    image.SetMaskColour(maskRed, maskGreen, maskBlue);
  } else {
    source_alpha = GetAlpha();
    if (source_alpha) {
      image.SetAlpha();
      target_alpha = image.GetAlpha();
    }
  }

  for (long y = 0 ; y < height ; y++) {
    for (long x = 0 ; x < width ; x++) {
      unsigned long avgRed = 0;
      unsigned long avgGreen = 0;
      unsigned long avgBlue = 0;
      unsigned long avgAlpha = 0;
      unsigned long counter = 0;
      // determine average
      for (int y1 = 0 ; y1 < yFactor ; ++y1) {
        long y_offset = (y * yFactor + y1) * old_width;
        for (int x1 = 0 ; x1 < xFactor ; ++x1) {
          const Byte* pixel = source_data + 3 * (y_offset + x * xFactor + x1);
          Byte red   = pixel[0];
          Byte green = pixel[1];
          Byte blue  = pixel[2];
          if (!hasMask || red != maskRed || green != maskGreen || blue != maskBlue) {
            Byte alpha = 255;
            if (source_alpha) alpha = *(source_alpha + y_offset + x * xFactor + x1);
            if (alpha > 0) {
              avgRed   += red;
              avgGreen += green;
              avgBlue  += blue;
            }
            avgAlpha += alpha;
            counter++;
          }
        }
      }
      if (counter == 0) {
        *(target_data++) = data->mask_r;
        *(target_data++) = data->mask_g;
        *(target_data++) = data->mask_b;
      } else {
        if (source_alpha) *(target_alpha++) = (Byte)(avgAlpha / counter);
        *(target_data++) = (Byte)(avgRed   / counter);
        *(target_data++) = (Byte)(avgGreen / counter);
        *(target_data++) = (Byte)(avgBlue  / counter);
      }
    }
  }
  return image;
}

Image Image::ResampleNearest(int width, int height) const {
  Image image;
  const uintptr_t old_width  = data->width;
  const uintptr_t old_height = data->height;
  static const uintptr_t SIZE_LIMIT = static_cast<uintptr_t>(-1) >> 16;
  if (old_width > SIZE_LIMIT || old_height > SIZE_LIMIT) return image;

  image.Create(width, height, false);
  Byte* target_data = image.GetData();
  if (!target_data) return image;

  const Byte* source_data = data->rgb.data();
  const Byte* source_alpha = nullptr;
  Byte* target_alpha = nullptr;
  if (!data->has_mask) {
    source_alpha = GetAlpha();
    if (source_alpha) {
      image.SetAlpha();
      target_alpha = image.GetAlpha();
    }
  }

  const uintptr_t x_delta = (old_width  << 16) / width;
  const uintptr_t y_delta = (old_height << 16) / height;
  Byte* dest_pixel = target_data;
  uintptr_t y = y_delta / 2;
  for (int j = 0 ; j < height ; j++) {
    const Byte* src_line = &source_data[(y>>16)*old_width*3];
    const Byte* src_alpha_line = source_alpha ? &source_alpha[(y>>16)*old_width] : 0;
    uintptr_t x = x_delta / 2;
    for (int i = 0 ; i < width ; i++) {
      const Byte* src_pixel = &src_line[(x>>16)*3];
      dest_pixel[0] = src_pixel[0];
      dest_pixel[1] = src_pixel[1];
      dest_pixel[2] = src_pixel[2];
      dest_pixel += 3;
      if (source_alpha) *(target_alpha++) = src_alpha_line[(x>>16)];
      x += x_delta;
    }
    y += y_delta;
  }
  return image;
}

// ----------------------------------------------------------------------------- : Rotation

// Rotation code by Carlos Moreno, from wxImage::Rotate

static const double ROTATE_EPSILON = 1e-10;

namespace {
  struct RotPoint { double x, y; };
}

static inline RotPoint rotate_point(double x, double y, double cos_angle, double sin_angle, const RotPoint& p0) {
  return RotPoint{p0.x + (x - p0.x) * cos_angle - (y - p0.y) * sin_angle,
                  p0.y + (y - p0.y) * cos_angle + (x - p0.x) * sin_angle};
}

static inline int wx_round(double x) {
  return int(std::lround(x)); // as wxRound
}

Image Image::Rotate(double angle, const IntPoint& centre_of_rotation, bool interpolating) const {
  // screen coordinates are a mirror image of "real" coordinates
  angle = -angle;

  const bool has_alpha = HasAlpha();
  const int w = GetWidth();
  const int h = GetHeight();

  // Create pointer-based array to accelerate access to the image's data
  vector<Byte*> data_rows(h), alpha_rows(has_alpha ? h : 0);
  if (h > 0) {
    data_rows[0] = GetData();
    for (int i = 1 ; i < h ; i++) data_rows[i] = data_rows[i - 1] + (3 * w);
    if (has_alpha) {
      alpha_rows[0] = GetAlpha();
      for (int i = 1 ; i < h ; i++) alpha_rows[i] = alpha_rows[i - 1] + w;
    }
  }
  Byte** rows  = data_rows.data();
  Byte** alpha = alpha_rows.data();

  // precompute coefficients for rotation formula
  const double cos_angle = cos(angle);
  const double sin_angle = sin(angle);

  // find rectangle that covers the rotated image, by rotating the four corners
  const RotPoint p0{(double)centre_of_rotation.x, (double)centre_of_rotation.y};
  RotPoint p1 = rotate_point(0, 0, cos_angle, sin_angle, p0);
  RotPoint p2 = rotate_point(0, h, cos_angle, sin_angle, p0);
  RotPoint p3 = rotate_point(w, 0, cos_angle, sin_angle, p0);
  RotPoint p4 = rotate_point(w, h, cos_angle, sin_angle, p0);

  int x1a = (int) floor(min(min(p1.x, p2.x), min(p3.x, p4.x)));
  int y1a = (int) floor(min(min(p1.y, p2.y), min(p3.y, p4.y)));
  int x2a = (int) ceil (max(max(p1.x, p2.x), max(p3.x, p4.x)));
  int y2a = (int) ceil (max(max(p1.y, p2.y), max(p3.y, p4.y)));

  // Create rotated image
  Image rotated(x2a - x1a + 1, y2a - y1a + 1, false);
  if (has_alpha) rotated.SetAlpha();

  Byte* dst = rotated.GetData();
  Byte* alpha_dst = has_alpha ? rotated.GetAlpha() : nullptr;

  // if the original image has a mask, use its RGB values as the blank pixel, else black
  Byte blank_r = 0, blank_g = 0, blank_b = 0;
  if (HasMask()) {
    blank_r = GetMaskRed();
    blank_g = GetMaskGreen();
    blank_b = GetMaskBlue();
    rotated.SetMaskColour(blank_r, blank_g, blank_b);
  }

  // for each point of the rotated image, find where it came from, by an inverse rotation
  const int rH = rotated.GetHeight();
  const int rW = rotated.GetWidth();

  if (interpolating) {
    for (int y = 0 ; y < rH ; y++) {
      for (int x = 0 ; x < rW ; x++) {
        RotPoint src = rotate_point(x + x1a, y + y1a, cos_angle, -sin_angle, p0);
        if (-0.25 < src.x && src.x < w - 0.75 &&
            -0.25 < src.y && src.y < h - 0.75) {
          // interpolate using the 4 enclosing grid-points.
          int x1, y1, x2, y2;
          if (0 < src.x && src.x < w - 1) {
            x1 = (int) floor(src.x);
            x2 = (int) ceil(src.x);
          } else { // x is near one of the borders (0 or width-1)
            x1 = x2 = wx_round(src.x);
          }
          if (0 < src.y && src.y < h - 1) {
            y1 = (int) floor(src.y);
            y2 = (int) ceil(src.y);
          } else {
            y1 = y2 = wx_round(src.y);
          }
          // the four points and the squared distances for the interpolation formula
          const double d1 = (src.x - x1) * (src.x - x1) + (src.y - y1) * (src.y - y1);
          const double d2 = (src.x - x2) * (src.x - x2) + (src.y - y1) * (src.y - y1);
          const double d3 = (src.x - x2) * (src.x - x2) + (src.y - y2) * (src.y - y2);
          const double d4 = (src.x - x1) * (src.x - x1) + (src.y - y2) * (src.y - y2);
          // If the point is exactly at one point of the grid of the source image, just assign the pixel
          if (d1 < ROTATE_EPSILON) {
            Byte* p = rows[y1] + (3 * x1);
            *(dst++) = *(p++);
            *(dst++) = *(p++);
            *(dst++) = *p;
            if (has_alpha) *(alpha_dst++) = *(alpha[y1] + x1);
          } else if (d2 < ROTATE_EPSILON) {
            Byte* p = rows[y1] + (3 * x2);
            *(dst++) = *(p++);
            *(dst++) = *(p++);
            *(dst++) = *p;
            if (has_alpha) *(alpha_dst++) = *(alpha[y1] + x2);
          } else if (d3 < ROTATE_EPSILON) {
            Byte* p = rows[y2] + (3 * x2);
            *(dst++) = *(p++);
            *(dst++) = *(p++);
            *(dst++) = *p;
            if (has_alpha) *(alpha_dst++) = *(alpha[y2] + x2);
          } else if (d4 < ROTATE_EPSILON) {
            Byte* p = rows[y2] + (3 * x1);
            *(dst++) = *(p++);
            *(dst++) = *(p++);
            *(dst++) = *p;
            if (has_alpha) *(alpha_dst++) = *(alpha[y2] + x1);
          } else {
            // weights for the weighted average are proportional to the inverse of the distance
            Byte* v1 = rows[y1] + (3 * x1);
            Byte* v2 = rows[y1] + (3 * x2);
            Byte* v3 = rows[y2] + (3 * x2);
            Byte* v4 = rows[y2] + (3 * x1);
            const double w1 = 1/d1, w2 = 1/d2, w3 = 1/d3, w4 = 1/d4;
            *(dst++) = (Byte)
              ( (w1 * *(v1++) + w2 * *(v2++) +
                 w3 * *(v3++) + w4 * *(v4++)) /
                (w1 + w2 + w3 + w4) );
            *(dst++) = (Byte)
              ( (w1 * *(v1++) + w2 * *(v2++) +
                 w3 * *(v3++) + w4 * *(v4++)) /
                (w1 + w2 + w3 + w4) );
            *(dst++) = (Byte)
              ( (w1 * *v1 + w2 * *v2 +
                 w3 * *v3 + w4 * *v4) /
                (w1 + w2 + w3 + w4) );
            if (has_alpha) {
              v1 = alpha[y1] + (x1);
              v2 = alpha[y1] + (x2);
              v3 = alpha[y2] + (x2);
              v4 = alpha[y2] + (x1);
              *(alpha_dst++) = (Byte)
                ( (w1 * *v1 + w2 * *v2 +
                   w3 * *v3 + w4 * *v4) /
                  (w1 + w2 + w3 + w4) );
            }
          }
        } else {
          *(dst++) = blank_r;
          *(dst++) = blank_g;
          *(dst++) = blank_b;
          if (has_alpha) *(alpha_dst++) = 0;
        }
      }
    }
  } else { // not interpolating
    for (int y = 0 ; y < rH ; y++) {
      for (int x = 0 ; x < rW ; x++) {
        RotPoint src = rotate_point(x + x1a, y + y1a, cos_angle, -sin_angle, p0);
        const int xs = wx_round(src.x);
        const int ys = wx_round(src.y);
        if (0 <= xs && xs < w && 0 <= ys && ys < h) {
          Byte* p = rows[ys] + (3 * xs);
          *(dst++) = *(p++);
          *(dst++) = *(p++);
          *(dst++) = *p;
          if (has_alpha) *(alpha_dst++) = *(alpha[ys] + (xs));
        } else {
          *(dst++) = blank_r;
          *(dst++) = blank_g;
          *(dst++) = blank_b;
          if (has_alpha) *(alpha_dst++) = 255;
        }
      }
    }
  }
  return rotated;
}
