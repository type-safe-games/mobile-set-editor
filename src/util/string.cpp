//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/string.hpp>
#include <util/for_each.hpp>
#include <cerrno>
#include <climits>
#include <cstdarg>
#include <cwchar>
#include <clocale>
#include <locale.h>

// ----------------------------------------------------------------------------- : Char functions

#ifdef CHAR_FUNCTIONS_ARE_SLOW

Char toLower(Char c) {
  if (c <= 128) {
    if (c >= _('A') && c <= _('Z')) return c + (_('a') - _('A'));
    else                            return c;
  } else {
    return IF_UNICODE( towlower(c) , tolower(c) );
  }
}

Char toUpper(Char c) {
  if (c <= 128) {
    if (c >= _('a') && c <= _('z')) return c + (_('A') - _('a'));
    else                            return c;
  } else {
    return IF_UNICODE( towupper(c) , toupper(c) );
  }
}

#endif

// ----------------------------------------------------------------------------- : String utilities

StringView trim(StringView s) {
  String::const_iterator begin = s.begin();
  String::const_iterator end = s.end();
  while (begin != end && isSpace(*begin)) ++begin;
  while (begin != end && isSpace(*(end - 1))) --end;
  return StringView(begin, end);
}

StringView trim_left(StringView s) {
  String::const_iterator begin = s.begin();
  String::const_iterator end = s.end();
  while (begin != end && isSpace(*begin)) ++begin;
  return StringView(begin, end);
}

String substr_replace(const String& input, size_t start, size_t end, const String& replacement) {
  return input.substr(0,start) + replacement + input.substr(end);
}

String replace_all(const String& heystack, const String& needle, const String& replacement) {
  // as wxString::Replace: replace all non-overlapping occurrences, left to right; an empty needle replaces nothing
  if (needle.empty()) return heystack;
  String ret;
  size_t pos = 0;
  while (true) {
    size_t found = heystack.find(needle, pos);
    if (found == String::npos) break;
    ret.append(heystack, pos, found - pos);
    ret += replacement;
    pos = found + needle.size();
  }
  ret.append(heystack, pos, String::npos);
  return ret;
}

String reverse_string(String const& input) {
  // Note: std::reverse doesn't work because of unicode encoding stuff
  String reversed;
  for (auto it = input.rbegin(); it != input.rend(); ++it) {
    reversed += *it;
  }
  return reversed;
}

// ----------------------------------------------------------------------------- : Caseing

/// Quick check to see if the substring starting at the given iterator is equal to some given string
bool is_substr(const String& s, String::const_iterator it, const Char* cmp) {
  while (it != s.end() && *cmp != 0) {
    if (*it++ != *cmp++) return false;
  }
  return *cmp == 0;
}

void capitalize_in_place(String& s) {
  bool after_space = true;
  for (String::iterator it = s.begin(); it != s.end(); ++it) {
    if (*it == _(' ') || *it == _('/')) {
      after_space = true;
    } else if (after_space) {
      after_space = false;
      if (it != s.begin() &&
          (is_substr(it, s.end(), _("is ")) || is_substr(it, s.end(), _("the ")) ||
           is_substr(it, s.end(), _("in ")) || is_substr(it, s.end(), _("of "))  ||
           is_substr(it, s.end(), _("to ")) || is_substr(it, s.end(), _("at "))  ||
           is_substr(it, s.end(), _("a " )) || is_substr(it, s.end(), _("an ")) ||
           is_substr(it, s.end(), _("for ")) || is_substr(it, s.end(), _("and "))  ||
           is_substr(it, s.end(), _("nor ")) || is_substr(it, s.end(), _("but "))  ||
           is_substr(it, s.end(), _("or " )) || is_substr(it, s.end(), _("yet ")) ||
           is_substr(it, s.end(), _("so ")) || is_substr(it, s.end(), _("if "))  ||
           is_substr(it, s.end(), _("as ")) || is_substr(it, s.end(), _("by "))  ||
           is_substr(it, s.end(), _("off " )) || is_substr(it, s.end(), _("on ")) ||
           is_substr(it, s.end(), _("up ")) || is_substr(it, s.end(), _("via ")))) {
        // Short words are not capitalized, keep lower case
      } else {
        *it = toUpper(*it);
      }
    }
  }
}

