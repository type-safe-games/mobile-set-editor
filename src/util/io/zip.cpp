//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/io/zip.hpp>
#include <util/file_utils.hpp>
#include <climits>
#include <cstring>
#include <mz.h>
#include <mz_strm.h>
#include <mz_strm_os.h>
#include <mz_zip.h>

// ----------------------------------------------------------------------------- : Utility

// minizip-ng takes paths as UTF-8 on Windows, and as native bytes elsewhere
static std::string minizip_path(const String& filename) {
  #ifdef _WIN32
    return encode_utf8(filename);
  #else
    return to_fs_path(filename).string();
  #endif
}

// Open a zip file for reading, returns the mz_zip handle (or nullptr)
static void* open_zip_for_reading(const String& filename, void*& file_stream) {
  file_stream = mz_stream_os_create();
  if (!file_stream) return nullptr;
  if (mz_stream_os_open(file_stream, minizip_path(filename).c_str(), MZ_OPEN_MODE_READ) != MZ_OK) {
    mz_stream_os_delete(&file_stream);
    return nullptr;
  }
  void* zip = mz_zip_create();
  if (!zip || mz_zip_open(zip, file_stream, MZ_OPEN_MODE_READ) != MZ_OK) {
    if (zip) mz_zip_delete(&zip);
    mz_stream_os_close(file_stream);
    mz_stream_os_delete(&file_stream);
    return nullptr;
  }
  return zip;
}

static void close_zip(void*& zip, void*& file_stream) {
  if (zip) {
    mz_zip_close(zip);
    mz_zip_delete(&zip);
  }
  if (file_stream) {
    mz_stream_os_close(file_stream);
    mz_stream_os_delete(&file_stream);
  }
  zip = file_stream = nullptr;
}

static inline bool is_path_sep(Char c) {
  return c == _('/') || c == _('\\');
}

// The name of a zip entry as wxZipEntry::GetName(wxPATH_UNIX) returned it
static String zip_entry_name(const mz_zip_file& info) {
  const char* raw = info.filename ? info.filename : "";
  size_t raw_size = strlen(raw);
  String name;
  // Names flagged as UTF-8 are UTF-8. Other names are in the 'local' encoding, which wx took
  // from the locale; this only handles UTF-8 there, other bytes are read as Latin-1.
  if (!decode_utf8(raw, raw_size, name)) {
    name.clear();
    for (size_t i = 0 ; i < raw_size ; ++i) name += (Char)(unsigned char)raw[i];
  }
  // internal format (wxZipEntry::GetInternalName)
  bool is_dir = !name.empty() && is_path_sep(name[name.size() - 1]);
  if (is_dir) name.erase(name.size() - 1);
  while (!name.empty() && is_path_sep(name[0])) name.erase(0, 1);
  while (name.size() >= 2 && name[0] == _('.') && is_path_sep(name[1])) name.erase(0, 2);
  if (name == _(".") || name == _("..")) name.clear();
  is_dir = is_dir || mz_zip_attrib_is_dir(info.external_fa, info.version_madeby) == MZ_OK;
  if (is_dir && !name.empty()) name += _('/');
  return name;
}

// ----------------------------------------------------------------------------- : ZipReader

ZipReader::ZipReader(const String& filename)
  : zip_filename(filename)
{
  zip = open_zip_for_reading(filename, file_stream);
  if (!zip) return;
  // comment
  const char* comment = nullptr;
  if (mz_zip_get_comment(zip, &comment) == MZ_OK && comment) the_comment = comment;
  // entries
  int32_t err = mz_zip_goto_first_entry(zip);
  while (err == MZ_OK) {
    mz_zip_file* info = nullptr;
    if (mz_zip_entry_get_info(zip, &info) != MZ_OK || !info) break;
    the_entries.push_back(ZipEntry{zip_entry_name(*info), mz_zip_get_entry(zip), info->modified_date});
    err = mz_zip_goto_next_entry(zip);
  }
  if (err != MZ_OK && err != MZ_END_OF_LIST) {
    // a broken central directory
    close_zip(zip, file_stream);
    the_entries.clear();
  }
}

ZipReader::~ZipReader() {
  close_zip(zip, file_stream);
}

unique_ptr<InputStream> ZipReader::openEntry(const ZipEntry& entry) const {
  return make_unique<ZipEntryInputStream>(zip_filename, entry.cd_pos);
}

// ----------------------------------------------------------------------------- : ZipEntryInputStream

ZipEntryInputStream::ZipEntryInputStream(const String& zip_filename, int64_t cd_pos) {
  zip = open_zip_for_reading(zip_filename, file_stream);
  if (zip && mz_zip_goto_entry(zip, cd_pos) == MZ_OK && mz_zip_entry_read_open(zip, 0, nullptr) == MZ_OK) {
    entry_open = true;
  } else {
    setError();
  }
}

ZipEntryInputStream::~ZipEntryInputStream() {
  close_zip(zip, file_stream);
}

size_t ZipEntryInputStream::doRead(void* buffer, size_t size) {
  if (!entry_open) return 0;
  int32_t n = mz_zip_entry_read(zip, buffer, (int32_t)min(size, (size_t)INT32_MAX));
  if (n > 0) return (size_t)n;
  // end of the entry, or an error
  entry_open = false;
  if (n < 0) {
    mz_zip_entry_close(zip);
    setError();
  } else if (mz_zip_entry_read_close(zip, nullptr, nullptr, nullptr) != MZ_OK) {
    setError(); // for example a CRC mismatch
  }
  return 0;
}

