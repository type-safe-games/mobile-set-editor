//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

/** @file gfx/image.hpp
 *  MSE's own in-memory image: an RGB plane plus an optional alpha plane.
 *
 *  This replaces wxImage in the core (#31). It deliberately keeps wxImage's memory layout and
 *  the subset of its behaviour that the image code relies on, so the pixel loops port unchanged
 *  and render bit-identically:
 *   - GetData() is width*height*3 bytes, rows top to bottom, R,G,B per pixel.
 *   - GetAlpha() is width*height bytes, or nullptr when the image has no alpha channel.
 *   - Copies share their pixels (reference counting, like wxImage): writing through GetData() or
 *     GetAlpha() of one copy is seen by the others. Operations that Image makes exclusive
 *     (InitAlpha, SetAlpha, Paste, SetMaskColour, SetOption, ...) first unshare, exactly as wx
 *     does. Use Copy() for an independent image.
 *   - An optional mask colour (images decoded by the codec can have one; InitAlpha turns it into
 *     alpha), and the palette of a decoded palette-based image.
 *   - Named string options, carried for the codec (resolution etc.), and the card metadata
 *     (<mse-card-data>/<mse-crop-data>) that is written as the PNG description.
 *
 *  Decoding and encoding go through the ImageCodec (gfx/image_codec.hpp).
 *  Conversion to and from wx (for drawing on a DC) is in gfx/wx_image_adapter.hpp.
 */

// ----------------------------------------------------------------------------- : Includes

#include <memory>

// ----------------------------------------------------------------------------- : Image

/// How Image::Paste combines the alpha of the pasted image (as wxImageAlphaBlendMode)
enum ImageAlphaBlend
{  IMAGE_ALPHA_BLEND_OVER    ///< overwrite pixels and alpha
,  IMAGE_ALPHA_BLEND_COMPOSE ///< compose the pasted image over this one
};

/// An RGB image with an optional alpha channel
class Image {
public:
  /// An invalid image (!IsOk())
  Image() {}
  /// A new image; black if clear, otherwise the contents are unspecified
  Image(int width, int height, bool clear = true) { Create(width, height, clear); }

  bool Create(int width, int height, bool clear = true);
  void Destroy() { data.reset(); }

  bool IsOk() const;
  inline bool Ok() const { return IsOk(); }

  int GetWidth() const;
  int GetHeight() const;
  inline wxSize GetSize() const { return wxSize(GetWidth(), GetHeight()); }

  /// The RGB plane (width*height*3 bytes), shared between copies
  Byte* GetData() const;
  /// The alpha plane (width*height bytes), or nullptr
  Byte* GetAlpha() const;
  inline bool HasAlpha() const { return GetAlpha() != nullptr; }
  /// Add an alpha channel: opaque, or from the mask if there is one (which is then removed)
  void InitAlpha();
  /// Allocate a new (zeroed) alpha channel, replacing any existing one
  void SetAlpha();
  /// Set the alpha of a single pixel, the image must have alpha
  void SetAlpha(int x, int y, Byte alpha);
  /// Remove the alpha channel
  void ClearAlpha();

  // Mask colour
  bool HasMask() const;
  void SetMaskColour(Byte r, Byte g, Byte b);
  void SetMask(bool mask = true);
  Byte GetMaskRed() const;
  Byte GetMaskGreen() const;
  Byte GetMaskBlue() const;

  /// Do the two images share their data? (wxObject::IsSameAs)
  inline bool IsSameAs(const Image& that) const { return data == that.data; }
  
  /// An independent copy (pixels, alpha, mask and options)
  Image Copy() const;
  /// A copy of a part of this image (without options)
  Image GetSubImage(const wxRect& rect) const;
  /// Paste another image into this one at position (x,y), as wxImage::Paste
  void Paste(const Image& image, int x, int y, ImageAlphaBlend blend = IMAGE_ALPHA_BLEND_OVER);
  /// Nearest neighbour scaling, as wxImage::Scale with wxIMAGE_QUALITY_NORMAL
  Image Scale(int width, int height) const;
  inline Image& Rescale(int width, int height) { return *this = Scale(width, height); }
  /// Rotate by an arbitrary angle (radians, counter clockwise) around a point, as wxImage::Rotate
  Image Rotate(double angle, const wxPoint& centre, bool interpolating = true) const;

  /// The palette the image was decoded with, if any (r,g,b triples), carried along for the codec.
  /** Like wxImage's palette it only survives copies and in-place operations; the wx PNG encoder
   *  writes a palette based PNG when the image has one (and few enough colours). */
  bool HasPalette() const;
  const vector<Byte>& GetPalette() const;
  void SetPalette(const vector<Byte>& rgb_triples);
  
  // Named options (case insensitive names), carried along for the codec
  void SetOption(const String& name, const String& value);
  void SetOption(const String& name, int value);
  String GetOption(const String& name) const;
  int GetOptionInt(const String& name) const;
  bool HasOption(const String& name) const;
  /// All options, in the order they were set
  const vector<pair<String,String>>& GetOptions() const;

  /// Card metadata, saved as the PNG description
  inline bool HasMetadata() const { return HasOption(IMAGE_OPTION_METADATA); }
  inline String GetMetadata() const { return GetOption(IMAGE_OPTION_METADATA); }
  inline void SetMetadata(const String& metadata) { SetOption(IMAGE_OPTION_METADATA, metadata); }
  /// Name of the option holding the metadata (wx's wxIMAGE_OPTION_PNG_DESCRIPTION)
  static const String IMAGE_OPTION_METADATA;

private:
  struct Data {
    int width = 0, height = 0;
    vector<Byte> rgb;   ///< empty if not ok
    vector<Byte> alpha; ///< empty if there is no alpha channel
    bool has_mask = false;
    Byte mask_r = 0, mask_g = 0, mask_b = 0;
    bool has_palette = false;
    vector<Byte> palette;
    vector<pair<String,String>> options;
  };
  shared_ptr<Data> data;

  /// Make sure this image doesn't share data with other images (wxObject::AllocExclusive)
  void AllocExclusive();
  Image ShrinkBy(int x_factor, int y_factor) const;
  Image ResampleNearest(int width, int height) const;
};
