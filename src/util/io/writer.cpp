//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include "writer.hpp"
#include <util/vector2d.hpp>
#include <util/error.hpp>
#include <util/version.hpp>
#include <util/io/package.hpp>
#include <boost/logic/tribool.hpp>
using boost::tribool;

// ----------------------------------------------------------------------------- : Writer

Writer::Writer(OutputStream& output, bool indent_output)
  : indentation(0)
  , output(output)
  , stream(output, LineEnding::UNIX)
  , indent_output(indent_output)
{}

Writer::Writer(OutputStream& output, Version file_app_version)
  : indentation(0)
  , output(output)
  , stream(output, LineEnding::UNIX)
  , indent_output(true)
{
  stream.writeString(BYTE_ORDER_MARK);
  handle(_("mse_version"), file_app_version);
}



void Writer::enterBlock(const Char* name) {
  // don't write the key yet
  pending_opened.push_back(name);
}

void Writer::exitBlock() {
  if (pending_opened.empty()) {
    assert(indentation > 0);
    indentation -= 1;
  } else {
    // this block was apparently empty, ignore it
    pending_opened.pop_back();
  }
}

void Writer::writePending() {
  // In enterBlock we have delayed the actual writing of the keys until this point
  // here we write all the pending keys, and increase indentation along the way.
  for (size_t i = 0 ; i < pending_opened.size() ; ++i) {
    if (i > 0) {
      // before entering a sub-block, write a colon after the parent's name
      stream.writeString(_(":\n"));
    }
    indentation += 1;
    writeIndentation();
    stream.writeString(pending_opened[i]);
  }
  pending_opened.clear();
}

void Writer::writeIndentation() {
  if (!indent_output) return;
  for(int i = 1 ; i < indentation ; ++i) {
    stream.putChar(_('\t'));
  }
}

// ----------------------------------------------------------------------------- : Handling basic types

void Writer::handle(const String& value) {
  if (pending_opened.empty()) {
    throw InternalError(_("Can only write a value in a key that was just opened"));
  }
  writePending();
  // write indentation and key
  if (value.find_first_of(_('\n')) != String::npos || (!value.empty() && isSpace(value.GetChar(0)))) {
    // multiline string, or contains leading whitespace
    stream.writeString(_(":\n"));
    indentation += 1;
    // split lines, and write each line
    size_t start = 0, end, size = value.size();
    while (start < size) {
      end = value.find_first_of(_("\n\r"), start); // until end of line
      // write the line
      writeIndentation();
      stream.writeString(value.substr(start, end - start));
      // Skip \r and \n
      if (end == String::npos) break;
      stream.putChar(_('\n'));
      start = end + 1;
      if (start < size) {
        Char c1 = value.GetChar(start - 1);
        Char c2 = value.GetChar(start);
        // skip second character of \r\n or \n\r
        if (c1 != c2 && (c2 == _('\r') || c2 == _('\n')))  start += 1;
      }
    }
    indentation -= 1;
  } else {
    stream.writeString(_(": "));
    stream.writeString(value);
  }
  stream.putChar(_('\n'));
}

template <> void Writer::handle(const int& value) {
  handle(String() << value);
}
template <> void Writer::handle(const unsigned int& value) {
  handle(String() << value);
}
template <> void Writer::handle(const double& value) {
  handle(String() << value);
}
template <> void Writer::handle(const bool& value) {
  handle(value ? _("true") : _("false"));
}
template <> void Writer::handle(const tribool& value) {
  if (!indeterminate(value)) {
    handle(value ? _("true") : _("false"));
  }
}

// ----------------------------------------------------------------------------- : Handling less basic util types

template <> void Writer::handle(const DateTime& date) {
  if (date.isValid()) {
    handle(date.format(DATE_TIME_PERSIST_FORMAT));
  }
}
template <> void Writer::handle(const Vector2D& vec) {
  handle(String::Format(_("(%.10lf,%.10lf)"), vec.x, vec.y));
}

template <> void Writer::handle(const LocalFileName& value) {
  handle(value.toStringForWriting());
}
