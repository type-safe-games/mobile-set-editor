// Mobile Set Editor - single-byte character sets
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

/** @file util/charset.hpp
 *
 *  @brief The 8-bit character sets Hunspell dictionaries may use (besides UTF-8 and ISO8859-1):
 *  ISO8859-2 to -10 and -13 to -15, KOI8-R, KOI8-U, CP1251 and TIS620. They replace wxCSConv in
 *  the wx-free core (#43). The tables are generated (util/charset.cpp).
 */

#include <util/prec.hpp>

/// The characters of the bytes 0x80 to 0xFF of a character set (bytes below 0x80 are ASCII)
struct SingleByteCharset {
  wchar_t upper[128]; ///< 0 where a byte has no character
};

/// A character set by name (case-insensitive, ignoring '-' and '_'); null if it isn't known
const SingleByteCharset* find_single_byte_charset(const String& name);
/// Encode text; false (and an empty out) if a character has no byte in the set
bool encode_single_byte(const SingleByteCharset& charset, const String& text, std::string& out);
/// Decode bytes; bytes without a character become U+FFFD
String decode_single_byte(const SingleByteCharset& charset, const std::string& bytes);
