//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

/** @file util/io/stream.hpp
 *
 *  @brief MSE's own byte streams, and reading/writing text on them.
 *
 *  These replace wxInputStream/wxOutputStream in the core. Zip entry streams are in util/io/zip.hpp.
 */

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

// ----------------------------------------------------------------------------- : InputStream

/// A stream of bytes that can be read from
/** Reads are buffered here, so getc() is cheap.
 *  Like wxInputStream, eof() only becomes true after trying to read past the end.
 */
class InputStream {
public:
  InputStream() = default;
  InputStream(const InputStream&) = delete;
  InputStream& operator = (const InputStream&) = delete;
  virtual ~InputStream();

  /// Read up to size bytes; returns the number of bytes read.
  /** Fewer than size bytes are only returned at the end of the stream or on an error. */
  size_t read(void* buffer, size_t size);
  /// Read a single byte, or EOF at the end of the stream (or on an error)
  inline int getc() {
    if (pos < end) return (unsigned char)buf[pos++];
    return getcSlow();
  }
  /// Put a byte back, it will be the next one read. Bytes are read back in the reverse order they were put back.
  void ungetc(int c);
  /// Read everything that remains in the stream
  std::string readAll();

  /// Has a read tried to go past the end of the stream?
  inline bool eof() const { return at_end && pos == end; }
  /// Did reading fail? (this does not include reaching the end of the stream)
  inline bool error() const { return has_error; }
  /// Was the stream opened successfully, and has no error happened since?
  inline bool ok() const { return !has_error; }

protected:
  /// Read up to size (> 0) bytes from the source.
  /** Returns 0 at the end of the stream; on failure call setError() and return 0. */
  virtual size_t doRead(void* buffer, size_t size) = 0;
  inline void setError() { has_error = true; }

private:
  std::vector<char> buf; ///< buffered data, buf[pos..end) is still to be read
  size_t pos = 0, end = 0;
  bool at_end = false;
  bool has_error = false;
  int getcSlow();
  size_t fill();
};

// ----------------------------------------------------------------------------- : OutputStream

/// A stream that bytes can be written to
class OutputStream {
public:
  OutputStream() = default;
  OutputStream(const OutputStream&) = delete;
  OutputStream& operator = (const OutputStream&) = delete;
  /// Destroying a stream closes it, errors are then ignored; call close() to check them.
  virtual ~OutputStream();

  /// Write bytes; returns false if they could not all be written (the error is remembered).
  bool write(const void* data, size_t size);
  inline bool write(const std::string& data) { return write(data.data(), data.size()); }
  /// Write everything that remains in an input stream.
  /** Returns false if reading or writing failed. */
  bool write(InputStream& in);
  /// Flush and finish the stream; returns ok(). Closing an already closed stream does nothing.
  /** Subclasses that override doClose() must call close() in their destructor. */
  bool close();

  /// Was the stream opened successfully, and have all writes succeeded so far?
  inline bool ok() const { return !has_error; }

protected:
  /// Write all bytes, return false on failure
  virtual bool doWrite(const void* data, size_t size) = 0;
  /// Flush and release resources, return false on failure
  virtual bool doClose() { return true; }
  inline void setError() { has_error = true; }

private:
  bool has_error = false;
  bool closed = false;
};

// ----------------------------------------------------------------------------- : Files

/// Reading a file
class FileInputStream : public InputStream {
public:
  explicit FileInputStream(const String& filename);
  ~FileInputStream() override;
protected:
  size_t doRead(void* buffer, size_t size) override;
private:
  FILE* file;
};

/// Writing a file, which is created or truncated
class FileOutputStream : public OutputStream {
public:
  explicit FileOutputStream(const String& filename);
  ~FileOutputStream() override;
protected:
  bool doWrite(const void* data, size_t size) override;
  bool doClose() override;
private:
  FILE* file;
};

/// Writing to a file descriptor that is already open (for example one handed over by Android for saveTo(fd)).
/** The descriptor is only closed if close_fd is set. */
class FdOutputStream : public OutputStream {
public:
  explicit FdOutputStream(int fd, bool close_fd = false);
  ~FdOutputStream() override;
protected:
  bool doWrite(const void* data, size_t size) override;
  bool doClose() override;
private:
  int fd;
  bool close_fd;
  std::vector<char> buf;
  bool writeAll(const char* data, size_t size);
  bool flush();
};

// ----------------------------------------------------------------------------- : Memory

/// Reading from bytes in memory, which are not owned by the stream
class MemoryInputStream : public InputStream {
public:
  MemoryInputStream(const void* data, size_t size);
protected:
  size_t doRead(void* buffer, size_t size) override;
private:
  const char* data;
  size_t size, read_pos = 0;
};

/// Reading from a string of bytes owned by the stream
class StringInputStream : public InputStream {
public:
  explicit StringInputStream(std::string data);
protected:
  size_t doRead(void* buffer, size_t size) override;
private:
  std::string data;
  size_t read_pos = 0;
};

/// Writing to a string of bytes in memory
class StringOutputStream : public OutputStream {
public:
  inline const std::string& str() const { return data; }
  inline std::string& str() { return data; }
protected:
  bool doWrite(const void* data, size_t size) override;
private:
  std::string data;
};

// ----------------------------------------------------------------------------- : Text input

/// Eat a UTF-8 byte order mark from the beginning of a stream, if there is one
bool eat_utf8_bom(InputStream& input);

/// Read a UTF-8 encoded line, ending in "\n", "\r\n" or "\r" (not included in the result).
/** If until_eof, read everything that remains instead. Throws ParseError on invalid UTF-8. */
String read_utf8_line(InputStream& input, bool until_eof = false);

/// Read a line in the C library's current multibyte encoding, as wxTextInputStream::ReadLine with wxConvLibc did.
/** Lines end in "\n", "\r\n", "\r" or a NUL character. Bytes that can't be decoded are read as Latin-1 characters. */
String read_locale_line(InputStream& input);

// ----------------------------------------------------------------------------- : Text output

/// Line ending to write for "\n"
enum class LineEnding {
  UNIX,   ///< "\n"
  DOS,    ///< "\r\n"
  NATIVE, ///< DOS on Windows, UNIX elsewhere
};

/// Writes strings to an OutputStream as UTF-8, as wxTextOutputStream with a UTF-8 conversion did
class TextOutputStream {
public:
  explicit TextOutputStream(OutputStream& output, LineEnding line_ending = LineEnding::NATIVE);
  /// Write a string, "\n" is written as the line ending. A string that can't be encoded is not written.
  void writeString(const String& str);
  void putChar(Char c);
private:
  OutputStream& output;
  bool dos;
};
