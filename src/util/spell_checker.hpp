//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#undef near
#include "hunspell.hxx"

DECLARE_POINTER_TYPE(SpellChecker);

// ----------------------------------------------------------------------------- : Spell checker

/// A spelling checker for a particular language
class SpellChecker : public Hunspell, public IntrusivePtrBase<SpellChecker> {
public:
  SpellChecker(const char* aff_path, const char* dic_path);
  /// Get a SpellChecker object for the given language.
  /** Returns nullptr on error
   *  Note: This is not threadsafe yet */
  static SpellChecker* get(const String& language);
  /// Get a SpellChecker object for the given language and filename
  /** Returns nullptr on error
   *  Note: This is not threadsafe yet */
  static SpellChecker* get(const String& filename, const String& language);
  /// Destroy all cached SpellChecker objects
  static void destroyAll();

  /// Check the spelling of a single word
  bool spell(const String& word);

  /// Give spelling suggestions
  void suggest(const String& word, vector<String>& suggestions_out);

private:
  /// The dictionary encoding (hunspell's SET, ISO8859-1 by default)
  enum class Encoding { UTF8, LATIN1, OTHER };
  Encoding encoding;
  String encoding_name; ///< Only used for Encoding::OTHER
  /// Convert from String to the dictionary encoding; false if the word can't be encoded
  bool convert_encoding(const String& word, std::string& out);
  /// Convert from the dictionary encoding to String; "" if that fails
  String convert_from_encoding(const std::string& word);

  static map<String,SpellCheckerP> spellers; //< Cached checkers for each language
};

