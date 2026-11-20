//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/format/formats.hpp>
#include <data/set.hpp>
#include <data/game.hpp>
#include <data/stylesheet.hpp>
#include <data/card.hpp>
#include <data/field/text.hpp>
#include <data/field/choice.hpp>
#include <data/field/image.hpp>
#include <util/io/stream.hpp>

// ----------------------------------------------------------------------------- : MSE1FileFormat

/// The file format of MSE1 files
class MSE1FileFormat : public FileFormat {
public:
  String extension() override          { return _("mse"); }
  String name() override               { return _("Magic Set Editor version 1 files (*.mse)"); }
  bool canImport() override            { return true; }
  bool canExport(const Game&) override { return false; }
  SetP importSet(const String& filename) override;
};

FileFormatP mse1_file_format() {
  return make_unique<MSE1FileFormat>();
}

// ----------------------------------------------------------------------------- : Importing

// read a card from a mse1 file, add to the set when done
void read_mse1_card(Set& set, InputStream& f);

SetP MSE1FileFormat::importSet(const String& filename) {
  FileInputStream f(filename);
  // create set
  SetP set(new Set(Game::byName(_("magic"))));
  
  // file version check
  String format = read_locale_line(f);
  if (format.substr(0,8) != _("MTG Set8")) {
    throw ParseError(_("Expected MSE format version 8\nTo convert files made with older versions of Magic Set Editor:\n  1. Download the latest version 1 from https://magicseteditor.boards.net/page/downloads\n  2. Open the set, then save the set\n  3. Try to open them again in this program."));
  }
  // read general info
  set->value<TextValue>(_("title"))    .value = read_locale_line(f);
  set->value<TextValue>(_("artist"))   .value = read_locale_line(f);
  set->value<TextValue>(_("copyright")).value = read_locale_line(f);
  read_locale_line(f); // border color, ignored
  String stylesheet = read_locale_line(f);
  set->apprentice_code = read_locale_line(f); // apprentice prefix
  read_locale_line(f); // 'formatN'?, not even used by MSE1 :S, ignored
  read_locale_line(f); // 'formatS'?, same, ignored
  read_locale_line(f); // symbol filename, ignored
  read_locale_line(f); // use black symbol for all rarities, ignored
  String desc, line;
  while (!f.eof()) {
    line = read_locale_line(f);
    if (line == _("\xFF")) break;
    desc += line;
  }
  set->value<TextValue>(_("description")).value = desc;
  
  // load stylesheet
  if (stylesheet.substr(0,3) == _("old")) {
    try {
      set->stylesheet = StyleSheet::byGameAndName(*set->game, _("old"));
    } catch (const Error&) {
      // If old style doesn't work try the new one
      set->stylesheet = StyleSheet::byGameAndName(*set->game, _("new"));
    }
  } else {
    set->stylesheet = StyleSheet::byGameAndName(*set->game, _("new"));
  }
  
  // read cards
  CardP current_card;
  while (!f.eof()) {
    read_mse1_card(*set, f);
  }
  
  // done
  set->validate();
  return set;
}

void read_mse1_card(Set& set, InputStream& f) {
  CardP card(new Card(*set.game));
  while (!f.eof()) {
    // read a line
    String line = read_locale_line(f);
    if (line.empty()) continue;
    Char type = line[0];
    line = line.substr(1);
    // interpret this line
    switch (type) {
      case 'A': {    // done
        set.cards.push_back(card);
        return;
      } case 'B': {  // name
        card->value<TextValue>(_("name"))        .value.assign(line);
        break;
      } case 'C': case 'D': { // image filename
        LocalFileName image_file = set.newFileName(_("image"),_("")); // a new unique name in the package
        if (file_exists(line) && copy_file(line, set.nameOut(image_file))) {
          card->value<ImageValue>(_("image")).filename = image_file;
        }
        break;
      } case 'E':  {  // super type
        card->value<TextValue>(_("super type")).value.assign(line);
        break;
      } case 'F': {  // sub type
        card->value<TextValue>(_("sub type")).value.assign(line);
        break;
      } case 'G': {  // casting cost
        card->value<TextValue>(_("casting cost")).value.assign(line);
        break;
      } case 'H': {  // rarity
        String rarity;
        if      (line == _("(U)")) rarity = _("uncommon");
        else if (line == _("(R)")) rarity = _("rare");
        else                       rarity = _("common");
        card->value<ChoiceValue>(_("rarity"))    .value.assign(rarity);
        break;
      } case 'I': {  // power/thoughness
        size_t pos = line.find_first_of(_('/'));
        if (pos != String::npos) {
          card->value<TextValue>(_("power"))       .value.assign(line.substr(0, pos));
          card->value<TextValue>(_("toughness"))   .value.assign(line.substr(pos+1));
        }
        break;
      } case 'J': {  // rule text or part of text
        Defaultable<String>& text = card->value<TextValue>(_("rule text")).value;
        if (!text().empty()) text.mutate() += _('\n');
        text.mutate() += line;
        break;
      } case 'K':  {  // flavor text or part of text
        Defaultable<String>& text = card->value<TextValue>(_("flavor text")).value;
        if (!text().empty()) text.mutate() += _('\n');
        text.mutate() += line;
        break;
      } case 'L': {  // card color (if not default)
        // decode color
        String color;
        if      (line == _("1")) color = _("white");
        else if (line == _("2")) color = _("blue");
        else if (line == _("3")) color = _("black");
        else if (line == _("4")) color = _("red");
        else if (line == _("5")) color = _("green");
        else if (line == _("6")) color = _("colorless");
        else if (line == _("7")) color = _("land");
        else if (line == _("9")) color = _("multicolor");
        else                     color = _("colorless");
        card->value<ChoiceValue>(_("card color")).value.assign(color);
        break;
      } default: {
        throw ParseError(_("Not a valid MSE1 file"));
      }
    }
  }
}
