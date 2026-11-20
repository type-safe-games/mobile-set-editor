//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

/** @file util/string.hpp
 *
 *  @brief String and character utility functions and macros
 */

// ----------------------------------------------------------------------------- : Includes

#include "prec.hpp"
#include "for_each.hpp"
#include <ctype.h>
#include <string>
#include <type_traits>
#include <boost/preprocessor/cat.hpp>

// ----------------------------------------------------------------------------- : String type

/// The string type used throughout MSE
typedef std::wstring String;

// ----------------------------------------------------------------------------- : Unicode

/// u if UNICODE is defined, a otherwise
#ifdef UNICODE
#  define IF_UNICODE(u,a) u
#else
#  define IF_UNICODE(u,a) a
#endif

#undef _
/// A string/character constant, correctly handled in unicode builds
#define _(S) IF_UNICODE(BOOST_PP_CAT(L,S), S)

/// The character type used
typedef wchar_t Char;

/// UTF-8 Byte order mark for writing at the start of files
/** In non-unicode builds it is UTF8 encoded \xFEFF.
 *  In unicode builds it is a normal \xFEFF.
 */
const wchar_t BYTE_ORDER_MARK[] = L"\xFEFF";

/// Some constants we like to use
#ifdef UNICODE
  #define  LEFT_ANGLE_BRACKET _("\x2039")
  #define RIGHT_ANGLE_BRACKET _("\x203A")
  #define  LEFT_SINGLE_QUOTE  _('\x2018')
  #define RIGHT_SINGLE_QUOTE  _('\x2019')
  #define  LEFT_DOUBLE_QUOTE  _('\x201C')
  #define RIGHT_DOUBLE_QUOTE  _('\x201D')
  #define EN_DASH             _('\x2013')
  #define EM_DASH             _('\x2014')
  #define CONNECTION_SPACE    _('\xEB00') // in private use area, untags to ' '
#else
  #define  LEFT_ANGLE_BRACKET _("<")
  #define RIGHT_ANGLE_BRACKET _(">")
  #define  LEFT_SINGLE_QUOTE  _('\'')
  #define RIGHT_SINGLE_QUOTE  _('\'')
  #define  LEFT_DOUBLE_QUOTE  _('\"')
  #define RIGHT_DOUBLE_QUOTE  _('\"')
  #define EN_DASH             _('-') // 150?
  #define EM_DASH             _('-') // 151?
  #define CONNECTION_SPACE    _(' ') // too bad
#endif

// ----------------------------------------------------------------------------- : Char functions

// Character set tests
inline bool isAlpha(Char c) { return IF_UNICODE( iswalpha(c) , isalpha((unsigned char)c) ); }
inline bool isDigit(Char c) { return IF_UNICODE( iswdigit(c) , isdigit((unsigned char)c) ); }
inline bool isAlnum(Char c) { return IF_UNICODE( iswalnum(c) , isalnum((unsigned char)c) ); }
inline bool isUpper(Char c) { return IF_UNICODE( iswupper(c) , isupper((unsigned char)c) ); }
inline bool isLower(Char c) { return IF_UNICODE( iswlower(c) , islower((unsigned char)c) ); }
inline bool isPunct(Char c) { return IF_UNICODE( iswpunct(c) , ispunct((unsigned char)c) ); }
// Character conversions
#ifdef _MSC_VER
  #define CHAR_FUNCTIONS_ARE_SLOW
#endif
#ifdef CHAR_FUNCTIONS_ARE_SLOW
  // These functions are slow as hell on msvc.
  // If also in other compilers, they can also use these routines.
  Char toLower(Char c);
  Char toUpper(Char c);
  inline bool isSpace(Char c) {
    if (c <= 128) {
      return (c >= 0x09 && c <= 0x0D) || c == 0x20;
    } else {
      return IF_UNICODE( iswspace(c) , isspace((unsigned char)c) ) || c == CONNECTION_SPACE;
    }
  }
