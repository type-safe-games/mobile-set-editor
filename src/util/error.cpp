//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <mutex>
#include <util/thread.hpp>
#include <util/error.hpp>
#include <queue>

// ----------------------------------------------------------------------------- : Debug utilities

#if defined(_MSC_VER) && defined(_DEBUG) && defined(_CRT_WIDE)
  void msvc_assert(const wchar_t* msg, const wchar_t* expr, const wchar_t* file, unsigned line) {
    if (IsDebuggerPresent()) {
      wchar_t buffer[1024];
      if (msg) {
        wsprintf(buffer, L"Assertion failed: %s: %s, file %s, line %d\n", msg, expr, file, line);
      } else {
        wsprintf(buffer, L"Assertion failed: %s, file %s, line %d\n", expr, file, line);
      }
      OutputDebugStringW(buffer);
      DebugBreak();
    } else {
      _wassert(expr, file, line);
    }
  }
#endif

// ----------------------------------------------------------------------------- : Error types

Error::Error(const String& message)
  : message(message)
{}

Error::~Error() {}

String Error::what() const {
  return message;
}



function<int(const vector<String>& arguments, bool wait)> run_command_hook;

int run_command(const vector<String>& arguments, bool wait) {
  return run_command_hook ? run_command_hook(arguments, wait) : -1;
}

void log_debug(const String& msg) {
  #ifndef NDEBUG
    fprintf(stderr, "%s\n", encode_utf8(msg).c_str());
  #endif
}

String get_stack_trace() {
  return _(""); // not supported
}

InternalError::InternalError(const String& str)
  : Error(
    _("An internal error occurred:\n\n") +
    str + _("\n\n")
    _("Please save your work (use 'save as' so you don't overwrite things)\n")
    _("and restart Magic Set Editor.\n")
    _("You can also find a backup of your last save in the same folder as your set file\n")
    _("called 'SETNAME.mse-set.bak'. Rename it to 'SETNAME-backup.mse-set' to open it.\n")
    _("You should leave a bug report on https://github.com/twanvl/MagicSetEditor2/issues/\n")
    _("Press Ctrl+C to copy this message to the clipboard.")
  )
{
  // add a stacktrace
  const String stack_trace = get_stack_trace();
  if (!stack_trace.empty()) {
    message += _("\n\nCall stack:\n") + stack_trace;
  }
}

// ----------------------------------------------------------------------------- : Parse errors

ScriptParseError::ScriptParseError(size_t pos, int line, const String& filename, const String& error)
  : ParseError(error)
  , start(pos), end(pos), line(line), filename(filename)
{}
ScriptParseError::ScriptParseError(size_t pos, int line, const String& filename, const String& exp, const String& found)
  : ParseError(_("Expected '") + exp + _("' instead of '") + found + _("'"))
  , start(pos), end(pos + found.size()), line(line), filename(filename)
{}
ScriptParseError::ScriptParseError(size_t pos1, size_t pos2, int line, const String& filename, const String& open, const String& close, const String& found)
  : ParseError(_("Expected closing '") + close + _("' for this '") + open + _("' instead of '") + found + _("'"))
  , start(pos1), end(pos2 + found.size()), line(line), filename(filename)
{}
String ScriptParseError::what() const {
  return String(_("(")) + int_to_string((int)start) + _("): ") + Error::what();
}

String concat(const vector<ScriptParseError>& errors) {
  String total;
  FOR_EACH_CONST(e, errors) {
    if (!total.empty()) total += _("\n");
    total += e.what();
  }
  return total;
}
ScriptParseErrors::ScriptParseErrors(const vector<ScriptParseError>& errors)
  : ParseError(concat(errors))
{}

// ----------------------------------------------------------------------------- : Error handling

// messages can be posted from other threads, this mutex protects the message list
std::mutex crit_error_handling;
typedef pair<MessageType,String> Message;
deque<Message> message_queue;
ErrorSink error_sink;

void queue_message(MessageType type, String const& msg) {
  if (error_sink && is_core_thread() && error_sink(type, msg)) {
    return;
  }
  // Thread safety
  std::lock_guard<std::mutex> lock(crit_error_handling);
  // Only show errors in the main thread
  message_queue.push_front(make_pair(type,msg));
}
void qm(String const& msg) {
  queue_message(MESSAGE_ERROR, msg);
}

void handle_error(const Error& e) {
  queue_message(e.is_fatal() ? MESSAGE_FATAL_ERROR : MESSAGE_ERROR, e.what());
}

void handle_error_now(const Error& e) {
  queue_message(MESSAGE_FATAL_ERROR, e.what());
}

bool have_queued_message() {
  std::lock_guard<std::mutex> lock(crit_error_handling);
  return !message_queue.empty();
}

bool get_queued_message(MessageType& type, String& msg) {
  std::lock_guard<std::mutex> lock(crit_error_handling);
  if (message_queue.empty()) {
    return false;
  } else {
    type = message_queue.back().first;
    msg  = message_queue.back().second;
    message_queue.pop_back();
    return true;
  }
}
