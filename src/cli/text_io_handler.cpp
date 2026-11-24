//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <cli/text_io_handler.hpp>
#include <util/error.hpp>

// ----------------------------------------------------------------------------- : Text I/O handler

const Char* BRIGHT   = _("\x1B[1m");
const Char* NORMAL   = _("\x1B[0m");
const Char* PARAM    = _("\x1B[36m");
const Char* FILE_EXT = _("\x1B[0;1m");
const Char* GRAY     = _("\x1B[1;30m");
const Char* RED      = _("\x1B[1;31m");
const Char* YELLOW   = _("\x1B[1;33m");
const Char* ENDL     = _("\n");

TextIOHandler cli;

#ifdef __WXMSW__
  bool StdHandleOk(DWORD std_handle) {
    // GetStdHandle sometimes returns an invalid handle instead of INVALID_HANDLE_VALUE
    // check with GetHandleInformation
    HANDLE h = GetStdHandle(std_handle);
    DWORD flags;
    return GetHandleInformation(h,&flags);
  }
#endif

void TextIOHandler::init() {
  bool have_stderr;
  #if defined(__WXMSW__)
    have_console = false;
    escapes = false;
    // Detect whether to use console output
    have_console = StdHandleOk(STD_OUTPUT_HANDLE);
    have_stderr  = StdHandleOk(STD_ERROR_HANDLE);
    // Detect the --color flag, indicating we should allow escapes
    if (have_console) {
      for (int i = 1 ; i < wxTheApp->argc ; ++i) {
        if (String(wxTheApp->argv[i]) == _("--color")) {
          escapes = true;
          break;
        }
      }
    }
  #else
    // TODO: detect console on linux?
    have_console = false;
    have_stderr = false;
    // Use console mode if one of the cli flags is passed
    static const Char* redirect_flags[] = {_("-?"),_("--help"),_("-v"),_("--version"),_("--cli"),_("-c"),_("--export"),_("--create-installer")};
    for (int i = 1 ; i < wxTheApp->argc ; ++i) {
      for (size_t j = 0 ; j < sizeof(redirect_flags)/sizeof(redirect_flags[0]) ; ++j) {
        if (String(wxTheApp->argv[i]) == redirect_flags[j]) {
          have_console = true;
          have_stderr = true;
          break;
        }
      }
    }
    escapes = true; // TODO: detect output redirection
  #endif
  // write to standard output
  stream = stdout;
  raw_mode = false;
}

bool TextIOHandler::haveConsole() const {
  return have_console;
}


// ----------------------------------------------------------------------------- : Output

TextIOHandler& TextIOHandler::operator << (const Char* str) {
  if ((escapes && !raw_mode) || str[0] != 27) {
    if (have_console && !raw_mode) {
      // UTF-8 bytes, as raw mode writes them: fwprintf went through the C locale (non-ASCII came
      // out transliterated or as '?', #55) and took the text as its format string
      fputs(encode_utf8(str).c_str(), stream);
    } else {
      buffer += str;
    }
  }
  return *this;
}

TextIOHandler& TextIOHandler::operator << (const String& str) {
  return *this << static_cast<const Char*>(str.c_str());
}

void TextIOHandler::flush() {
  if (raw_mode) return;
  if (have_console) {
    fflush(stream);
  } else if (!buffer.empty()) {
    // Show message box
    wxMessageBox(buffer, _("Magic Set Editor"), wxOK | wxICON_INFORMATION);
    buffer.clear();
  }
}

// ----------------------------------------------------------------------------- : Input

String TextIOHandler::getLine() {
  // Bytes, decoded as UTF-8 here: fgetws decodes with the C locale, which nothing sets, and on the
  // first non-ASCII byte it failed without reaching the end of the input, so the command loop
  // asked for the next line forever (#55). Bytes that aren't UTF-8 are read as Latin-1.
  std::string line;
  char buffer[2048];
  while (fgets(buffer, sizeof(buffer), stdin)) {
    line += buffer;
    if (!line.empty() && line.back() == '\n') {
      line.pop_back(); // drop newline, done
      break;
    }
  }
  return decode_utf8_or_latin1(line.data(), line.size());
}
bool TextIOHandler::canGetLine() {
  return !feof(stdin) && !ferror(stdin);
}

// ----------------------------------------------------------------------------- : Raw mode

void TextIOHandler::enableRaw() {
  raw_mode = true;
  raw_mode_status = 0;
}

void TextIOHandler::flushRaw() {
  if (!raw_mode) return;
  // always end in a newline
  if (!buffer.empty() && buffer[buffer.size()-1] != _('\n')) {
    buffer += _('\n');
  }
  // count newlines
  int newline_count = 0;
  FOR_EACH_CONST(c,buffer) if (c==_('\n')) newline_count++;
  // write record
  printf("%d\n%d\n", raw_mode_status, newline_count);
  if (!buffer.empty()) {
    #ifdef UNICODE
      fputs(encode_utf8(buffer).c_str(), stream);
    #else
      fputs(buffer.c_str(), stream);
    #endif
  }
  fflush(stdout);
  // clear
  buffer.clear();
  raw_mode_status = 0;
}

// ----------------------------------------------------------------------------- : Errors

void TextIOHandler::show_message(MessageType type, String const& message) {
  stream = stdout;
  if (type == MESSAGE_WARNING) {
    *this << YELLOW << _("WARNING: ") << NORMAL << replace_all(message,_("\n"),_("\n         ")) << ENDL;
  } else {
    *this << RED << _("ERROR: ") << NORMAL << replace_all(message,_("\n"),_("\n       ")) << ENDL;
  }
  encountered_errors = true;
  flush();
  stream = stdout;
  if (raw_mode) raw_mode_status = max(raw_mode_status, type == MESSAGE_WARNING ? 1 : 2);
}

void TextIOHandler::print_pending_errors() {
  MessageType type;
  String msg;
  while (get_queued_message(type, msg)) {
    if (haveConsole()) {
      show_message(type, msg);
      flush();
    } else {
      // no console, use a messagebox instead
      wxMessageBox(msg, wxMessageBoxCaptionStr, type == MESSAGE_INFO ? wxICON_INFORMATION : type == MESSAGE_WARNING ? wxICON_WARNING : wxICON_ERROR);
    }
  }
}

bool TextIOHandler::shown_errors() const {
  return encountered_errors;
}