#else
  inline Char toLower(Char c) { return IF_UNICODE( towlower(c) , tolower(c) ); }
  inline Char toUpper(Char c) { return IF_UNICODE( towupper(c) , toupper(c) ); }
  inline bool isSpace(Char c) { return IF_UNICODE( iswspace(c) , isspace((unsigned char)c) ) || c == CONNECTION_SPACE; }
#endif

// ----------------------------------------------------------------------------- : String view

// A view of (part of a string)
class StringView {
public:
  StringView(String const& str)
    : begin_(str.begin()), end_(str.end())
  {}
  StringView(String const& str, size_t pos)
    : begin_(str.begin() + pos), end_(str.end())
  {}
  StringView(String const& str, size_t pos, size_t count)
    : begin_(str.begin() + pos), end_(str.begin() + pos + min(count, str.size()-pos))
  {}
  StringView(String::const_iterator begin, String::const_iterator end)
    : begin_(begin), end_(end)
  {
    assert(begin <= end);
  }
  inline operator String () const {
    return String(begin_, end_);
  }
  using iterator = String::const_iterator;
  using const_iterator = String::const_iterator;
  inline String::const_iterator begin() const {
    return begin_;
  }
  inline String::const_iterator end() const {
    return end_;
  }
  inline size_t size() const {
    return end_ - begin_;
  }
  inline bool empty() const {
    return begin() == end();
  }
  inline bool operator == (StringView const& str) {
    return str.size() == size() && std::equal(begin(), end(), str.begin());
  }
  template <typename AnyChar>
  inline bool operator == (const AnyChar* str) {
    String::const_iterator it = begin_;
    while (true) {
      if (it == end_) return *str == '\0';
      if (*str == '\0') return false;
      if (*str != *it) return false;
      ++it; ++str;
    }
  }
private:
  String::const_iterator begin_, end_;
};

inline String& operator += (String& a, StringView b) {
  return a.append(b.begin(), b.end());
}

inline StringView substr(String const& str, size_t pos, size_t len) {
  return StringView(str, pos, len);
}
inline StringView substr(String const& str, size_t pos) {
  return StringView(str, pos);
}

// ----------------------------------------------------------------------------- : String utilities

/// Remove whitespace from both ends of a string
StringView trim(StringView);

/// Remove whitespace from the start of a string
StringView trim_left(StringView);

/// Replace the substring [start...end) of 'input' with 'replacement'
String substr_replace(const String& input, size_t start, size_t end, const String& replacement);

/// Replace all occurences of one needle with replacement
String replace_all(const String& heystack, const String& needle, const String& replacement);

/// Reverses a string, Note: std::reverse doesn't work with wxString
String reverse_string(String const& input);

// ----------------------------------------------------------------------------- : Caseing

/// Make each word in a string start with an upper case character.
/** for use in menus */
void capitalize_in_place(String&);
inline String capitalize(String const& s) {
  String result = s;
  capitalize_in_place(result);
  return result;
}

/// Make the first word in a string start with an upper case character.
/** for use in dialogs */
void capitalize_sentence_in_place(String&);
inline String capitalize_sentence(String const& s) {
  String result = s;
  capitalize_sentence_in_place(result);
  return result;
}

/// Convert a field name to canonical form
/** - converts ' ' to '_'
 */
void canonical_name_form_in_place(String&);
inline String canonical_name_form(String s) {
  canonical_name_form_in_place(s);
  return s;
}

/// Undo canonical_name_form: replace '_' by ' '
void uncanonical_name_form_in_place(String&);
inline String uncanonical_name_form(String s) {
  uncanonical_name_form_in_place(s);
  return s;
}

/// Convert a field name to canonical form, then to lower case, then trim it
String unified_form(String);

/// Convert a field name to a string that can be shown to the user
String name_to_caption(const String&);

/// Returns the singular form of a string
/** Used for reflection, for example "vector<T> apples" is written with keys
 *  singular_form("apples"), which is "apple"
 */
String singular_form(const String&);