// ----------------------------------------------------------------------------- : ZipWriter

static const uint16_t VERSION_MADEBY = (MZ_HOST_SYSTEM_MSDOS << 8) | 45; // MS-DOS attributes, like wx

ZipWriter::ZipWriter(const String& filename) {
  file_stream = mz_stream_os_create();
  if (!file_stream) return;
  if (mz_stream_os_open(file_stream, minizip_path(filename).c_str(), MZ_OPEN_MODE_WRITE | MZ_OPEN_MODE_CREATE) != MZ_OK) {
    mz_stream_os_delete(&file_stream);
    return;
  }
  zip = mz_zip_create();
  if (!zip || mz_zip_open(zip, file_stream, MZ_OPEN_MODE_WRITE) != MZ_OK) {
    if (zip) mz_zip_delete(&zip);
    mz_stream_os_close(file_stream);
    mz_stream_os_delete(&file_stream);
    zip = nullptr;
    return;
  }
  opened = true;
  mz_zip_set_version_madeby(zip, VERSION_MADEBY);
  mz_zip_set_data_descriptor(zip, 0); // the file is seekable, so sizes go in the local headers
}

ZipWriter::~ZipWriter() {
  close();
}

void ZipWriter::setComment(const std::string& comment) {
  the_comment = comment;
}

/// Writes the contents of the entry that a ZipWriter has open
class ZipEntryOutputStream : public OutputStream {
public:
  ZipEntryOutputStream(ZipWriter& writer) : writer(writer) {}
  ~ZipEntryOutputStream() override { close(); }
protected:
  bool doWrite(const void* data, size_t size) override {
    const char* p = (const char*)data;
    while (size > 0) {
      int32_t n = mz_zip_entry_write(writer.zip, p, (int32_t)min(size, (size_t)INT32_MAX));
      if (n <= 0) {
        writer.has_error = true;
        return false;
      }
      p += n;
      size -= (size_t)n;
    }
    return true;
  }
  bool doClose() override {
    if (mz_zip_entry_close(writer.zip) != MZ_OK) writer.has_error = true;
    return !writer.has_error;
  }
private:
  ZipWriter& writer;
};

unique_ptr<OutputStream> ZipWriter::openEntry(const String& name) {
  std::string name_utf8 = encode_utf8(name);
  bool ascii = true;
  for (char c : name_utf8) if (c & 0x80) ascii = false;
  mz_zip_file info = {};
  info.version_madeby     = VERSION_MADEBY;
  info.compression_method = MZ_COMPRESS_METHOD_DEFLATE;
  info.modified_date      = time(nullptr);
  info.flag               = ascii ? 0 : MZ_ZIP_FLAG_UTF8;
  info.filename           = name_utf8.c_str();
  info.zip64              = MZ_ZIP64_AUTO;
  if (!ok() || !zip || mz_zip_entry_write_open(zip, &info, MZ_COMPRESS_LEVEL_DEFAULT, 0, nullptr) != MZ_OK) {
    has_error = true;
    // a stream to nowhere, that fails
    struct FailedStream : OutputStream {
      ~FailedStream() override { close(); }
      bool doWrite(const void*, size_t) override { return false; }
      bool doClose() override { return false; }
    };
    return make_unique<FailedStream>();
  }
  return make_unique<ZipEntryOutputStream>(*this);
}

bool ZipWriter::copyEntry(const ZipReader& from, const ZipEntry& entry) {
  if (!ok() || !zip || !from.ok()) return has_error = true, false;
  void* src = from.zip;
  mz_zip_file* info = nullptr;
  if (mz_zip_goto_entry(src, entry.cd_pos) != MZ_OK ||
      mz_zip_entry_get_info(src, &info) != MZ_OK ||
      mz_zip_entry_read_open(src, 1, nullptr) != MZ_OK) {
    return has_error = true, false;
  }
  bool success = mz_zip_entry_write_open(zip, info, MZ_COMPRESS_LEVEL_DEFAULT, 1, nullptr) == MZ_OK;
  // copy the compressed data
  char buffer[16 * 1024];
  while (success) {
    int32_t n = mz_zip_entry_read(src, buffer, sizeof(buffer));
    if (n < 0) success = false;
    if (n <= 0) break;
    if (mz_zip_entry_write(zip, buffer, n) != n) success = false;
  }
  uint32_t crc = 0;
  int64_t compressed_size = 0, uncompressed_size = 0;
  if (mz_zip_entry_read_close(src, &crc, &compressed_size, &uncompressed_size) != MZ_OK) success = false;
  if (mz_zip_entry_is_open(zip) == MZ_OK) {
    if (success) {
      success = mz_zip_entry_write_close(zip, crc, compressed_size, uncompressed_size) == MZ_OK;
    } else {
      mz_zip_entry_close(zip);
    }
  }
  if (!success) has_error = true;
  return success;
}

bool ZipWriter::close() {
  if (zip) {
    if (!the_comment.empty() && mz_zip_set_comment(zip, the_comment.c_str()) != MZ_OK) has_error = true;
    if (mz_zip_close(zip) != MZ_OK) has_error = true;
    mz_zip_delete(&zip);
    zip = nullptr;
    if (mz_stream_os_close(file_stream) != MZ_OK) has_error = true;
    mz_stream_os_delete(&file_stream);
    file_stream = nullptr;
  }
  return ok();
}
