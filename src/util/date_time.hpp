//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : Includes

#include <chrono>
#include <ctime>
#include <cstdint>

// ----------------------------------------------------------------------------- : DateTime

/// A point in time, with millisecond precision, or an 'invalid' value.
/** Replaces wxDateTime in the core. Calendar fields (formatting, parsing, start of day)
 *  are always in the local time zone, like wxDateTime's defaults.
 *
 *  The default constructed value is invalid (as with wxDateTime). Comparisons work on
 *  invalid values too, an invalid value is before every valid one.
 */
class DateTime {
public:
  using Clock     = std::chrono::system_clock;
  using TimePoint = std::chrono::time_point<Clock, std::chrono::milliseconds>;

  /// An invalid date
  DateTime() : time(TimePoint::min()) {}
  explicit DateTime(TimePoint time) : time(time) {}

  /// The current time
  static DateTime now();
  /// From seconds since the epoch
  static DateTime fromTimeT(time_t t);
  /// From local calendar fields (month is 1-12). Out of range fields are normalized as by mktime.
  /** Returns an invalid date if the time can't be represented. */
  static DateTime fromLocal(int year, int month, int day, int hour = 0, int minute = 0, int second = 0, int millisecond = 0);

  /// Parse a date in one of the formats accepted by the old wx based reader (see parse_date_time).
  /** The whole string must be a date (surrounding whitespace is allowed); returns an invalid date otherwise. */
  static DateTime parse(const String& str);

  inline bool isValid() const { return time != TimePoint::min(); }
  inline TimePoint timePoint() const { return time; }
  /// Seconds since the epoch (rounded down)
  time_t toTimeT() const;
  /// Milliseconds within the second (0-999)
  int millisecond() const;
  /// Local calendar fields of this time
  struct tm toLocalTm() const;

  /// Midnight (local time) at the start of the same day
  DateTime startOfDay() const;
  /// The same local time of day, a number of calendar days later (or earlier if negative)
  DateTime addDays(int days) const;

  /// Format in local time with a strftime format string. Also supports "%l" for milliseconds (as wx did).
  /** Returns "" for an invalid date. */
  String format(const String& fmt) const;

  inline bool operator == (const DateTime& that) const { return time == that.time; }
  inline bool operator != (const DateTime& that) const { return time != that.time; }
  inline bool operator <  (const DateTime& that) const { return time <  that.time; }
  inline bool operator >  (const DateTime& that) const { return time >  that.time; }
  inline bool operator <= (const DateTime& that) const { return time <= that.time; }
  inline bool operator >= (const DateTime& that) const { return time >= that.time; }

private:
  TimePoint time;
};

/// The format in which dates are persisted (in .mse-set files, etc.) and shown as script strings.
#define DATE_TIME_PERSIST_FORMAT _("%Y-%m-%d %H:%M:%S")