void capitalize_sentence_in_place(String& s) {
  if (!s.empty()) {
    s[0] = toUpper(s[0]);
  }
}

void canonical_name_form_in_place(String& str) {
  for (String::iterator it = str.begin(); it != str.end(); ++it) {
    if (*it == ' ') *it = '_';
  }
}

void uncanonical_name_form_in_place(String& str) {
  for (String::iterator it = str.begin(); it != str.end(); ++it) {
    if (*it == '_') *it = ' ';
  }
}

String unified_form(String str) {
  str = trim(str);
  for (String::iterator it = str.begin(); it != str.end(); ++it) {
    if (*it == ' ') *it = '_';
    else *it = toLower(*it);
  }
  return str;
}

String name_to_caption(const String& str) {
  String ret;
  ret.reserve(str.size());
  bool leading = true, first = true;
  for (Char c : str) {
    if ((c == _('_') || c == _(' '))) {
      ret += leading ? c : _(' ');
    } else if (first) {
      // capitalize_sentence
      ret += toUpper(c);
      leading = false;
      first = false;
    } else {
      ret += c;
      leading = false;
    }
  }
  return ret;
}

String singular_form(const String& str) {
  assert(str.size() > 1);
  assert(str[str.size() - 1] == _('s')); // ends in 's'
  if (str.size() > 3 && is_substr(str, str.size()-3, _("ies"))) {
    return str.substr(0, str.size() - 3) + _("y");
  }
  return str.substr(0, str.size() - 1);
}

// ----------------------------------------------------------------------------- : Comparing / finding

// Nice unicode normalization tables, probably not conform the standards
char latin_1[] = "aaaaaaaceeeeiiii"
                 "dnooooo ouuuuy  "
                 "aaaaaaaceeeeiiii"
                 "dnooooo ouuuuy y";
char latin_A[] = "aaaaaaccccccccdd"
                 "ddeeeeeeeeeegggg"
                 "gggghhhhiiiiiiii"
                 "iiiijjkkklllllll"
                 "lllnnnnnnnnnoooo"
                 "oooorrrrrrssssss"
                 "ssttttttuuuuuuuu"
                 "uuuuwwyyyzzzzzzs";
char latin_B[] = "bbbbbbcccdddddee"
                 "effgg iikkllmnno"
                 "oo  pprssssttttu"
                 "uuuyyzz         "
                 "    dddlllnnnaai"
                 "ioouuuuuuuuuueaa"
                 "aaaaggggkkoooo  "
                 "jdddgg  nnaaaaoo"
                 "aaaaeeeeiiiioooo"
                 "rrrruuuusstt  hh"
                 "nd  zzaaeeoooooo"
                 "ooyylntj  acclts"
                 "z  buveejjqqrryy";
char latin_E[] = "aabbbbbbccdddddd"
                 "ddddeeeeeeeeeeff"
                 "gghhhhhhhhhhiiii"
                 "kkkkkkllllllllmm"
                 "mmmmnnnnnnnnoooo"
                 "oooopppprrrrrrrr"
                 "sssssssssstttttt"
                 "ttuuuuuuuuuuvvvv"
                 "wwwwwwwwwwxxxxyy"
                 "zzzzzzhtwyas    "
                 "aaaaaaaaaaaaaaaa"
                 "aaaaaaaaeeeeeeee"
                 "eeeeeeeeiiiioooo"
                 "oooooooooooooooo"
                 "oooouuuuuuuuuuuu"
                 "uuyyyyyyyy      ";

