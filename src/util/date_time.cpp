//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/date_time.hpp>
#include <cwchar>
#include <cwctype>
#include <string>

using namespace std::chrono;

// ----------------------------------------------------------------------------- : Conversion

namespace {
  bool local_tm(time_t t, struct tm& out) {
    #ifdef _WIN32
      return localtime_s(&out, &t) == 0;
    #else
      return localtime_r(&t, &out) != nullptr;
    #endif
  }
}

DateTime DateTime::now() {
  return DateTime(time_point_cast<milliseconds>(Clock::now()));
}

DateTime DateTime::fromTimeT(time_t t) {
  return DateTime(TimePoint(seconds(t)));
}

DateTime DateTime::fromLocal(int year, int month, int day, int hour, int minute, int second, int millisecond) {
  struct tm tm = {};
  tm.tm_year  = year - 1900;
  tm.tm_mon   = month - 1;
  tm.tm_mday  = day;
  tm.tm_hour  = hour;
  tm.tm_min   = minute;
  tm.tm_sec   = second;
  tm.tm_isdst = -1; // let the C library decide whether DST applies
  tm.tm_wday  = -1; // mktime sets this on success, -1 is also a valid time_t
  time_t t = mktime(&tm);
  if (tm.tm_wday < 0) return DateTime();
  return DateTime(TimePoint(seconds(t)) + milliseconds(millisecond));
}

time_t DateTime::toTimeT() const {
  return (time_t)floor<seconds>(time).time_since_epoch().count();
}

int DateTime::millisecond() const {
  return (int)(time - floor<seconds>(time)).count();
}

struct tm DateTime::toLocalTm() const {
  struct tm tm = {};
  local_tm(toTimeT(), tm);
  return tm;
}