// ----------------------------------------------------------------------------- : Comparing / finding

/// Compare two strings
/** Uses a smart comparison algorithm that understands numbers. 
 *  The comparison is case insensitive.
 *  Doesn't handle leading zeros.
 *
 *  Returns -1 if a < b, 0 if they are equal, and 1 if a > b
 */
int smart_compare(const String&, const String&);
/// Compare two strings, is the first less than the first?
bool smart_less(const String&, const String&);
/// Compare two strings for equality
bool smart_equal(const String&, const String&);

/// Return whether str starts with start
/** starts_with(a,b) == is_substr(a,0,b) */
//bool starts_with(const String& str, const String& start);
inline bool starts_with(StringView str, StringView const& start) {
  return str.size() >= start.size() && std::equal(start.begin(), start.end(), str.begin());
}
template <typename AnyChar>
inline bool starts_with(StringView str, const AnyChar* start) {
  String::const_iterator it = str.begin();
  while (true) {
    if (*start == '\0') return true;
    if (it == str.end()) return false;
    if (*start != *it) return false;
    ++it; ++start;
  }
}

/// Return whether str contains the string cmp at position pos
template <typename Cmp>
inline bool is_substr(const String& str, size_t pos, const Cmp& cmp) {
  return starts_with(StringView(str, pos), cmp);
}
/// Return whether begin..end contains the string cmp at position begin
template <typename It, typename Cmp>
inline bool is_substr(It begin, It end, const Cmp& cmp) {
  return starts_with(StringView(begin, end),cmp);
}

/// Return whether str contains the string cmp at position pos, case insensitive compare
bool is_substr_i(const String& str, size_t pos, const Char* cmp);
/// Return whether str contains the string cmp at position pos, case insensitive compare
bool is_substr_i(const String& str, size_t pos, const String& cmp);

/// Case insensitive string search, returns String::npos if not found
size_t find_i(const String& heystack, const String& needle);

/// Compare two strings for equality, a may contain '_' where b contains ' '
/** canoncial_name_compare(a,b) == (cannocial_name_form(a) == b)
 *  b should already be in cannonical name form
 */
bool canonical_name_compare(StringView a, const Char* b);

// ----------------------------------------------------------------------------- : wxString replacements

/* Free functions replacing the wxString members the core uses (issue #30, ADR 0002).
 *
 * They were written with only the operations wxString and std::wstring have in common, so the String
 * typedef could flip; since #30 stage 2 String is std::wstring. Number conversions use the "C"
 * locale whatever the global locale is; the CLI and golden runs already use "C", so output is unchanged.
 *
 * wxString members used by the core when #30 started (all core dirs, call counts), and their replacements:
 *    GetChar(i)            87  s[i]
 *    String::Format(...)  156  format_string(fmt, ...)      (also wxString::Format; FormatV was only in locale.hpp)
 *    operator << (x)       ~40 s + int_to_string(x) / double_to_string(x) / s + other_string
 *                              (<< int/long/unsigned is %d/%ld/%u, << double is %g, << float is %f!)
 *    ToStdString()         16  encode_utf8(s) (was the C locale encoding: differs only for non-ASCII)
 *    ToStdString(wxConvUTF8), ToUTF8(), utf8_str()   encode_utf8(s)
 *    String::FromUTF8(p,n) 18  decode_utf8(p, n)
 *    mb_str()              14  to_fs_path(s) for paths (util/file_utils.hpp); encode_utf8 otherwise
 *    wc_str(), fn_str()        c_str() / to_fs_path(s)
 *    ToStdWstring()         4  std::wstring(s.begin(), s.end()) -- or just s once String is std::wstring
 *    ToLong(&l)            13  parse_long(s, l)
 *    ToULong(&l)            1  parse_ulong(s, l)
 *    ToInt(&i)              5  parse_int(s, i)
 *    ToDouble(&d)           9  parse_double(s, d)
 *    EndsWith(x)           11  ends_with(s, x)
 *    StartsWith(x)          3  starts_with(s, x)
 *    Contains(x)            3  contains(s, x)
 *    Replace(a, b)          7  replace_all(s, a, b)          (wx replaced all occurrences by default)
 *    Lower(), Upper()       9  to_lower(s), to_upper(s)
 *    MakeUpper()            1  s = to_upper(s)
 *    Trim(true/false)       3  trim_ascii_right(s) / trim_ascii_left(s)  NB: Trim() modifies s in place
 *    Last(), RemoveLast()   6  s[s.size() - 1], s.erase(s.size() - 1)
 *    SubString(a, b)        3  s.substr(a, b - a + 1)        (Mid/SubString clamp; substr throws if a > size)
 *    Mid(a, n), Left(n)     2  s.substr(a, n), s.substr(0, n)
 *    Clone(), Clear()       2  s, s.clear()
 *    IsSameAs(x)            1  ==   (gfx: that one is wxImage, not a string)
 *    wxUniChar, wxUniCharRef   Char, auto&& (iterating a wxString yields wxUniCharRef proxies)
 *    wxT("..")                 _("..")
 *    wxSscanf, wxStrchr        swscanf (beware: locale dependent), wcschr
 *    String(p, wxConvUTF8)     decode_utf8; wxConvWhateverWorks / wxConvFileName: decode_utf8_or_latin1
 *
 * At the boundary with wx (GUI, and the wx image adapter / the colour database in the core), std::wstring
 * converts implicitly to wxString; the other way is wxString::ToStdWstring().
 */

