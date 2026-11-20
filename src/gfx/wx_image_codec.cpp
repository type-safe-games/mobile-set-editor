//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gfx/wx_image_codec.hpp>
#include <gfx/wx_image_adapter.hpp>
#include <wx/mstream.h>
#include <wx/stream.h>

// ----------------------------------------------------------------------------- : Streams

/// A wxOutputStream that writes to an MSE OutputStream
class WxOutputStreamAdapter : public wxOutputStream {
public:
  explicit WxOutputStreamAdapter(OutputStream& out) : out(out) {}
protected:
  size_t OnSysWrite(const void* buffer, size_t size) override {
    if (!out.write(buffer, size)) {
      m_lasterror = wxSTREAM_WRITE_ERROR;
      return 0;
    }
    return size;
  }
private:
  OutputStream& out;
};

// ----------------------------------------------------------------------------- : WxImageCodec

class WxImageCodec : public ImageCodec {
public:
  bool decode(const void* data, size_t size, Image& out) override {
    wxLogNull noLog; // prevent wx from showing popups about sRGB profiles and other notices
    // wxImage's format detection seeks around in the stream, a memory stream is seekable
    wxMemoryInputStream stream(data, size);
    wxImage image;
    if (!image.LoadFile(stream)) return false;
    out = from_wx_image(image);
    return true;
  }

  bool canEncode(const String& extension) override {
    return wxImage::FindHandler(extension, wxBITMAP_TYPE_ANY) != nullptr;
  }

  bool encode(const Image& image, const String& extension, OutputStream& out) override {
    wxImageHandler* handler = wxImage::FindHandler(extension, wxBITMAP_TYPE_ANY);
    if (!handler) return false;
    wxImage wx_image = to_wx_image(image);
    // (the metadata is the wxIMAGE_OPTION_PNG_DESCRIPTION option, which the PNG handler of wx >= 3.3 writes)
    WxOutputStreamAdapter stream(out);
    return wx_image.SaveFile(stream, handler->GetType()) && stream.IsOk();
  }
};

unique_ptr<ImageCodec> make_wx_image_codec() {
  return make_unique<WxImageCodec>();
}