DateTime DateTime::startOfDay() const {
  if (!isValid()) return *this;
  struct tm tm = toLocalTm();
  return fromLocal(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
}

DateTime DateTime::addDays(int days) const {
  if (!isValid()) return *this;
  struct tm tm = toLocalTm();
  return fromLocal(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday + days, tm.tm_hour, tm.tm_min, tm.tm_sec, millisecond());
}

// ----------------------------------------------------------------------------- : Formating

String DateTime::format(const String& fmt_in) const {
  if (!isValid()) return String();
  // wx extension: %l is the milliseconds, strftime doesn't know it
  std::wstring fmt = fmt_in;
  std::wstring fmt2;
  fmt2.reserve(fmt.size());
  for (size_t i = 0; i < fmt.size(); ++i) {
    if (fmt[i] == L'%' && i + 1 < fmt.size()) {
      if (fmt[i+1] == L'l') {
        wchar_t ms[8];
        swprintf(ms, 8, L"%03d", millisecond());
        fmt2 += ms;
      } else if (fmt[i+1] == L'Y') {
        // wx always wrote at least 4 digits, strftime doesn't pad years before 1000
        wchar_t year[16];
        swprintf(year, 16, L"%04d", toLocalTm().tm_year + 1900);
        fmt2 += year;
      } else {
        fmt2 += fmt[i];
        fmt2 += fmt[i+1];
      }
      ++i;
    } else {
      fmt2 += fmt[i];
    }
  }
  if (fmt2.empty()) return String();
  struct tm tm = toLocalTm();
  // grow the buffer until the result fits
  for (size_t size = 64 + 2 * fmt2.size(); size < 65536; size *= 2) {
    std::wstring buffer(size, L'\0');
    size_t n = wcsftime(&buffer[0], size, fmt2.c_str(), &tm);
    if (n > 0) {
      buffer.resize(n);
      return String(buffer);
    }
  }
  return String(); // the result really is empty (e.g. "%p" in some locales)
}

// ----------------------------------------------------------------------------- : Parsing

// The old reader used wxDateTime::ParseDateTime, a heuristic parser for "free format" dates.
// MSE itself only ever wrote "%Y-%m-%d %H:%M:%S" (local time), but files edited by hand or other
// tools could contain anything wx accepted. This parser accepts the forms wx accepted in practice:
//
//   <date> [ (whitespace | 'T') <time> ]   or   <time> <whitespace> <date>
//
//   <date> := Y-M-D | Y/M/D | Y.M.D          (year first, 3+ digits or > 31)
//           | M/D/Y | D/M/Y                  (year last; day first only if the first number is > 12)
//           | D Month Y | Month D[,] Y       (English month names or 3 letter abbreviations)
//           | today | yesterday | tomorrow
//   <time> := H[:M[:S[.fraction]]] [am|pm] [zone] | noon | midnight
//   <zone> := +HHMM | -HHMM | GMT | UT | UTC | Z
//
// optionally preceded by a weekday name (as in RFC 822 dates: "Sat, 07 Jan 2023 12:34:56 +0000"),
// in local time unless a zone is given, case insensitive, with optional surrounding whitespace.
// Unlike wx, a date without a time is also accepted (it means midnight).

namespace {
  struct DateParser {
    std::wstring s;
    size_t pos = 0;

    bool atEnd() const { return pos >= s.size(); }
    wchar_t peek(size_t ahead = 0) const { return pos + ahead < s.size() ? s[pos + ahead] : L'\0'; }

    size_t skipWhitespace() {
      size_t start = pos;
      while (!atEnd() && iswspace(s[pos])) ++pos;
      return pos - start;
    }
    // read an unsigned decimal number, returns the number of digits (0 if there is none)
    int number(int& value) {
      value = 0;
      int digits = 0;
      while (!atEnd() && iswdigit(s[pos])) {
        if (digits < 9) value = value * 10 + (s[pos] - L'0');
        ++digits; ++pos;
      }
      return digits;
    }
    std::wstring word() {
      std::wstring w;
      while (!atEnd() && iswalpha(s[pos])) w += (wchar_t)towlower(s[pos++]);
      return w;
    }

    static int monthFromName(const std::wstring& w) {
      static const wchar_t* names[] = {L"january",L"february",L"march",L"april",L"may",L"june",L"july",L"august",L"september",L"october",L"november",L"december"};
      if (w.size() < 3) return 0;
      for (int i = 0; i < 12; ++i) {
        std::wstring name = names[i];
        if (w == name || w == name.substr(0,3) || (i == 8 && w == L"sept")) return i + 1;
      }
      return 0;
    }
    static bool isLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
    static int daysInMonth(int y, int m) {
      static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
      return m == 2 && isLeap(y) ? 29 : days[m-1];
    }
    static bool validDate(int y, int m, int d) {
      return m >= 1 && m <= 12 && d >= 1 && d <= daysInMonth(y, m);
    }
    static bool isDateSep(wchar_t c) { return c == L'-' || c == L'/' || c == L'.'; }

    // parse a date, on failure pos is unspecified
    bool date(int& y, int& m, int& d) {
      if (iswalpha(peek())) {
        std::wstring w = word();
        int day_offset = w == L"today" ? 0 : w == L"yesterday" ? -1 : w == L"tomorrow" ? 1 : 99;
        if (day_offset != 99) {
          struct tm tm = DateTime::now().addDays(day_offset).toLocalTm();
          y = tm.tm_year + 1900; m = tm.tm_mon + 1; d = tm.tm_mday;
          return true;
        }
        // Month D[,] Y
        m = monthFromName(w);
        if (!m) return false;
        if (peek() == L'.') ++pos;
        if (!skipWhitespace()) return false;
        if (!number(d)) return false;
        if (peek() == L',') ++pos;
        if (!skipWhitespace()) return false;
        if (!number(y)) return false;
        return validDate(y, m, d);
      }
      int n1, n2, n3;
      int digits1 = number(n1);
      if (!digits1) return false;
      if (isDateSep(peek())) {
        // numeric date
        ++pos;
        if (!number(n2)) return false;
        if (!isDateSep(peek())) return false;
        ++pos;
        int digits3 = number(n3);
        if (!digits3) return false;
        if (digits1 >= 3 || n1 > 31 || !(digits3 >= 3 || n3 > 31)) {
          y = n1; m = n2; d = n3;
          if (m > 12 && d <= 12) std::swap(m, d); // wx's heuristic did this too
        } else {
          y = n3;
          if (n1 > 12) { d = n1; m = n2; }
          else         { m = n1; d = n2; }
        }
        return validDate(y, m, d);
      } else {
        // D Month Y
        d = n1;
        if (!skipWhitespace()) return false;
        m = monthFromName(word());
        if (!m) return false;
        if (peek() == L'.') ++pos;
        if (peek() == L',') ++pos;
        if (!skipWhitespace()) return false;
        if (!number(y)) return false;
        return validDate(y, m, d);
      }
    }

    // parse a time, on failure pos is unspecified
    bool time(int& h, int& mi, int& sec, int& ms) {
      h = mi = sec = ms = 0;
      if (iswalpha(peek())) {
        std::wstring w = word();
        if (w == L"noon")     { h = 12; return true; }
        if (w == L"midnight") { h = 0;  return true; }
        return false;
      }
      if (!number(h)) return false;
      if (peek() == L':' && iswdigit(peek(1))) {
        ++pos; number(mi);
        if (peek() == L':' && iswdigit(peek(1))) {
          ++pos; number(sec);
          if ((peek() == L'.' || peek() == L',') && iswdigit(peek(1))) {
            ++pos;
            int digits = 0;
            while (!atEnd() && iswdigit(s[pos])) {
              if (digits < 3) ms = ms * 10 + (s[pos] - L'0');
              ++digits; ++pos;
            }
            for (; digits < 3; ++digits) ms *= 10;
          }
        }
      }
      // optional am/pm
      size_t before = pos;
      skipWhitespace();
      std::wstring w = word();
      if (w == L"am" || w == L"pm") {
        if (h < 1 || h > 12) return false;
        h = h % 12 + (w == L"pm" ? 12 : 0);
      } else {
        pos = before;
      }
      return h <= 23 && mi <= 59 && sec <= 60;
    }

    // optional weekday name, as in RFC 822 dates ("Sat, 07 Jan 2023 ...")
    void weekday() {
      static const wchar_t* names[] = {L"monday",L"tuesday",L"wednesday",L"thursday",L"friday",L"saturday",L"sunday"};
      size_t before = pos;
      std::wstring w = word();
      for (const wchar_t* name : names) {
        std::wstring n = name;
        if (w.size() >= 3 && (w == n || w == n.substr(0,3))) {
          if (peek() == L',') ++pos;
          if (skipWhitespace() || (pos > 0 && s[pos-1] == L',')) return;
        }
      }
      pos = before;
    }

    // optional time zone after the time: +HHMM, -HHMM, GMT, UT, UTC or Z. Returns offset east of UTC in minutes.
    bool zone(int& offset) {
      size_t before = pos;
      skipWhitespace();
      wchar_t c = peek();
      if ((c == L'+' || c == L'-') && iswdigit(peek(1)) && iswdigit(peek(2)) && iswdigit(peek(3)) && iswdigit(peek(4)) && !iswdigit(peek(5))) {
        int hhmm = (peek(1)-L'0')*1000 + (peek(2)-L'0')*100 + (peek(3)-L'0')*10 + (peek(4)-L'0');
        pos += 5;
        offset = (hhmm / 100 * 60 + hhmm % 100) * (c == L'-' ? -1 : 1);
        return true;
      }
      std::wstring w = word();
      if (w == L"gmt" || w == L"ut" || w == L"utc" || w == L"z") {
        offset = 0;
        return true;
      }
      pos = before;
      return false;
    }

    // days since 1970-01-01 of a proleptic Gregorian date (Howard Hinnant's days_from_civil)
    static long long daysFromCivil(long long y, int m, int d) {
      y -= m <= 2;
      long long era = (y >= 0 ? y : y - 399) / 400;
      long long yoe = y - era * 400;
      long long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
      long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
      return era * 146097 + doe - 719468;
    }

    DateTime parse() {
      int y, m, d, h = 0, mi = 0, sec = 0, ms = 0, offset = 0;
      bool has_zone = false;
      skipWhitespace();
      weekday();
      size_t start = pos;
      if (date(y, m, d)) {
        size_t after_date = pos;
        skipWhitespace();
        if (!atEnd()) { // otherwise: only a date, midnight
          pos = after_date;
          bool sep = skipWhitespace() > 0;
          if (!sep && peek() == L'T' && iswdigit(peek(1))) { ++pos; sep = true; }
          if (!sep || !time(h, mi, sec, ms)) return DateTime();
          has_zone = zone(offset);
        }
      } else {
        pos = start;
        if (!time(h, mi, sec, ms)) return DateTime();
        if (!skipWhitespace()) return DateTime();
        if (!date(y, m, d)) return DateTime();
      }
      skipWhitespace();
      if (!atEnd()) return DateTime();
      if (has_zone) {
        long long secs = daysFromCivil(y, m, d) * 86400 + h * 3600 + mi * 60 + sec - offset * 60;
        return DateTime(DateTime::TimePoint(std::chrono::seconds(secs)) + std::chrono::milliseconds(ms));
      }
      return DateTime::fromLocal(y, m, d, h, mi, sec, ms);
    }
  };
}

DateTime DateTime::parse(const String& str) {
  DateParser parser;
  parser.s = str;
  return parser.parse();
}
