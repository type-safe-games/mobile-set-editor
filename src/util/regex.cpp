//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/regex.hpp>
#include <util/error.hpp>

#if USE_BOOST_REGEX
// ----------------------------------------------------------------------------- : Regex : boost

void Regex::assign(const String& code) {
  // compile string
  try {
    regex.assign(toStdString(code));
  } catch (const boost::regex_error& e) {
    /// TODO: be more precise
    throw ScriptError(format_string(_("Error while compiling regular expression: '%s'\nAt position: %d\n%s"),
                      code, e.position(), decode_utf8(std::string(e.what()))));
  }
}

String Regex::replace_all(const String& input, const String& format) const {
  return regex_replace(toStdString(input), regex, toStdString(format), boost::format_sed);
}

#endif // USE_BOOST_REGEX
// ----------------------------------------------------------------------------- : Regex : common
