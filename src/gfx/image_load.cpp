//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gfx/image_load.hpp>
#include <util/error.hpp>
#include <util/file_utils.hpp>
#include <wx/mstream.h>

// ----------------------------------------------------------------------------- : Seekable stream

/// A wxInputStream in which we can seek, even if the underlying stream can't
/** Seeking is used by the wxImage format detection code, to peek at the header.
 *  So we only need to be able to seek back to the beginning.
 *  The solution is to keep a small buffer of the file header.
 */
class SeekableInputStream : public wxInputStream {
public:
  SeekableInputStream(wxInputStream& stream)
    : stream(stream) {
  }
  bool IsOk() const override {
    return stream.IsOk();
  }
  bool IsSeekable() const override {
    return pos < (wxFileOffset)BUFFER_SIZE;
  }
  size_t LastRead() const override {
    return last_read;
  }
  char Peek() override {
    if (pos < stream_pos) {
      return buffer[pos];
    } else {
      return stream.Peek();
    }
  }
  wxInputStream& Read(void* out_buffer, size_t size) override {
    last_read = 0;
    if (pos < stream_pos) {
      // take from buffer
      size_t n = min(size, min((size_t)stream_pos, BUFFER_SIZE) - (size_t)pos);
      memcpy(out_buffer, buffer + pos, n);
      size -= n;
      out_buffer = (char*)out_buffer + n;
      last_read += n;
      pos += n;
    }
    if (size > 0) {
      assert(pos == stream_pos);
      // take from stream
      stream.Read(out_buffer, size);
      size_t read = stream.LastRead();
      last_read += read;
      if (pos < (wxFileOffset)BUFFER_SIZE) {
        // update buffer
        memcpy(buffer + pos, out_buffer, min(BUFFER_SIZE - (size_t)pos, read));
      }
      stream_pos += read;
      pos += read;
    }
    return *this;
  }
  wxFileOffset SeekI(wxFileOffset target, wxSeekMode mode = wxFromStart) override {
    assert(mode == wxFromStart);
    assert(target < (wxFileOffset)BUFFER_SIZE);
    if (target < (wxFileOffset)BUFFER_SIZE) {
      pos = target;
      return target;
    } else {
      return wxInvalidOffset;
    }
  }
  wxFileOffset TellI() const override {
    return pos;
  }
protected:
  size_t OnSysRead(void* out_buffer, size_t size) override {
    Read(out_buffer, size);
    return last_read;
  }
private:
  wxInputStream& stream;
  static constexpr size_t BUFFER_SIZE = 16;
  Byte buffer[BUFFER_SIZE];
  // positions:
  // Invariant: pos < stream_pos ==> pos < BUFFER_SIZE && buffer[i] is valid for i<=pos
  wxFileOffset stream_pos = 0; // position in underlying stream
  wxFileOffset pos = 0; // position taking into account any seeking done.
  size_t last_read = 0;
};

// ----------------------------------------------------------------------------- : Image related

bool image_load_file(Image& image, wxInputStream &stream) {
  wxLogNull noLog; // prevent wx from showing popups about sRGB profiles and other notices
  if (!stream.IsSeekable()) {
    SeekableInputStream seek_stream(stream);
    return image.LoadFile(seek_stream);
  } else {
    return image.LoadFile(stream);
  }
}

bool image_load_file(Image& image, const wxString &name) {
  wxLogNull noLog;
  return retry_io([&]{ return image.LoadFile(name); });
}

// ----------------------------------------------------------------------------- : Resource related

Image load_resource_image(const String& name) {
  #if defined(__WXMSW__) && !defined(__GNUC__)
    // Load resource
    // based on wxLoadUserResource
    // The image can be in an IMAGE resource, in any file format
    HRSRC hResource = ::FindResource(wxGetInstance(), name.wc_str(), _("IMAGE"));
    if ( hResource == 0 ) throw InternalError(String::Format(_("Resource not found: %s"), name));
    
    HGLOBAL hData = ::LoadResource(wxGetInstance(), hResource);
    if ( hData == 0 ) throw InternalError(String::Format(_("Resource not an image: %s"), name));
    
    char* data = (char *)::LockResource(hData);
    if ( !data ) throw InternalError(String::Format(_("Resource cannot be locked: %s"), name));
    
    int len = ::SizeofResource(wxGetInstance(), hResource);
    wxMemoryInputStream stream(data, len);

    wxLogNull noLog;
    return wxImage(stream);
  #elif defined(__GNUC__)
    static String path = app_data_dir() + _("/resource/");
    String file = path + name;
    wxImage resource;
    if (file_exists(file + _(".png"))) image_load_file(resource, file + _(".png"));
    else if (file_exists(file + _(".bmp"))) image_load_file(resource, file + _(".bmp"));
    else if (file_exists(file + _(".ico"))) image_load_file(resource, file + _(".ico"));
    else if (file_exists(file + _(".cur"))) image_load_file(resource, file + _(".cur"));
    if (resource.Ok()) return resource;
        static String local_path = user_data_dir() + _("/resource/");
        file = local_path + name;
        if (file_exists(file + _(".png"))) image_load_file(resource, file + _(".png"));
        else if (file_exists(file + _(".bmp"))) image_load_file(resource, file + _(".bmp"));
        else if (file_exists(file + _(".ico"))) image_load_file(resource, file + _(".ico"));
        else if (file_exists(file + _(".cur"))) image_load_file(resource, file + _(".cur"));
        if (!resource.Ok()) handle_error(InternalError(String(_("Cannot find resource file at ")) + path + name + _(" or ") + file));
    return resource;
  #else
    #error Handling of resource loading needs to be declared.
  #endif
}

