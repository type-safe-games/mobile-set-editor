//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>

// Use boost::regex as opposed to wxRegex
/* 2008-09-01:
 *     Script profiling shows that the boost library is significantly faster:
 *     When loading a large magic set (which calls ScriptManager::updateAll):
 *            function      Calls   wxRegEx    boost
 *            ------------------------------------------------------------------
 *            replace        3791   0.38607    0.20857
 *            filter_text      11   0.32251    0.02446
 *
 *            (times are avarage over all calls, in ms)
 */
#define USE_BOOST_REGEX 1

#if USE_BOOST_REGEX
  #include <boost/regex.hpp>
  #include <boost/regex/pattern_except.hpp>
#endif

// ----------------------------------------------------------------------------- : Boost implementation

#if USE_BOOST_REGEX
  /// Our own regular expression wrapper
  /** Has an interface like boost::regex, but compatible with wxStrings.
   */
  class Regex {
  public:
    struct Results : public boost::match_results<String::const_iterator> {
      /// Get a sub match
      inline String str(int sub = 0) const {
        const_reference v = (*this)[sub];
        return String(v.first, v.second);
      }
      /// Format a replacement string
      inline String format(const String& format) const {
        std::basic_string<Char> fmt(format.begin(),format.end());
        String output;
        boost::match_results<String::const_iterator>::format(
          insert_iterator<String>(output, output.end()), fmt, boost::format_sed);
        return output;
      }
    };
    
    inline Regex() {}
    inline Regex(const String& code) { assign(code); }
    
    void assign(const String& code);
    inline bool matches(const String& str) const {
      return regex_search(str, regex);
    }
    inline bool matches(Results& results, const String& str, size_t start = 0) const {
      return matches(results, str.begin() + start, str.end());
    }
    inline bool matches(Results& results, const String::const_iterator& begin, const String::const_iterator& end) const {
      return regex_search(begin, end, results, regex);
    }
    String replace_all(const String& input, const String& format) const;
    
    inline bool empty() const {
      return regex.empty();
    }
    
  private:
    boost::basic_regex<Char> regex; ///< The regular expression
  };

#endif

