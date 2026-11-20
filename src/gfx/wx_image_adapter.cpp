//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gfx/wx_image_adapter.hpp>
#include <wx/imagpng.h>
#include <wx/imaggif.h>
#include <wx/imagtiff.h>
#include <wx/imagbmp.h>
#include <wx/rawbmp.h>
#include <cstring>

// ----------------------------------------------------------------------------- : Conversion

// wxImage can't enumerate its options, so these are the ones that are copied from a wxImage:
// everything wx's image handlers set when loading or read when saving.
static const wxString wx_image_options[] = {
  wxIMAGE_OPTION_FILENAME,
  wxIMAGE_OPTION_QUALITY,
  wxIMAGE_OPTION_RESOLUTION,
  wxIMAGE_OPTION_RESOLUTIONX,
  wxIMAGE_OPTION_RESOLUTIONY,
  wxIMAGE_OPTION_RESOLUTIONUNIT,
  wxIMAGE_OPTION_MAX_WIDTH,
  wxIMAGE_OPTION_MAX_HEIGHT,
  wxIMAGE_OPTION_ORIGINAL_WIDTH,
  wxIMAGE_OPTION_ORIGINAL_HEIGHT,
  wxIMAGE_OPTION_PNG_DESCRIPTION,
  wxIMAGE_OPTION_PNG_FORMAT,
  wxIMAGE_OPTION_PNG_BITDEPTH,
  wxIMAGE_OPTION_PNG_FILTER,
  wxIMAGE_OPTION_PNG_COMPRESSION_LEVEL,
  wxIMAGE_OPTION_PNG_COMPRESSION_MEM_LEVEL,
  wxIMAGE_OPTION_PNG_COMPRESSION_STRATEGY,
  wxIMAGE_OPTION_PNG_COMPRESSION_BUFFER_SIZE,
  wxIMAGE_OPTION_GIF_COMMENT,
  wxIMAGE_OPTION_GIF_TRANSPARENCY,
  wxIMAGE_OPTION_BMP_FORMAT,
  wxIMAGE_OPTION_CUR_HOTSPOT_X,
  wxIMAGE_OPTION_CUR_HOTSPOT_Y,
  wxIMAGE_OPTION_TIFF_BITSPERSAMPLE,
  wxIMAGE_OPTION_TIFF_SAMPLESPERPIXEL,
  wxIMAGE_OPTION_TIFF_COMPRESSION,
  wxIMAGE_OPTION_TIFF_PHOTOMETRIC,
  wxIMAGE_OPTION_TIFF_IMAGEDESCRIPTOR,
};

wxImage to_wx_image(const Image& image) {
  if (!image.IsOk()) return wxImage();
  const int w = image.GetWidth(), h = image.GetHeight();
  const size_t n = (size_t)w * h;
  // wxImage takes ownership of malloc()ed planes
  Byte* rgb = (Byte*)malloc(n * 3);
  memcpy(rgb, image.GetData(), n * 3);
  Byte* alpha = nullptr;
  if (image.HasAlpha()) {
    alpha = (Byte*)malloc(n);
    memcpy(alpha, image.GetAlpha(), n);
  }
  wxImage out;
  out.Create(w, h, rgb, alpha, false);
  if (image.HasMask()) {
    out.SetMaskColour(image.GetMaskRed(), image.GetMaskGreen(), image.GetMaskBlue());
  }
  if (image.HasPalette()) {
    const vector<Byte>& pal = image.GetPalette();
    const size_t n_colours = pal.size() / 3;
    vector<unsigned char> r(n_colours), g(n_colours), b(n_colours);
    for (size_t i = 0 ; i < n_colours ; ++i) {
      r[i] = pal[3*i]; g[i] = pal[3*i+1]; b[i] = pal[3*i+2];
    }
    out.SetPalette(wxPalette((int)n_colours, r.data(), g.data(), b.data()));
  }
  for (auto const& opt : image.GetOptions()) {
    out.SetOption(opt.first, opt.second);
  }
  return out;
}

Image from_wx_image(const wxImage& image) {
  Image out;
  if (!image.IsOk()) return out;
  const int w = image.GetWidth(), h = image.GetHeight();
  const size_t n = (size_t)w * h;
  out.Create(w, h, false);
  memcpy(out.GetData(), image.GetData(), n * 3);
  if (image.HasAlpha()) {
    out.SetAlpha();
    memcpy(out.GetAlpha(), image.GetAlpha(), n);
  }
  if (image.HasMask()) {
    out.SetMaskColour(image.GetMaskRed(), image.GetMaskGreen(), image.GetMaskBlue());
  }
  if (image.HasPalette()) {
    const wxPalette& palette = image.GetPalette();
    vector<Byte> pal;
    for (int i = 0 ; i < palette.GetColoursCount() ; ++i) {
      unsigned char r, g, b;
      palette.GetRGB(i, &r, &g, &b);
      pal.push_back(r); pal.push_back(g); pal.push_back(b);
    }
    out.SetPalette(pal);
  }
  for (auto const& name : wx_image_options) {
    if (image.HasOption(name)) out.SetOption(name.ToStdWstring(), image.GetOption(name).ToStdWstring());
  }
  return out;
}

Bitmap to_bitmap(const Image& image) {
  return Bitmap(to_wx_image(image));
}

Image from_bitmap(const Bitmap& bitmap) {
  return from_wx_image(bitmap.ConvertToImage());
}

// ----------------------------------------------------------------------------- : Bitmap pixels

void invert_bitmap(Bitmap& bmp) {
  wxNativePixelData data(bmp);
  if (!data) return; // Raw access not available on this platform/bitmap
  wxNativePixelData::Iterator p(data);
  for (int j = 0; j < bmp.GetHeight(); ++j) {
    wxNativePixelData::Iterator rowStart = p;
    for (int i = 0; i < bmp.GetWidth(); ++i) {
      // Invert each channel
      p.Red()   = 255 - p.Red();
      p.Green() = 255 - p.Green();
      p.Blue()  = 255 - p.Blue();
      ++p; // Move to next pixel in the row
    }
    p = rowStart;
    p.OffsetY(data, 1); // Move to the next row
  }
}
