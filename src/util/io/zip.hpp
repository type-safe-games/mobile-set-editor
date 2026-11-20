//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

/** @file util/io/zip.hpp
 *
 *  @brief Reading and writing zip files, on top of minizip-ng.
 */

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/io/stream.hpp>
#include <ctime>
#include <cstdint>

// ----------------------------------------------------------------------------- : ZipEntry

/// An entry in a zip file, as listed by ZipReader
struct ZipEntry {
  String  name;     ///< Name in the zip: '/' separated, without leading '/' or "./", directories end in '/'
  int64_t cd_pos;   ///< Position of the entry in the zip's central directory
  time_t  modified; ///< Last modification time
};

// ----------------------------------------------------------------------------- : ZipReader

/// Lists the entries of a zip file, and opens them for reading
/** The reader keeps the file open until it is destroyed. */
class ZipReader {
public:
  explicit ZipReader(const String& filename);
  ~ZipReader();
  ZipReader(const ZipReader&) = delete;
  ZipReader& operator = (const ZipReader&) = delete;

  /// Was the file opened and read as a zip file?
  inline bool ok() const { return zip != nullptr; }
  inline const String& filename() const { return zip_filename; }
  inline const vector<ZipEntry>& entries() const { return the_entries; }
  /// The comment of the zip file (raw bytes)
  inline const std::string& comment() const { return the_comment; }

  /// Open an entry for reading.
  /** The stream has its own handle on the file, so it doesn't depend on this reader. */
  unique_ptr<InputStream> openEntry(const ZipEntry& entry) const;

private:
  String zip_filename;
  void* file_stream = nullptr; ///< mz_stream_os
  void* zip = nullptr;         ///< mz_zip handle
  vector<ZipEntry> the_entries;
  std::string the_comment;
  friend class ZipWriter;
};

/// Reads the contents of a single entry of a zip file
class ZipEntryInputStream : public InputStream {
public:
  /// Open the entry at position cd_pos in the central directory (see ZipEntry) of the given zip file
  ZipEntryInputStream(const String& zip_filename, int64_t cd_pos);
  ~ZipEntryInputStream() override;
protected:
  size_t doRead(void* buffer, size_t size) override;
private:
  void* file_stream = nullptr;
  void* zip = nullptr;
  bool entry_open = false;
};

// ----------------------------------------------------------------------------- : ZipWriter

/// Writes a new zip file
class ZipWriter {
public:
  /// Create (or truncate) a zip file
  explicit ZipWriter(const String& filename);
  /// Destroying an unclosed writer closes it, ignoring errors
  ~ZipWriter();
  ZipWriter(const ZipWriter&) = delete;
  ZipWriter& operator = (const ZipWriter&) = delete;

  /// Was the file created, and has everything succeeded so far?
  inline bool ok() const { return opened && !has_error; }

  /// Set the comment of the zip file (raw bytes)
  void setComment(const std::string& comment);

  /// Start a new entry (deflate compressed) and return a stream for its contents.
  /** Only one entry can be written at a time: close or destroy the stream before starting the next one.
   *  Closing the stream returns whether the entry was written successfully. */
  unique_ptr<OutputStream> openEntry(const String& name);

  /// Copy an entry from another zip file as it is, without decompressing it.
  bool copyEntry(const ZipReader& from, const ZipEntry& entry);

  /// Write the central directory and close the file. Returns ok().
  bool close();

private:
  void* file_stream = nullptr;
  void* zip = nullptr;
  bool opened = false;
  bool has_error = false;
  std::string the_comment;
  friend class ZipEntryOutputStream;
};
