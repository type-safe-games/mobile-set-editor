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

// ----------------------------------------------------------------------------- : Image related

// Image codec adapter (wx boundary, until images move off wx in #31):
// wxImage decodes from a wxInputStream, and its format detection seeks around in it.
// So the MSE stream is read into memory, and handed to wx as a seekable wxMemoryInputStream.
bool image_load_file(Image& image, InputStream& stream) {
  wxLogNull noLog; // prevent wx from showing popups about sRGB profiles and other notices
  std::string data = stream.readAll(); // (on a read error, decode what was read, as before)
  wxMemoryInputStream wx_stream(data.data(), data.size());
  return image.LoadFile(wx_stream);
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