/// Remove accents from a (lowercase) character
Char remove_accents(Char c) {
  char dec = ' ';
  if (c >= 0xC0) {
    if (c <= 0xFF) { // Latin 1
      dec = latin_1[c - 0xC0];
    } else if (c <= 0x17F) { // Latin extended A
      dec = latin_A[c - 0x100];
    } else if (c <= 0x24F) { // Latin extended B
      dec = latin_B[c - 0x180];
    } else if (c >= 0x1E00 && c <= 0x1EFF) { // Latin additional
      dec = latin_E[c - 0x1E00];
    }
    // (until #54 these ranges were off: everything from U+0181 to U+1DFF read latin_E at a
    // negative index, so non-Latin text sorted by whatever bytes lay before the table)
  }
  return dec == ' ' ? toLower(c) : dec;
}

/// Is c a precomposed character (not counting accent marks)
/** If so, returns the second character of the decomposition */
Char decompose_char2(Char c) {
  if (c <  0xC6) {
    return 0;
  } else if (c == 0xC6 || c == 0xE6 || c == 0x152 || c == 0x153 || c == 0x1E2 || c == 0x1E3 || c == 0x1FC || c == 0x1FD) {
    return _('e'); // "ae" or "oe"
  } else if (c == 0x132 || c == 0x133 || (c >= 0x1C7 && c <= 0x1CC)) {
    return _('j'); // "ij", "lj", "nj"
  } else if ((c >= 0x1C4 && c <= 0x1C6) || (c >= 0x1F1 && c <= 0x1F3)) {
    return _('z'); // "dz"
  } else {
    return 0;
  }
}

int smart_compare(const String& sa, const String& sb) {
  bool in_num = false; // are we inside a number?
  bool lt = false;     // is sa less than sb?
  bool eq = true;      // so far is everything equal?
  size_t na = sa.size(), nb = sb.size();
  size_t pa = 0, pb = 0;
  for (; pa < na && pb < nb ; ++pa, ++pb) {
    Char a = sa[pa], b = sb[pb];
  next:
    bool da = isDigit(a), db = isDigit(b);
    if (da && db) {
      // compare numbers
      in_num = true;
      if (eq && a != b) {
        eq = false;
        lt = a < b;
      }
    } else if (in_num && da) {
      // comparing numbers, one is longer, therefore it is greater
      return 1;
    } else if (in_num && db) {
      return -1;
    } else if (in_num && !eq) {
      // two numbers of the same length, but not equal
      return lt ? -1 : 1;
    } else if (a != b) {
      // not a number
      eq = true; lt = false;
      if (a >= 0x20 && b >= 0x20) {
        // compare characters
        Char la = remove_accents(a), lb = remove_accents(b);
        // Decompose characters
        Char la2 = decompose_char2(a), lb2 = decompose_char2(b);
        // Compare
        if (la < lb) return -1;
        if (la > lb) return 1;
        // Remaining from decomposition
        if (la2 || lb2) {
          if (la2) a = la2;
          else {
            if (++pa >= na) return 1;
            a = sa[pa];
          }
          if (lb2) b = lb2;
          else {
            if (++pb >= nb) return -1;
            b = sb[pb];
          }
          goto next; // don't move to the next character in both strings
        }
      } else {
        // control characters
        if (a < b) return -1;
        else       return 1;
      }
    }
    in_num = da && db;
  }
  // When we are at the end; shorter strings come first
  // This is true for normal string collation
  // and also when both end in a number and another digit follows
  if (in_num) {
    if (na - pa < nb - pb) {
      // number b continues?
      Char b = sb[pb];
      if (isDigit(b) || eq) return -1; // b is longer
    } else if (na - pa > nb - pb) {
      Char a = sa[pa];
      if (isDigit(a) || eq) return 1;  // a is longer
    }
    return eq ? 0 : lt ? -1 : 1; // compare numbers
  } else {
    return na - pa == nb - pb ? 0
         : na - pa <  nb - pb ? -1 : 1; // outside number, shorter string comes first
  }
}
bool smart_less(const String& sa, const String& sb) {
  return smart_compare(sa, sb) == -1;
}
bool smart_equal(const String& sa, const String& sb) {
  return smart_compare(sa, sb) == 0;
}

