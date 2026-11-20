//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/format/formats.hpp>
#include <data/game.hpp>
#include <data/set.hpp>
#include <data/card.hpp>
#include <data/field/text.hpp>
#include <data/field/choice.hpp>
#include <util/string.hpp>
#include <util/tagged_string.hpp>
#include <util/io/stream.hpp>

// ----------------------------------------------------------------------------- : Utilities

/// Convert a tagged string to MWS format: \t\t before each line beyond the first
String untag_mws(const String& str) {
  // TODO : em dashes?
  return replace_all(untag(curly_quotes(str,false)), _("\n"), _("\n\t\t") );
}
//String untag_mws(const Defaultable<String>& str) {
//  str.
//}

/// Code for card color in MWS format
String card_color_mws(const String& col) {
  if (col == _("white"))     return _("W");
  if (col == _("blue"))      return _("U");
  if (col == _("black"))     return _("B");
  if (col == _("red"))       return _("R");
  if (col == _("green"))     return _("G");
  if (col == _("artifact"))  return _("Art");
  if (col == _("colorless")) return _("Art");
  if (col.find(_("land")) != String::npos) {
    return _("Lnd"); // land
  } else {
    return _("Gld"); // multicolor
  }
}

/// Code for card rarity, used for MWS and Apprentice
String card_rarity_code(const String& rarity) {
  if (rarity == _("rare"))     return _("R");
  if (rarity == _("uncommon")) return _("U");
  else                         return _("C");
}

// ----------------------------------------------------------------------------- : export_mws

void export_mws(const SetP& set, const String& name) {
  if (!set->game->isMagic()) {
    throw Error(_("Can only export magic sets to Magic Workstation"));
  }
  // Open file
  FileOutputStream f(name);
  TextOutputStream file(f, LineEnding::DOS);
  
  // Write header
  file.writeString(set->value<TextValue>(_("title")).value() + _(" Spoiler List\n"));
  file.writeString(_("Set exported using Magic Set Editor 2, version ") + app_version.toString() + _("\n\n"));
  file.writeString(_("Spoiler List created on ") + DateTime::now().format(_("%Y-%m-%d %H:%M:%S")));
  file.writeString(_("\n\n"));
  
  // Write cards
  FOR_EACH(card, set->cards) {
    file.writeString(_("Card Name:\t"));
    file.writeString(untag_mws(card->value<TextValue>(_("name")).value));
    file.writeString(_("\nCard Color:\t"));
    file.writeString(card_color_mws(card->value<ChoiceValue>(_("card color")).value));
    file.writeString(_("\nMana Cost:\t"));
    file.writeString(untag_mws(card->value<TextValue>(_("casting cost")).value));
    file.writeString(_("\nType & Class:\t"));
    String sup_type = untag_mws(card->value<TextValue>(_("super type")).value);
    String sub_type = untag_mws(card->value<TextValue>(_("sub type")).value);
    if (sub_type.empty()) {
      file.writeString(sup_type);
    } else {
      file.writeString(sup_type + _(" - ") + sub_type);
    }
    file.writeString(_("\nPow/Tou:\t"));
    file.writeString(untag_mws(card->value<TextValue>(_("pt")).value));
    file.writeString(_("\nCard Text:\t"));
    file.writeString(untag_mws(card->value<TextValue>(_("rule text")).value));
    file.writeString(_("\nFlavor Text:\t"));
    file.writeString(untag_mws(card->value<TextValue>(_("flavor text")).value));
    file.writeString(_("\nArtist:\t\t"));
    file.writeString(untag_mws(card->value<TextValue>(_("illustrator")).value));
    file.writeString(_("\nRarity:\t\t"));
    file.writeString(card_rarity_code(card->value<ChoiceValue>(_("rarity")).value));
    file.writeString(_("\nCard #:\t\t"));
    file.writeString(untag_mws(card->value<TextValue>(_("card number")).value));
    file.writeString(_("\n\n"));
  }
}