/// Lower case version of a string, converting each character with toLower (as wxString::Lower)
String to_lower(const String& s);
/// Upper case version of a string, converting each character with toUpper (as wxString::Upper)
String to_upper(const String& s);

/// Return whether str ends with suffix
inline bool ends_with(StringView str, StringView const& suffix) {
  return str.size() >= suffix.size() && std::equal(suffix.begin(), suffix.end(), str.end() - suffix.size());
}
template <typename AnyChar>
inline bool ends_with(StringView str, const AnyChar* suffix) {
  size_t n = 0;
  while (suffix[n] != '\0') ++n;
  if (str.size() < n) return false;
  String::const_iterator it = str.end() - n;
  for (size_t i = 0 ; i < n ; ++i, ++it) {
    if (suffix[i] != *it) return false;
  }
  return true;
}

/// Does str contain needle? An empty needle is contained in every string (as wxString::Contains).
inline bool contains(const String& str, const String& needle) {
  return str.find(needle) != String::npos;
}

/// Remove ASCII whitespace (' ', \t, \n, \v, \f, \r) from the start, as wxString::Trim(false)
/** Unlike trim_left, other Unicode spaces and CONNECTION_SPACE are kept. */
String trim_ascii_left(const String& s);
/// Remove ASCII whitespace (' ', \t, \n, \v, \f, \r) from the end, as wxString::Trim(true)
String trim_ascii_right(const String& s);

// ----------------------------------------------------------------------------- : Numbers

// Parsing works like wxString::ToLong etc.: leading whitespace and a sign are allowed (wcstol/wcstod),
// false is returned if nothing could be parsed or the value is out of range (out is unchanged then),
// and if only a prefix of s is a number, out is set to it but false is returned.

/// Parse an integer, as wxString::ToLong
bool parse_long(const String& s, long& out, int base = 10);
/// Parse an unsigned integer, as wxString::ToULong (a leading '-' is accepted, as by wcstoul)
bool parse_ulong(const String& s, unsigned long& out, int base = 10);
/// Parse an int, as wxString::ToInt (false and out unchanged if the value doesn't fit in an int)
bool parse_int(const String& s, int& out, int base = 10);
/// Parse a floating point number with '.' as the decimal point, as wxString::ToDouble in the "C" locale
bool parse_double(const String& s, double& out);

/// Format an integer in decimal, as String() << i
String int_to_string(long long i);
/// Format a double as "%g", as String() << d
String double_to_string(double d);