bool starts_with(const String& str, const String& start) {
  if (str.size() < start.size()) return false;
  return equal(start.begin(), start.end(), str.begin());
}


bool is_substr_i(const String& str, size_t pos, const Char* cmp) {
  for (String::const_iterator it = str.begin() + pos ; *cmp && it < str.end() ; ++cmp, ++it) {
    if (toLower(*cmp) != toLower(*it)) return false;
  }
  return *cmp == _('\0');
}
bool is_substr_i(const String& str, size_t pos, const String& cmp) {
  return is_substr_i(str, pos, static_cast<const Char*>(cmp.c_str()));
}

bool canonical_name_compare(StringView as, const Char* b) {
  assert(canonical_name_form(b) == b);
  for (String::const_iterator a_it = as.begin(); a_it != as.end(); ++a_it, ++b) {
    if (*a_it != *b && !(*a_it == '_' && *b == ' ')) {
      return false;
    }
  }
  return true;
}

size_t find_i(const String& heystack, const String& needle) {
  if (needle.empty()) return 0;
  for (size_t i = 0 ; i + needle.size() <= heystack.size() ; ++i) {
    if (is_substr_i(heystack, i, needle)) return true;
  }
  return String::npos;
}

// ----------------------------------------------------------------------------- : wxString replacements

String to_lower(const String& s) {
  String ret = s;
  for (size_t i = 0 ; i < ret.size() ; ++i) {
    ret[i] = toLower(ret[i]);
  }
  return ret;
}

String to_upper(const String& s) {
  String ret = s;
  for (size_t i = 0 ; i < ret.size() ; ++i) {
    ret[i] = toUpper(ret[i]);
  }
  return ret;
}

// Whitespace as wxString::Trim sees it: isspace, but only for ASCII characters
static inline bool is_ascii_space(Char c) {
  return c == _(' ') || (c >= 0x09 && c <= 0x0D);
}

String trim_ascii_left(const String& s) {
  size_t start = 0;
  while (start < s.size() && is_ascii_space(s[start])) ++start;
  return s.substr(start);
}

String trim_ascii_right(const String& s) {
  size_t end = s.size();
  while (end > 0 && is_ascii_space(s[end - 1])) --end;
  return s.substr(0, end);
}

// ----------------------------------------------------------------------------- : Numbers

namespace {
  // Use the "C" locale for number conversions in the current thread while this object exists
  #if defined(__GLIBC__) || defined(__ANDROID__) || defined(__APPLE__)
    locale_t c_locale() {
      static locale_t loc = newlocale(LC_ALL_MASK, "C", (locale_t)0);
      return loc;
    }
    struct CLocaleScope {
      CLocaleScope() : old(c_locale() ? uselocale(c_locale()) : (locale_t)0) {}
      ~CLocaleScope() { if (old) uselocale(old); }
      locale_t old;
    };
  #else
    struct CLocaleScope {}; // TODO: Windows (_configthreadlocale), the desktop build there is not used
  #endif
}

// These follow WX_STRING_TO_X_TYPE_START/END in wx/src/common/string.cpp

bool parse_long(const String& s, long& out, int base) {
  CLocaleScope c_locale;
  const wchar_t* start = s.c_str();
  wchar_t* end;
  errno = 0;
  long value = wcstol(start, &end, base);
  if (end == start || errno == ERANGE) return false;
  out = value;
  return *end == 0;
}

bool parse_ulong(const String& s, unsigned long& out, int base) {
  CLocaleScope c_locale;
  const wchar_t* start = s.c_str();
  wchar_t* end;
  errno = 0;
  unsigned long value = wcstoul(start, &end, base);
  if (end == start || errno == ERANGE) return false;
  out = value;
  return *end == 0;
}

