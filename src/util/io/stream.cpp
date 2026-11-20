//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/io/stream.hpp>
#include <util/file_utils.hpp>
#include <util/error.hpp>
#include <cerrno>
#include <climits>
#include <cstring>
#include <cwchar>
#ifdef _WIN32
  #include <io.h>
#else
  #include <unistd.h>
#endif

// ----------------------------------------------------------------------------- : InputStream

static const size_t STREAM_BUFFER_SIZE = 64 * 1024;

InputStream::~InputStream() {}

size_t InputStream::fill() {
  // precondition: pos == end
  if (at_end || has_error) return 0;
  if (buf.size() < STREAM_BUFFER_SIZE) buf.resize(STREAM_BUFFER_SIZE);
  pos = end = 0;
  size_t n = doRead(buf.data(), buf.size());
  if (n == 0 && !has_error) at_end = true;
  end = n;
  return n;
}

int InputStream::getcSlow() {
  if (fill() == 0) return EOF;
  return (unsigned char)buf[pos++];
}

size_t InputStream::read(void* buffer, size_t size) {
  char* out = (char*)buffer;
  size_t done = 0;
  while (done < size) {
    if (pos < end) {
      size_t n = min(size - done, end - pos);
      memcpy(out + done, buf.data() + pos, n);
      pos += n;
      done += n;
    } else if (at_end || has_error) {
      break;
    } else if (size - done >= STREAM_BUFFER_SIZE) {
      // large read: bypass the buffer
      size_t n = doRead(out + done, size - done);
      if (n == 0) {
        if (!has_error) at_end = true;
        break;
      }
      done += n;
    } else if (fill() == 0) {
      break;
    }
  }
  return done;
}

void InputStream::ungetc(int c) {
  if (c == EOF) return;
  if (pos == 0) {
    // make room at the front
    size_t room = max((size_t)16, buf.size() - end);
    buf.insert(buf.begin(), room, 0);
    pos += room;
    end += room;
  }
  buf[--pos] = (char)c;
}

std::string InputStream::readAll() {
  std::string data;
  char chunk[16 * 1024];
  while (true) {
    size_t n = read(chunk, sizeof(chunk));
    data.append(chunk, n);
    if (n < sizeof(chunk)) break;
  }
  return data;
}

// ----------------------------------------------------------------------------- : OutputStream

OutputStream::~OutputStream() {
  // note: subclasses with a doClose() call close() themselves, they are already destroyed by now
}

bool OutputStream::write(const void* data, size_t size) {
  if (has_error || closed) {
    has_error = true;
    return false;
  }
  if (size == 0) return true;
  if (!doWrite(data, size)) has_error = true;
  return !has_error;
}

bool OutputStream::write(InputStream& in) {
  char chunk[16 * 1024];
  while (true) {
    size_t n = in.read(chunk, sizeof(chunk));
    if (n > 0 && !write(chunk, n)) return false;
    if (n < sizeof(chunk)) break;
  }
  return ok() && !in.error();
}

bool OutputStream::close() {
  if (!closed) {
    closed = true;
    if (!doClose()) has_error = true;
  }
  return !has_error;
}

// ----------------------------------------------------------------------------- : Files

static FILE* open_file(const String& filename, bool for_writing) {
  std::filesystem::path path = to_fs_path(filename);
  #ifdef _WIN32
    return _wfopen(path.c_str(), for_writing ? L"wb" : L"rb");
  #else
    return fopen(path.c_str(), for_writing ? "wb" : "rb");
  #endif
}

FileInputStream::FileInputStream(const String& filename)
  : file(open_file(filename, false))
{
  if (!file) setError();
}

FileInputStream::~FileInputStream() {
  if (file) fclose(file);
}

size_t FileInputStream::doRead(void* buffer, size_t size) {
  if (!file) return 0;
  size_t n = fread(buffer, 1, size, file);
  if (n == 0 && ferror(file)) setError();
  return n;
}

FileOutputStream::FileOutputStream(const String& filename)
  : file(open_file(filename, true))
{
  if (!file) setError();
}

FileOutputStream::~FileOutputStream() {
  close();
}

bool FileOutputStream::doWrite(const void* data, size_t size) {
  return file && fwrite(data, 1, size, file) == size;
}

bool FileOutputStream::doClose() {
  if (!file) return false;
  bool result = fclose(file) == 0;
  file = nullptr;
  return result;
}

// ----------------------------------------------------------------------------- : File descriptors

FdOutputStream::FdOutputStream(int fd, bool close_fd)
  : fd(fd), close_fd(close_fd)
{
  if (fd < 0) setError();
  buf.reserve(STREAM_BUFFER_SIZE);
}

FdOutputStream::~FdOutputStream() {
  close();
}

bool FdOutputStream::writeAll(const char* data, size_t size) {
  while (size > 0) {
    #ifdef _WIN32
      int n = _write(fd, data, (unsigned int)min(size, (size_t)INT_MAX));
    #else
      ssize_t n = ::write(fd, data, size);
    #endif
    if (n < 0) {
      if (errno == EINTR) continue;
      return false;
    }
    data += n;
    size -= (size_t)n;
  }
  return true;
}

