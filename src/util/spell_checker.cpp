//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/spell_checker.hpp>
#include <util/string.hpp>
#include <util/io/package_manager.hpp>

// ----------------------------------------------------------------------------- : Spell checker : construction

map<String,SpellCheckerP> SpellChecker::spellers;

SpellChecker* SpellChecker::get(const String& language) {
  SpellCheckerP& speller = spellers[language];
  if (!speller) {
    String local_dir  = package_manager.getDictionaryDir(true);
    String global_dir = package_manager.getDictionaryDir(false);
    String aff_path = language + _(".aff");
    String dic_path = language + _(".dic");
    if (file_exists(local_dir + aff_path) && file_exists(local_dir + dic_path)) {
      speller = make_intrusive<SpellChecker>(to_fs_path(local_dir + aff_path).string().c_str(), to_fs_path(local_dir + dic_path).string().c_str());
    } else if (file_exists(global_dir + aff_path) && file_exists(global_dir + dic_path)) {
      speller = make_intrusive<SpellChecker>(to_fs_path(global_dir + aff_path).string().c_str(), to_fs_path(global_dir + dic_path).string().c_str());
    } else {
      queue_message(MESSAGE_ERROR, _("Dictionary not found for language: ") + language);
    }
  }
  return speller.get();
}

SpellChecker* SpellChecker::get(const String& filename, const String& language) {
  SpellCheckerP& speller = spellers[filename + _(".") + language];
  if (!speller) {
    String prefix = package_manager.openFilenameFromPackage(nullptr, filename) + _(".");
    String local_dir  = package_manager.getDictionaryDir(true);
    String global_dir = package_manager.getDictionaryDir(false);
    String aff_path = language + _(".aff");
    String dic_path = language + _(".dic");
    if (file_exists(prefix + dic_path)) {
      if (file_exists(prefix + aff_path)) {
        speller = make_intrusive<SpellChecker>(to_fs_path(prefix + aff_path).string().c_str(), to_fs_path(prefix + dic_path).string().c_str());
      } else if (file_exists(local_dir + aff_path)) {
        speller = make_intrusive<SpellChecker>(to_fs_path(local_dir + aff_path).string().c_str(), to_fs_path(prefix + dic_path).string().c_str());
      } else if (file_exists(global_dir + aff_path)) {
        speller = make_intrusive<SpellChecker>(to_fs_path(global_dir + aff_path).string().c_str(), to_fs_path(prefix + dic_path).string().c_str());
      }
    }
    if (!speller) {
      queue_message(MESSAGE_ERROR, _("Dictionary '") + filename + _("' not found for language: ") + language);
    }
  }
  return speller.get();
}

SpellChecker::SpellChecker(const char* aff_path, const char* dic_path)
  : Hunspell(aff_path,dic_path)
{
  // Dictionaries are UTF-8 or (hunspell's default without a SET line) ISO8859-1; those are converted without wx.
  String name = decode_utf8_or_latin1(get_dic_encoding(), strlen(get_dic_encoding()));
  String key;
  for (Char c : name) {
    if (c != _('-') && c != _('_')) key += toUpper(c);
  }
  if (key == _("UTF8")) {
    encoding = Encoding::UTF8;
  } else if (key == _("ISO88591") || key == _("LATIN1")) {
    encoding = Encoding::LATIN1;
  } else {
    encoding = Encoding::OTHER;
    encoding_name = name;
  }
}

void SpellChecker::destroyAll() {
  spellers.clear();
}

// ----------------------------------------------------------------------------- : Spell checker : use

bool SpellChecker::convert_encoding(const String& word, std::string& out) {
  // fix curly quotes, especially apstrophes
  String fixed;
  FOR_EACH_CONST(c,word) {
    if (c == LEFT_SINGLE_QUOTE || c == RIGHT_SINGLE_QUOTE) {
      fixed += _('\'');
    } else if (c == LEFT_DOUBLE_QUOTE || c == RIGHT_DOUBLE_QUOTE) {
      fixed += _('\"');
    } else if (c == 0x00C6) {
      // expand ligatures, TODO: put this in a better place
      fixed += _("Ae");
    } else if (c == 0x0132) {
      fixed += _("IJ");
    } else if (c == 0x0152) {
      fixed += _("Oe");
    } else if (c == 0xFB01) {
      fixed += _("fi");
    } else if (c == 0xFB02) {
      fixed += _("fl");
    } else {
      fixed += c;
    }
  }
  // convert encoding
  out.clear();
  if (encoding == Encoding::UTF8) {
    out = encode_utf8(fixed);
  } else if (encoding == Encoding::LATIN1) {
    for (Char c : fixed) {
      if ((unsigned)c > 0xFF) { out.clear(); break; }
      out += (char)(unsigned char)c;
    }
  } else {
    // wx boundary (#30): other 8-bit dictionary encodings (none in the template pack) have no wx-free converter yet
    wxCharBuffer buf = wxString(fixed).mb_str(wxCSConv(wxString(encoding_name)));
    if (buf.data()) out = buf.data();
  }
  if (out.empty() || out[0] == '\0') {
    // If encoding fails we get an empty string, since the word was not empty this can never happen
    // words that can't be encoded are not in the dictionary, so they are wrong.
    return false;
  } else {
    return true;
  }
}

String SpellChecker::convert_from_encoding(const std::string& word) {
  if (encoding == Encoding::UTF8) {
    return decode_utf8(word);
  } else if (encoding == Encoding::LATIN1) {
    String ret;
    for (char c : word) ret += (Char)(unsigned char)c;
    return ret;
  } else {
    // wx boundary (#30): no wx-free converter for arbitrary 8-bit encodings yet
    return wxString(word.c_str(), wxCSConv(wxString(encoding_name))).ToStdWstring();
  }
}

bool SpellChecker::spell(const String& word) {
  if (word.empty()) return true; // empty word is okay
  std::string str;
  if (!convert_encoding(word,str)) return false;
  return Hunspell::spell(str);
}

void SpellChecker::suggest(const String& word, vector<String>& suggestions_out) {
  std::string str;
  if (!convert_encoding(word,str)) return;
  // call Hunspell, copy sugestions
  for (const std::string& suggestion : Hunspell::suggest(str)) {
    suggestions_out.push_back(convert_from_encoding(suggestion));
  }
}