bool parse_int(const String& s, int& out, int base) {
  CLocaleScope c_locale;
  const wchar_t* start = s.c_str();
  wchar_t* end;
  errno = 0;
  long value = wcstol(start, &end, base);
  if (end == start || errno == ERANGE || value < INT_MIN || value > INT_MAX) return false;
  out = (int)value;
  return *end == 0;
}

bool parse_double(const String& s, double& out) {
  CLocaleScope c_locale;
  const wchar_t* start = s.c_str();
  wchar_t* end;
  errno = 0;
  double value = wcstod(start, &end);
  if (end == start || errno == ERANGE) return false;
  out = value;
  return *end == 0;
}

String int_to_string(long long i) {
  return String(std::to_wstring(i));
}

String double_to_string(double d) {
  return format_string(_("%g"), d);
}

// ----------------------------------------------------------------------------- : Formatting

// Convert a format string the way wxFormatString did for wchar_t printf on Unix:
// %s and %c without a size modifier become %ls and %lc
static std::wstring convert_format(const wchar_t* format) {
  std::wstring ret;
  for (const wchar_t* p = format ; *p ; ) {
    ret += *p;
    if (*p++ != L'%') continue;
    if (*p == L'%') { ret += *p++; continue; }
    // flags, width, precision, argument position
    while (*p && wcschr(L"0123456789-+ #'.*$", *p)) ret += *p++;
    // size modifiers
    bool has_size = false;
    while (*p && wcschr(L"hlLqjztI", *p)) { ret += *p++; has_size = true; }
    if (!has_size && (*p == L's' || *p == L'c')) ret += L'l';
  }
  return ret;
}

String string_format_detail::format_string_impl(const wchar_t* format, ...) {
  std::wstring fmt = convert_format(format);
  CLocaleScope c_locale;
  std::vector<wchar_t> buffer(256);
  while (true) {
    va_list args;
    va_start(args, format);
    int len = vswprintf(buffer.data(), buffer.size(), fmt.c_str(), args);
    va_end(args);
    if (len >= 0) return String(buffer.data(), (size_t)len);
    // vswprintf fails if the buffer is too small (or on a hard error)
    if (buffer.size() >= 16 * 1024 * 1024) return String();
    buffer.resize(buffer.size() * 2);
  }
}

// ----------------------------------------------------------------------------- : UTF-8

// Length of a UTF-8 sequence from its lead byte, 0 for bytes that can't start one
// (same table as wxMBConvStrictUTF8: C0, C1 and F5..FF are invalid)
static inline unsigned utf8_sequence_length(unsigned char c) {
  if (c < 0x80) return 1;
  if (c < 0xC2) return 0;
  if (c < 0xE0) return 2;
  if (c < 0xF0) return 3;
  if (c < 0xF5) return 4;
  return 0;
}

static inline void append_utf8(std::string& out, uint32_t code) {
  if (code <= 0x7F) {
    out += (char)code;
  } else if (code <= 0x7FF) {
    out += (char)(0xC0 | (code >> 6));
    out += (char)(0x80 | (code & 0x3F));
  } else if (code <= 0xFFFF) {
    out += (char)(0xE0 | (code >> 12));
    out += (char)(0x80 | ((code >> 6) & 0x3F));
    out += (char)(0x80 | (code & 0x3F));
  } else {
    out += (char)(0xF0 | (code >> 18));
    out += (char)(0x80 | ((code >> 12) & 0x3F));
    out += (char)(0x80 | ((code >> 6) & 0x3F));
    out += (char)(0x80 | (code & 0x3F));
  }
}