bool FdOutputStream::flush() {
  bool result = writeAll(buf.data(), buf.size());
  buf.clear();
  return result;
}

bool FdOutputStream::doWrite(const void* data, size_t size) {
  if (fd < 0) return false;
  if (buf.size() + size > STREAM_BUFFER_SIZE && !flush()) return false;
  if (size >= STREAM_BUFFER_SIZE) {
    return writeAll((const char*)data, size); // big chunks are not buffered
  }
  buf.insert(buf.end(), (const char*)data, (const char*)data + size);
  return true;
}

bool FdOutputStream::doClose() {
  if (fd < 0) return false;
  bool result = ok() ? flush() : true;
  if (close_fd) {
    #ifdef _WIN32
      if (_close(fd) != 0) result = false;
    #else
      if (::close(fd) != 0) result = false;
    #endif
  }
  fd = -1;
  return result;
}

// ----------------------------------------------------------------------------- : Memory

MemoryInputStream::MemoryInputStream(const void* data, size_t size)
  : data((const char*)data), size(size)
{}

size_t MemoryInputStream::doRead(void* buffer, size_t n) {
  n = min(n, size - read_pos);
  memcpy(buffer, data + read_pos, n);
  read_pos += n;
  return n;
}

StringInputStream::StringInputStream(std::string data)
  : data(move(data))
{}

size_t StringInputStream::doRead(void* buffer, size_t n) {
  n = min(n, data.size() - read_pos);
  memcpy(buffer, data.data() + read_pos, n);
  read_pos += n;
  return n;
}

bool StringOutputStream::doWrite(const void* bytes, size_t size) {
  data.append((const char*)bytes, size);
  return true;
}

// ----------------------------------------------------------------------------- : Text input

bool eat_utf8_bom(InputStream& input) {
  int c;
  if ((c = input.getc()) == 0xEF) {
    if ((c = input.getc()) == 0xBB) {
      if ((c = input.getc()) == 0xBF) {
        return true;
      } else if (c != EOF) input.ungetc(c);
    } else if (c != EOF) input.ungetc(c);
  } else if (c != EOF) input.ungetc(c);
  return false;
}

String read_utf8_line(InputStream& input, bool until_eof) {
  std::string buffer;
  if (until_eof) {
    buffer = input.readAll();
  } else {
    while (true) {
      int c = input.getc();
      if (c == EOF) break;
      if (c == '\n') break;
      if (c == '\r') {
        c = input.getc();
        if (c != '\n' && c != EOF) {
          input.ungetc(c); // \r but not \r\n
        }
        break;
      }
      buffer += (char)c;
    }
  }
  String result;
  if (!decode_utf8(buffer.data(), buffer.size(), result)) {
    throw ParseError(_("Invalid UTF-8 sequence"));
  }
  return result;
}

// Read one character in the C library's multibyte encoding; returns false at the end of the stream.
// The bytes that were used are stored in 'bytes', so they can be put back.
static bool read_locale_char(InputStream& input, wchar_t& wc, std::string& bytes) {
  bytes.clear();
  while (true) {
    int c = input.getc();
    if (c == EOF) return false; // (an incomplete character at the end is dropped, as wx did)
    bytes += (char)c;
    mbstate_t state = {};
    size_t n = mbrtowc(&wc, bytes.data(), bytes.size(), &state);
    if (n == (size_t)-2 && bytes.size() < MB_LEN_MAX) {
      continue; // incomplete, read more
    } else if (n == (size_t)-1 || n == (size_t)-2) {
      // invalid: take the first byte as a Latin-1 character, and read the rest again
      for (size_t i = bytes.size() ; i > 1 ; --i) input.ungetc((unsigned char)bytes[i-1]);
      wc = (unsigned char)bytes[0];
      bytes.resize(1);
      return true;
    } else {
      return true; // (n == 0 means a NUL character)
    }
  }
}

String read_locale_line(InputStream& input) {
  String line;
  wchar_t wc;
  std::string bytes;
  while (read_locale_char(input, wc, bytes)) {
    if (wc == L'\0' || wc == L'\n') break;
    if (wc == L'\r') {
      if (read_locale_char(input, wc, bytes) && wc != L'\n') {
        for (size_t i = bytes.size() ; i > 0 ; --i) input.ungetc((unsigned char)bytes[i-1]);
      }
      break;
    }
    line += wc;
  }
  return line;
}

// ----------------------------------------------------------------------------- : Text output

TextOutputStream::TextOutputStream(OutputStream& output, LineEnding line_ending)
  : output(output)
{
  #ifdef _WIN32
    dos = line_ending != LineEnding::UNIX;
  #else
    dos = line_ending == LineEnding::DOS;
  #endif
}

void TextOutputStream::writeString(const String& str) {
  if (dos && str.find(_('\n')) != String::npos) {
    String out;
    out.reserve(str.size() + 16);
    for (Char c : str) {
      if (c == _('\n')) out += _("\r\n");
      else              out += c;
    }
    output.write(encode_utf8(out));
  } else {
    output.write(encode_utf8(str));
  }
}

void TextOutputStream::putChar(Char c) {
  Char str[2] = {c, 0};
  writeString(String(str, 1));
}