// ----------------------------------------------------------------------------- : Formatting

namespace string_format_detail {
  inline const wchar_t* format_arg(const String& s) { return s.c_str(); }
  inline const wchar_t* format_arg(const wchar_t* s) { return s; }
  template <typename T, typename = std::enable_if_t<std::is_arithmetic<T>::value || std::is_enum<T>::value>>
  inline T format_arg(T x) { return x; }
  /// Other pointers are formatted with %p
  template <typename T, typename = std::enable_if_t<!std::is_same<std::remove_cv_t<T>, char>::value && !std::is_same<std::remove_cv_t<T>, wchar_t>::value>>
  inline const void* format_arg(T* p) { return p; }
  String format_string_impl(const wchar_t* format, ...);
}

/// Format a string, printf style, as wxString::Format did
/** String and wchar_t* arguments go with %s, Char arguments with %c; these are converted to %ls and %lc
 *  (wx did the same). Numbers are formatted in the "C" locale.
 */
template <typename... Args>
inline String format_string(const String& format, const Args&... args) {
  return string_format_detail::format_string_impl(format.c_str(), string_format_detail::format_arg(args)...);
}
/// Overload so that a braced {} argument still means an empty String
inline String format_string(const String& format, const String& a0) {
  return string_format_detail::format_string_impl(format.c_str(), string_format_detail::format_arg(a0));
}

// ----------------------------------------------------------------------------- : UTF-8

// The conversions behave like wxConvUTF8 (wxMBConvStrictUTF8), which the core used before.

/// Encode a string as UTF-8. Returns "" if it contains a code point above U+10FFFF (wx did the same).
std::string encode_utf8(const String& str);
/// Decode UTF-8 bytes. Returns false if they are not valid UTF-8.
/** Like wx, this only checks lead and continuation bytes, so overlong forms and encoded surrogates are accepted. */
bool decode_utf8(const char* data, size_t size, String& out);
/// Decode UTF-8 bytes. Returns "" if they are not valid UTF-8 (as String::FromUTF8).
String decode_utf8(const char* data, size_t size);
inline String decode_utf8(const std::string& data) {
  return decode_utf8(data.data(), data.size());
}
/// Decode bytes as UTF-8, or as ISO-8859-1 (one byte per character) if they are not valid UTF-8.
/** This is what wxConvWhateverWorks and wxConvFileName (on Linux) did in the "C" and UTF-8 locales:
 *  their middle step, the locale's multibyte encoding, fails exactly where UTF-8 does in those locales.
 */
String decode_utf8_or_latin1(const char* data, size_t size);

// ----------------------------------------------------------------------------- : Regular expressions

/// Escape a single character for use in regular expressions
String regex_escape(Char c);
/// Escape a string for use in regular expressions
String regex_escape(const String& s);

/// Make sure the given regex does no capturing
/** Basicly replaces "(" with "(?:" */
String make_non_capturing(const String& re);

// ----------------------------------------------------------------------------- : Iterator utilities

struct end_sentinel_t {};
const end_sentinel_t end_sentinel;

// Iterate over a string, removing all matching substrings.
// match.operator(it,end) should return false or return true and advance it past the substring
template <typename It, typename End, typename Match>
struct SkipSubstringIterator {
public:
  SkipSubstringIterator(It it, End end, Match const& match) : it(it), end(end), match(match) {
    while (match(it, end));
  }
  bool operator == (end_sentinel_t) const {
    return it == end;
  }
  bool operator != (end_sentinel_t) const {
    return it != end;
  }
  auto operator * () const {
    return *it;
  }
  auto& operator ++ () {
    ++it;
    while (match(it, end));
    return *this;
  }
private:
  It it;
  End end;
  Match match;
};

template <typename It, typename End, typename Match>
inline SkipSubstringIterator<It,End,Match> skip_substring_iterator(It it, End end, Match const& match) {
  return SkipSubstringIterator<It,End,Match>(it, end, match);
}