std::string encode_utf8(const String& str) {
  std::string out;
  out.reserve(str.size());
  const wchar_t* p   = str.c_str();
  const wchar_t* end = p + str.size();
  while (p < end) {
    uint32_t code = (uint32_t)*p++;
    #if WCHAR_MAX <= 0xFFFF
      // UTF-16: combine surrogate pairs, a lone surrogate can't be encoded
      code &= 0xFFFF;
      if (code >= 0xD800 && code <= 0xDFFF) {
        if (code >= 0xDC00 || p == end || (uint32_t)(*p & 0xFFFF) < 0xDC00 || (uint32_t)(*p & 0xFFFF) > 0xDFFF) return std::string();
        code = 0x10000 + ((code - 0xD800) << 10) + ((uint32_t)(*p++ & 0xFFFF) - 0xDC00);
      }
    #else
      code &= 0x7FFFFFFF;
      if (code > 0x10FFFF) return std::string();
    #endif
    append_utf8(out, code);
  }
  return out;
}

bool decode_utf8(const char* data, size_t size, String& out) {
  std::wstring result;
  result.reserve(size);
  const unsigned char* p   = (const unsigned char*)data;
  const unsigned char* end = p + size;
  while (p < end) {
    unsigned char c = *p;
    unsigned len = utf8_sequence_length(c);
    if (len == 0 || (size_t)(end - p) < len) return false;
    uint32_t code;
    if (len == 1) {
      code = c;
    } else {
      static const unsigned char lead_mask[] = { 0x7F, 0x1F, 0x0F, 0x07 };
      code = c & lead_mask[len - 1];
      for (unsigned i = 1 ; i < len ; ++i) {
        unsigned char cc = p[i];
        if ((cc & 0xC0) != 0x80) return false;
        code = (code << 6) | (cc & 0x3F);
      }
    }
    p += len;
    #if WCHAR_MAX <= 0xFFFF
      if (code >= 0x10000) {
        code -= 0x10000;
        result += (wchar_t)(0xD800 + (code >> 10));
        result += (wchar_t)(0xDC00 + (code & 0x3FF));
        continue;
      }
    #endif
    result += (wchar_t)code;
  }
  out = String(result);
  return true;
}

String decode_utf8(const char* data, size_t size) {
  String ret;
  if (!decode_utf8(data, size, ret)) return String();
  return ret;
}

String decode_utf8_or_latin1(const char* data, size_t size) {
  String ret;
  if (decode_utf8(data, size, ret)) return ret;
  ret.clear();
  for (size_t i = 0 ; i < size ; ++i) {
    ret += static_cast<Char>(static_cast<unsigned char>(data[i]));
  }
  return ret;
}

// ----------------------------------------------------------------------------- : Regular expressions

/// Escape a single character for use in regular expressions
String regex_escape(Char c) {
  if (c == _('(') || c == _(')') || c == _('[') || c == _(']') || c == _('{') ||
      c == _('.') || c == _('^') || c == _('$') || c == _('#') || c == _('\\') ||
      c == _('|') || c == _('+') || c == _('*') || c == _('?')) {
    // c needs to be escaped
    return _("\\") + String(1,c);
  } else {
    return String(1,c);
  }
}
/// Escape a string for use in regular expressions
String regex_escape(const String& s) {
  String ret;
  FOR_EACH_CONST(c,s) ret += regex_escape(static_cast<Char>(c));
  return ret;
}

String make_non_capturing(const String& re) {
  String ret;
  bool escape = false, bracket = false, capture = false;
  FOR_EACH_CONST(c, re) {
    if (capture) {
      if (c != _('?')) {
        // change this capture into a non-capturing "(" by appending "?:"
        ret += _("?:");
      }
      capture = false;
    }
    if (escape) { // second char of escape sequence
      escape = false;
    } else if (c == _('\\')) { // start of escape sequence
      escape = true;
    } else if (c == _('[')) { // start of [...]
      bracket = true;
    } else if (c == _(']')) { // end of [...]
      bracket = false;
    } else if (bracket && c == _('(')) {
      // wx has a bug, it counts the '(' in "[(]" as a matching group
      // escape it so wx doesn't see it
      ret += _('\\');
    } else if (c == _('(')) { // start of capture?
      capture = true;
    }
    ret += c;
  }
  return ret;
}
