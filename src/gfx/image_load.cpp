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
#include <gfx/image_codec.hpp>

// ----------------------------------------------------------------------------- : Image related

bool image_load_file(Image& image, InputStream& stream) {
  std::string data = stream.readAll(); // (on a read error, decode what was read, as before)
  return image_codec().decode(data.data(), data.size(), image);
}

bool image_load_file(Image& image, const String& name) {
  return retry_io([&]{
    FileInputStream stream(name);
    if (!stream.ok()) return false;
    std::string data = stream.readAll();
    if (stream.error()) return false;
    return image_codec().decode(data.data(), data.size(), image);
  });
}

// ----------------------------------------------------------------------------- : Resource related

Image load_resource_image(const String& name) {
  #if defined(__WXMSW__) && !defined(__GNUC__)
    // Load resource
    // based on wxLoadUserResource
    // The image can be in an IMAGE resource, in any file format
    HRSRC hResource = ::FindResource(wxGetInstance(), name.c_str(), _("IMAGE"));
    if ( hResource == 0 ) throw InternalError(format_string(_("Resource not found: %s"), name));
    
    HGLOBAL hData = ::LoadResource(wxGetInstance(), hResource);
    if ( hData == 0 ) throw InternalError(format_string(_("Resource not an image: %s"), name));
    
    char* data = (char *)::LockResource(hData);
    if ( !data ) throw InternalError(format_string(_("Resource cannot be locked: %s"), name));
    
    int len = ::SizeofResource(wxGetInstance(), hResource);

    Image image;
    image_codec().decode(data, len, image);
    return image;
  #elif defined(__GNUC__)
    static String path = app_data_dir() + _("/resource/");
    String file = path + name;
    Image resource;
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

