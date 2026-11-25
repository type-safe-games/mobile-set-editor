// Mobile Set Editor - headless tests of the facade on the desktop host
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Usage: facade_tests <fixture dir> <work dir> [<fixture goldens dir>]
//        facade_tests --import <zip> <template store> <font store>
//
// Runs the facade (ADR 0009) against the Test fixture (ADR 0008) with the wx backends of the
// desktop host. GTK needs a display: run under xvfb-run, as ctest does. Renders are also compared
// with the fixture's Golden images (test/fixture/goldens/fixture-cards) when given.
//
// The tests talk to the facade only; core headers are used just to decode PNGs and build a zip.
//
// With --import, it only imports a zip into a template store and font store through the facade, as
// the app would (e.g. to check the goldens against an imported Template pack).

#include <util/prec.hpp>
#include <facade/facade.hpp>
#include "test_host.hpp"
#include <facade/template_import.hpp>
#include <data/text_editing.hpp>
#include <gfx/image_codec.hpp>
#include <util/io/zip.hpp>
#include <util/thread.hpp>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <thread>

namespace F = mse::facade;
namespace fs = std::filesystem;

// ----------------------------------------------------------------------------- : Test runner

static int failures = 0;
static int checks = 0;
static std::string current_test;

static void report_failure(const char* file, int line, const std::string& what) {
  ++failures;
  std::fprintf(stderr, "FAIL [%s] %s:%d: %s\n", current_test.c_str(), file, line, what.c_str());
}

#define CHECK(cond) do { ++checks; if (!(cond)) report_failure(__FILE__, __LINE__, #cond); } while (0)
#define CHECK_EQ(a, b) do { ++checks; auto&& a_ = (a); auto&& b_ = (b); \
    if (!(a_ == b_)) report_failure(__FILE__, __LINE__, std::string(#a " == " #b) + " (" + to_s(a_) + " vs " + to_s(b_) + ")"); } while (0)
#define CHECK_OK(r) do { ++checks; const auto& r_ = (r); \
    if (!r_.ok()) report_failure(__FILE__, __LINE__, std::string(#r) + " failed: status " + std::to_string((int)r_.status) + ": " + r_.error); } while (0)
#define REQUIRE(cond) do { ++checks; if (!(cond)) { report_failure(__FILE__, __LINE__, #cond); return; } } while (0)
#define REQUIRE_OK(r) do { const auto& r_ = (r); ++checks; \
    if (!r_.ok()) { report_failure(__FILE__, __LINE__, std::string(#r) + " failed: status " + std::to_string((int)r_.status) + ": " + r_.error); return; } } while (0)

static std::string to_s(const std::string& s) { return "\"" + s + "\""; }
static std::string to_s(const char* s) { return to_s(std::string(s)); }
template <typename T> static std::string to_s(const T& x) {
  if constexpr (std::is_enum_v<T>) return std::to_string((int)x);
  else return std::to_string(x);
}

struct TestCase { const char* name; std::function<void()> run; };
static std::vector<TestCase>& tests() { static std::vector<TestCase> t; return t; }
struct Register { Register(const char* name, std::function<void()> f) { tests().push_back({name, f}); } };
#define TEST(name) static void test_##name(); static Register reg_##name(#name, test_##name); static void test_##name()

// ----------------------------------------------------------------------------- : Environment

static fs::path fixture_dir, work_dir, goldens_dir;
static fs::path templates_dir, font_store_dir;
static int session_counter = 0;

static fs::path fixture_set() { return fixture_dir / "sets" / "fixture-cards.mse-set"; }

/// A new session on the fixture set, with its own working copy
static F::SessionHandle open_fixture() {
  fs::path work = work_dir / "sessions" / ("s" + std::to_string(++session_counter) + ".mse-set");
  F::OpenResult r = F::openSession(fixture_set().string(), work.string());
  if (!r.ok()) {
    report_failure(__FILE__, __LINE__, "openSession: " + r.error);
    return 0;
  }
  return r.session;
}

static std::string read_text(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

static bool decode_png(const std::vector<std::uint8_t>& data, Image& out) {
  return image_codec().decode(data.data(), data.size(), out);
}

static bool has_message(const F::Result& r, const std::string& part) {
  for (const F::Message& m : r.messages) if (m.text.find(part) != std::string::npos) return true;
  return false;
}

static const F::CardInfo* find_card(const F::CardList& list, const std::string& uid) {
  for (const F::CardInfo& c : list.cards) if (c.uid == uid) return &c;
  return nullptr;
}

static bool contains(const std::vector<std::string>& v, const std::string& x) {
  return std::find(v.begin(), v.end(), x) != v.end();
}

// ----------------------------------------------------------------------------- : Sessions and cards

TEST(card_list_table) {
  // #86: the Game's card list columns (the fixture shows only the name) and each card's values
  F::SessionHandle s = open_fixture();
  F::CardListTable t = F::cardListTable(s);
  REQUIRE_OK(t);
  CHECK_EQ(t.columns.size(), (size_t)1);
  if (!t.columns.empty()) CHECK_EQ(t.columns[0].field, "name");
  CHECK_EQ(t.rows.size(), (size_t)3);
  if (t.rows.size() == 3 && !t.rows[0].empty()) {
    CHECK_EQ(t.rows[0][0], "Long Wrapping Text");
    CHECK_EQ(t.rows[1][0], "Front Face");
  }
  F::closeSession(s);
}

TEST(open_and_list_cards) {
  F::SessionHandle s = open_fixture();
  F::CardList cards = F::listCards(s);
  REQUIRE_OK(cards);
  CHECK_EQ(cards.cards.size(), (size_t)3);
  CHECK_EQ(cards.cards[0].uid, "fixture-wrap");
  CHECK_EQ(cards.cards[0].name, "Long Wrapping Text");
  CHECK_EQ(cards.cards[1].uid, "fixture-front");
  CHECK_EQ(cards.cards[2].uid, "fixture-back");
  CHECK_EQ(cards.cards[1].links.size(), (size_t)1);
  if (!cards.cards[1].links.empty()) {
    CHECK_EQ(cards.cards[1].links[0].uid, "fixture-back");
    CHECK_EQ(cards.cards[1].links[0].relation, "Back Face");
  }
  F::SessionInfo info = F::sessionInfo(s);
  CHECK_OK(info);
  CHECK_EQ(info.game, "fixture.mse-game");
  CHECK_EQ(info.stylesheet, "fixture-standard.mse-style");
  CHECK(!info.dirty);
  CHECK(!info.can_undo);
  // the working copy was written as a directory
  CHECK(fs::exists(work_dir / "sessions" / ("s" + std::to_string(session_counter) + ".mse-set") / "set"));
  F::FieldList fields = F::listFields(s, F::Target::card("fixture-wrap"));
  CHECK_OK(fields);
  bool has_rules = false;
  for (const F::FieldInfo& f : fields.fields) {
    if (f.name == "rules") { has_rules = true; CHECK_EQ(f.type, "text"); CHECK(f.multi_line); }
    if (f.name == "frame") { CHECK_EQ(f.type, "choice"); CHECK(contains(f.choices, "blue")); }
  }
  CHECK(has_rules);
  CHECK_OK(F::closeSession(s));
  CHECK_EQ(F::listCards(s).status, F::Status::NotFound);
}

// ----------------------------------------------------------------------------- : Rendering

TEST(render_matches_export_layout_and_goldens) {
  F::SessionHandle s = open_fixture();
  F::CardList cards = F::listCards(s);
  REQUIRE_OK(cards);
  bool have_goldens = !goldens_dir.empty() && fs::exists(goldens_dir / "0.json");
  if (!have_goldens) std::printf("  (no fixture goldens at '%s': comparing with the core's export only)\n", goldens_dir.string().c_str());
  for (size_t i = 0 ; i < cards.cards.size() ; ++i) {
    const std::string& uid = cards.cards[i].uid;
    F::CardSize size = F::cardSize(s, uid);
    REQUIRE_OK(size);
    CHECK_EQ(size.width, 300.0);
    CHECK_EQ(size.height, 420.0);
    int w = (int)size.width, h = (int)size.height, stride = w * 4 + 8; // padded rows
    std::vector<std::uint8_t> buf((size_t)stride * h, 0x55);
    F::RenderResult r = F::renderCard(s, uid, buf.data(), w, h, stride);
    REQUIRE_OK(r);
    CHECK(r.messages.empty());
    // regions: from the same pass as the layout dump
    F::TextOutput dump = F::layoutDump(s, uid);
    REQUIRE_OK(dump);
    size_t visible_in_dump = 0;
    for (size_t p = dump.text.find("\"visible\": true") ; p != std::string::npos ; p = dump.text.find("\"visible\": true", p + 1)) ++visible_in_dump;
    CHECK_EQ(r.regions.size(), visible_in_dump);
    bool has_rules = false;
    int last_z = -1000000;
    for (const F::FieldRegion& fr : r.regions) {
      CHECK(dump.text.find("\"name\": \"" + fr.field + "\"") != std::string::npos);
      CHECK(fr.z >= last_z); // drawing order
      last_z = fr.z;
      CHECK(fr.x >= 0 && fr.y >= 0 && fr.x + fr.width <= w + 0.5 && fr.y + fr.height <= h + 0.5);
      if (fr.field == "rules") { has_rules = true; CHECK(fr.editable); CHECK(fr.width > 0 && fr.height > 0); }
    }
    CHECK(has_rules);
    // the pixels are those of the desktop's export at zoom 1 (which made the goldens)
    F::ExportOptions plain;
    plain.zoom = 1.0; plain.angle_degrees = 0; plain.bleed = false; plain.both_faces = false; plain.metadata = false;
    F::BytesResult png = F::exportCardImage(s, uid, plain);
    REQUIRE_OK(png);
    Image exported;
    CHECK(decode_png(png.data, exported));
    CHECK_EQ(exported.GetWidth(), w);
    CHECK_EQ(exported.GetHeight(), h);
    if (exported.GetWidth() == w && exported.GetHeight() == h) {
      size_t differ = 0;
      for (int y = 0 ; y < h ; ++y) for (int x = 0 ; x < w ; ++x) {
        const Byte* e = exported.GetData() + 3 * ((size_t)y * w + x);
        const std::uint8_t* p = buf.data() + (size_t)y * stride + 4 * x;
        if (e[0] != p[0] || e[1] != p[1] || e[2] != p[2] || p[3] != 255) ++differ;
      }
      CHECK_EQ(differ, (size_t)0);
    }
    // the padding at the end of each row is untouched
    CHECK_EQ(buf[(size_t)stride - 1], 0x55);
    if (have_goldens) {
      std::string n = std::to_string(i);
      CHECK_EQ(dump.text, read_text(goldens_dir / (n + ".json")));
      std::string golden_png = read_text(goldens_dir / (n + ".png"));
      Image golden;
      CHECK(image_codec().decode(golden_png.data(), golden_png.size(), golden));
      // the Golden images were drawn by wx: another canvas must match their Layout dump (above),
      // its pixels are only compared, advisorily, by tools/goldens (ADR 0004, ADR 0010)
      const bool exact_pixels = test_host::draws_like_goldens();
      if (!exact_pixels) {
        CHECK_EQ(golden.GetWidth(), w);
      } else if (golden.GetWidth() == w && golden.GetHeight() == h) {
        size_t differ = 0;
        for (int y = 0 ; y < h ; ++y) for (int x = 0 ; x < w ; ++x) {
          const Byte* g = golden.GetData() + 3 * ((size_t)y * w + x);
          const std::uint8_t* p = buf.data() + (size_t)y * stride + 4 * x;
          if (g[0] != p[0] || g[1] != p[1] || g[2] != p[2]) ++differ;
        }
        CHECK_EQ(differ, (size_t)0);
      } else {
        report_failure(__FILE__, __LINE__, "golden image has another size");
      }
    }
    // at half size the regions scale with it
    std::vector<std::uint8_t> half((size_t)(w / 2) * 4 * (h / 2));
    F::RenderResult rh = F::renderCard(s, uid, half.data(), w / 2, h / 2, (w / 2) * 4);
    CHECK_OK(rh);
    CHECK_EQ(rh.regions.size(), r.regions.size());
    for (size_t k = 0 ; k < rh.regions.size() && k < r.regions.size() ; ++k) {
      CHECK(std::abs(rh.regions[k].x * 2 - r.regions[k].x) < 1e-6);
      CHECK(std::abs(rh.regions[k].width * 2 - r.regions[k].width) < 1e-6);
    }
  }
  // a bad buffer is an error, not a crash
  CHECK_EQ(F::renderCard(s, "fixture-wrap", nullptr, 300, 420, 1200).status, F::Status::InvalidArgument);
  std::vector<std::uint8_t> small(16);
  CHECK_EQ(F::renderCard(s, "fixture-wrap", small.data(), 300, 420, 100).status, F::Status::InvalidArgument);
  CHECK_EQ(F::renderCard(s, "no-such-card", small.data(), 1, 1, 4).status, F::Status::NotFound);
  F::closeSession(s);
}

// ----------------------------------------------------------------------------- : Text editing

TEST(field_at_picks_the_topmost_editable_field) {
  F::SessionHandle s = open_fixture();
  // the fixture's Style is 300 wide: the name's box is at (20, 18) 200x26, the rules' at (26, 246)
  F::FieldAt name = F::fieldAt(s, "fixture-wrap", 60, 30, 300);
  REQUIRE_OK(name);
  CHECK_EQ(name.field, std::string("name"));
  // at another width, the same place
  CHECK_EQ(F::fieldAt(s, "fixture-wrap", 120, 60, 600).field, std::string("name"));
  CHECK_EQ(F::fieldAt(s, "fixture-wrap", 100, 300, 300).field, std::string("rules"));
  // outside the card: nothing
  F::FieldAt outside = F::fieldAt(s, "fixture-wrap", -5, 500, 300);
  REQUIRE_OK(outside);
  CHECK_EQ(outside.field, std::string());
  CHECK_EQ(F::fieldAt(s, "fixture-wrap", 1, 1, 0).status, F::Status::InvalidArgument);
  CHECK_EQ(F::fieldAt(s, "no-such-card", 1, 1, 300).status, F::Status::NotFound);
  CHECK_OK(F::closeSession(s));
}

TEST(replace_range_typing_merges_into_one_undo_step) {
  F::SessionHandle s = open_fixture();
  F::Target card = F::Target::card("fixture-wrap");
  F::TextResult t = F::getText(s, card, "name");
  REQUIRE_OK(t);
  CHECK_EQ(t.text, "Long Wrapping Text");
  int n = (int)t.text.size();
  CHECK_EQ(t.selection_end, n);
  // three keystrokes at the caret
  F::TextResult r1 = F::replaceRange(s, card, "name", n, n, "X");
  REQUIRE_OK(r1);
  CHECK_EQ(r1.selection_end, n + 1);
  CHECK(r1.can_undo);
  CHECK_EQ(r1.undo_name, "Typing");
  F::TextResult r2 = F::replaceRange(s, card, "name", r1.selection_end, r1.selection_end, "Y");
  F::TextResult r3 = F::replaceRange(s, card, "name", r2.selection_end, r2.selection_end, "Z");
  REQUIRE_OK(r3);
  CHECK_EQ(r3.text, "Long Wrapping TextXYZ");
  CHECK_EQ(r3.selection_start, n + 3);
  CHECK_EQ(r3.selection_end, n + 3);
  // one undo step takes all three away
  F::ActionResult u = F::undo(s);
  REQUIRE_OK(u);
  CHECK(!u.can_undo);
  CHECK(u.can_redo);
  CHECK_EQ(F::getText(s, card, "name").text, "Long Wrapping Text");
  CHECK_OK(F::redo(s));
  CHECK_EQ(F::getText(s, card, "name").text, "Long Wrapping TextXYZ");
  // two backspaces merge too
  F::TextResult b1 = F::replaceRange(s, card, "name", n + 2, n + 3, "");
  F::TextResult b2 = F::replaceRange(s, card, "name", b1.selection_end - 1, b1.selection_end, "");
  REQUIRE_OK(b2);
  CHECK_EQ(b2.text, "Long Wrapping TextX");
  CHECK_EQ(b2.undo_name, "Backspace");
  CHECK_OK(F::undo(s));
  CHECK_EQ(F::getText(s, card, "name").text, "Long Wrapping TextXYZ");
  // typing somewhere else is a new step
  F::replaceRange(s, card, "name", 0, 0, "A");
  F::TextResult other = F::replaceRange(s, card, "name", 10, 10, "B");
  CHECK_EQ(other.text, "ALong WrapBping TextXYZ");
  CHECK_OK(F::undo(s));
  CHECK_EQ(F::getText(s, card, "name").text, "ALong Wrapping TextXYZ");
  // replacing a selection
  F::TextResult sel = F::replaceRange(s, card, "name", 1, 5, "Short");
  CHECK_EQ(sel.text, "AShort Wrapping TextXYZ");
  CHECK_EQ(sel.selection_end, 6);
  // out of range positions are clamped; a single-line field drops newlines
  F::TextResult clamp = F::replaceRange(s, card, "name", 1000, 1000, "!\n");
  CHECK_OK(clamp);
  CHECK_EQ(clamp.text, "AShort Wrapping TextXYZ!");
  CHECK_EQ(F::replaceRange(s, card, "name", -1, 0, "x").status, F::Status::InvalidArgument);
  F::closeSession(s);
}

TEST(replace_range_symbols_and_auto_replace) {
  F::SessionHandle s = open_fixture();
  F::Target card = F::Target::card("fixture-front");
  F::TextResult t = F::getText(s, card, "rules");
  REQUIRE_OK(t);
  CHECK_EQ(t.text, "Transform: pay U.");
  // as a symbol
  int n = (int)t.text.size();
  F::TextResult sym = F::replaceRange(s, card, "rules", n, n, "R", true);
  REQUIRE_OK(sym);
  CHECK_EQ(sym.text, "Transform: pay U.R");
  CHECK_EQ(F::getValue(s, card, "rules").value, "Transform: pay <sym>U</sym>.<sym>R</sym>");
  // auto replace with the host's rules, not the global settings
  F::AutoReplaceSettings ar;
  ar.rules.push_back(F::AutoReplaceRule{"teh", "the", true, true});
  CHECK_OK(F::setAutoReplace(s, ar));
  F::AutoReplaceResult got = F::getAutoReplace(s);
  CHECK_EQ(got.settings.rules.size(), (size_t)1);
  F::Target name = F::Target::card("fixture-front");
  F::TextResult r = F::getText(s, name, "name");
  int pos = (int)r.text.size();
  for (const char* c : {" ", "t", "e", "h"}) {
    r = F::replaceRange(s, name, "name", pos, pos, c);
    pos = r.selection_end;
  }
  CHECK_EQ(r.text, "Front Face the");
  CHECK_EQ(r.selection_end, (int)r.text.size());
  // disabled: no replacing
  ar.enabled = false;
  CHECK_OK(F::setAutoReplace(s, ar));
  for (const char* c : {" ", "t", "e", "h"}) {
    r = F::replaceRange(s, name, "name", pos, pos, c);
    pos = r.selection_end;
  }
  CHECK_EQ(r.text, "Front Face the teh");
  // not a text field
  CHECK_EQ(F::replaceRange(s, card, "frame", 0, 0, "x").status, F::Status::InvalidArgument);
  F::closeSession(s);
}

TEST(symbol_menu_and_previews) {
  F::SessionHandle s = open_fixture();
  F::Target wrap = F::Target::card("fixture-wrap");
  // the rules use the fixture's symbol font, and its menu
  F::SymbolMenu menu = F::symbolMenu(s, wrap, "rules");
  REQUIRE_OK(menu);
  CHECK(menu.has_symbols);
  CHECK(!menu.always_symbol);
  REQUIRE(menu.items.size() == 4);
  CHECK(menu.items[0].type == F::SymbolMenuItem::Type::Code);
  CHECK_EQ(menu.items[0].code, std::string("R"));
  CHECK_EQ(menu.items[0].id, 0);
  CHECK(menu.items[1].type == F::SymbolMenuItem::Type::Line);
  CHECK(menu.items[2].type == F::SymbolMenuItem::Type::Submenu);
  CHECK_EQ(menu.items[2].label, std::string("Colors")); // without the '&'
  REQUIRE(menu.items[2].items.size() == 2);
  CHECK_EQ(menu.items[2].items[0].code, std::string("U"));
  CHECK_EQ(menu.items[2].items[0].id, 1);
  CHECK(menu.items[3].type == F::SymbolMenuItem::Type::Custom);
  CHECK_EQ(menu.items[3].prompt, std::string("Which symbol?"));
  CHECK_EQ(menu.items[3].label, std::string("Other…")); // (without the shortcut after the tab)
  CHECK_EQ(menu.items[3].id, 3);
  // the blue symbol's preview: square-ish, mostly blue where it is opaque
  F::ImageData blue = F::symbolPreview(s, wrap, "rules", 1, 32);
  REQUIRE_OK(blue);
  CHECK(blue.width > 0 && blue.width <= 32 && blue.height > 0 && blue.height <= 32);
  REQUIRE(blue.rgba.size() == (size_t)blue.width * blue.height * 4);
  long r = 0, b = 0;
  for (size_t i = 0 ; i < blue.rgba.size() ; i += 4) if (blue.rgba[i+3] > 128) { r += blue.rgba[i]; b += blue.rgba[i+2]; }
  CHECK(b > r);
  CHECK_EQ(F::symbolPreview(s, wrap, "rules", 9, 32).status, F::Status::NotFound);
  // inserting the item's code as a symbol
  F::TextResult t = F::replaceRange(s, wrap, "rules", 0, 0, menu.items[2].items[0].code, true);
  REQUIRE_OK(t);
  CHECK(F::getValue(s, wrap, "rules").value.rfind("<sym>U</sym>", 0) == 0);
  // the name has no symbol font; a choice field is not a text field
  F::SymbolMenu name = F::symbolMenu(s, wrap, "name");
  REQUIRE_OK(name);
  CHECK(!name.has_symbols);
  CHECK(name.items.empty());
  CHECK_EQ(F::symbolMenu(s, wrap, "frame").status, F::Status::InvalidArgument);
  CHECK_OK(F::closeSession(s));
}

TEST(remove_accents_stays_in_its_tables) {
  // #54: each Latin block folds to its base letter, at both ends of its table
  CHECK_EQ(remove_accents(0xE0), (Char)'a');   // à, Latin-1
  CHECK_EQ(remove_accents(0x101), (Char)'a');  // ā, Latin Extended-A
  CHECK_EQ(remove_accents(0x17F), (Char)'s');  // ſ long s, the last of Extended-A
  CHECK_EQ(remove_accents(0x180), (Char)'b');  // ƀ, the first of Extended-B
  CHECK_EQ(remove_accents(0x1E5), (Char)'g');  // ǥ
  CHECK_EQ(remove_accents(0x24F), (Char)'y');  // ɏ, the last of Extended-B
  CHECK_EQ(remove_accents(0x1EA1), (Char)'a'); // ạ, Latin Extended Additional
  // and everything outside them is left alone (this read far outside latin_E)
  CHECK_EQ(remove_accents(0x3C9), (Char)0x3C9);   // ω omega
  CHECK_EQ(remove_accents(0x436), (Char)0x436);   // ж Cyrillic zhe
  CHECK_EQ(remove_accents(0x663), (Char)0x663);   // ٣ Arabic-Indic three
  CHECK_EQ(remove_accents(0x1DFF), (Char)0x1DFF);
  CHECK_EQ(remove_accents(0x1F00), (Char)0x1F00); // ἀ, Greek Extended
  // so sorting non-Latin text is stable: ψ before ω
  CHECK_EQ(smart_compare(String(1, (Char)0x3C8), String(1, (Char)0x3C9)), -1);
}

TEST(visible_text_and_separators) {
  // the core side of replaceRange's positions: atoms show as their contents and are locked
  VisibleText vt = visible_text(_("a<sep> - </sep>b<atom-reminder>(x)</atom-reminder>"));
  CHECK(vt.text == _("a - b(x)"));
  CHECK_EQ(vt.locked.size(), (size_t)2);
  // of which the separator divides the field's parts (rules / flavor, #83); the atom doesn't
  CHECK_EQ(vt.separators.size(), (size_t)1);
  if (!vt.separators.empty()) { CHECK_EQ(vt.separators[0].first, (size_t)1); CHECK_EQ(vt.separators[0].second, (size_t)4); }
  CHECK_EQ(vt.toCursor(0), (size_t)0);
  CHECK_EQ(vt.toCursor(1), (size_t)1);
  CHECK_EQ(vt.toCursor(2), (size_t)1); // inside the separator: nearest edge
  CHECK_EQ(vt.toCursor(3), (size_t)2);
  CHECK_EQ(vt.toCursor(4), (size_t)2);
  CHECK_EQ(vt.toCursor(5), (size_t)3);
  CHECK_EQ(vt.toVisible(3), (size_t)5);
  // a selection never crosses a separator
  String value = _("ab<sep> - </sep>cd");
  TextSelection sel;
  sel.start = 1; sel.end = 4;
  fix_text_selection(value, sel);
  CHECK_EQ(sel.end, (size_t)2);
}

// ----------------------------------------------------------------------------- : Undo and dirty

TEST(undo_redo_and_dirty_flag) {
  F::SessionHandle s = open_fixture();
  F::Target card = F::Target::card("fixture-back");
  CHECK(!F::sessionInfo(s).dirty);
  F::ActionResult a = F::setValue(s, card, "frame", "red");
  REQUIRE_OK(a);
  CHECK(a.dirty);
  CHECK_EQ(F::getValue(s, card, "frame").value, "red");
  // saving to a file descriptor makes it clean
  fs::path out = work_dir / "dirty.mse-set";
  int fd = ::open(out.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  CHECK(fd >= 0);
  F::ActionResult saved = F::saveTo(s, fd);
  ::close(fd);
  REQUIRE_OK(saved);
  CHECK(!saved.dirty);
  F::ActionResult u = F::undo(s);
  CHECK(u.dirty);
  CHECK_EQ(F::getValue(s, card, "frame").value, "grey");
  F::ActionResult r = F::redo(s);
  CHECK(!r.dirty);
  CHECK_EQ(F::getValue(s, card, "frame").value, "red");
  CHECK_EQ(F::redo(s).status, F::Status::InvalidArgument); // nothing to redo
  // autosave writes the working copy
  CHECK_OK(F::flush(s));
  CHECK(read_text(work_dir / "sessions" / ("s" + std::to_string(session_counter) + ".mse-set") / "set").find("frame: red") != std::string::npos);
  F::closeSession(s);
}

// ----------------------------------------------------------------------------- : Stale cards

TEST(stale_cards) {
  F::SessionHandle s = open_fixture();
  F::ActionResult a = F::setValue(s, F::Target::card("fixture-wrap"), "level", "1");
  REQUIRE_OK(a);
  CHECK(!a.stale.all);
  CHECK(contains(a.stale.card_uids, "fixture-wrap"));
  CHECK(!contains(a.stale.card_uids, "fixture-back"));
  F::TextResult t = F::replaceRange(s, F::Target::card("fixture-back"), "rules", 0, 0, "X");
  CHECK(!t.stale.all);
  CHECK(contains(t.stale.card_uids, "fixture-back"));
  F::ActionResult u = F::undo(s);
  CHECK(contains(u.stale.card_uids, "fixture-back"));
  // set info is on every card
  F::ActionResult set_info = F::setValue(s, F::Target::set(), "title", "Another title");
  REQUIRE_OK(set_info);
  CHECK(set_info.stale.all);
  CHECK_EQ(F::getValue(s, F::Target::set(), "title").value, "Another title");
  CHECK(F::undo(s).stale.all);
  F::closeSession(s);
}

// ----------------------------------------------------------------------------- : Linked cards

TEST(link_and_unlink_are_one_undo_step_each) {
  F::SessionHandle s = open_fixture();
  F::LinkTypeList types = F::listLinkTypes(s);
  REQUIRE_OK(types);
  // the fixture Game declares Front Face/Back Face, which the core also adds built in: listed once
  CHECK_EQ(types.types.size(), (size_t)1);
  if (!types.types.empty()) {
    CHECK_EQ(types.types[0].selected, "Front Face");
    CHECK_EQ(types.types[0].linked, "Back Face");
  }
  F::ActionResult l = F::link(s, "fixture-wrap", "fixture-back", "Front Face", "Back Face");
  REQUIRE_OK(l);
  CHECK(contains(l.stale.card_uids, "fixture-wrap"));
  CHECK(contains(l.stale.card_uids, "fixture-back"));
  F::CardList cards = F::listCards(s);
  const F::CardInfo* wrap = find_card(cards, "fixture-wrap");
  const F::CardInfo* back = find_card(cards, "fixture-back");
  CHECK(wrap && wrap->links.size() == 1 && wrap->links[0].uid == "fixture-back" && wrap->links[0].relation == "Back Face");
  CHECK(back && back->links.size() == 2 && back->links[1].uid == "fixture-wrap" && back->links[1].relation == "Front Face");
  // one undo step
  F::ActionResult u = F::undo(s);
  CHECK(!u.can_undo);
  cards = F::listCards(s);
  CHECK(find_card(cards, "fixture-wrap")->links.empty());
  CHECK_EQ(find_card(cards, "fixture-back")->links.size(), (size_t)1);
  // unlink the fixture's own pair, one undo step
  F::ActionResult ul = F::unlink(s, "fixture-front", "fixture-back");
  REQUIRE_OK(ul);
  cards = F::listCards(s);
  CHECK(find_card(cards, "fixture-front")->links.empty());
  CHECK(find_card(cards, "fixture-back")->links.empty());
  CHECK_OK(F::undo(s));
  cards = F::listCards(s);
  CHECK_EQ(find_card(cards, "fixture-front")->links.size(), (size_t)1);
  CHECK_EQ(find_card(cards, "fixture-back")->links.size(), (size_t)1);
  CHECK(!F::sessionInfo(s).can_undo);
  // errors
  CHECK_EQ(F::link(s, "fixture-wrap", "fixture-wrap", "a", "b").status, F::Status::InvalidArgument);
  CHECK_EQ(F::unlink(s, "fixture-wrap", "fixture-back").status, F::Status::InvalidArgument);
  F::closeSession(s);
}

// ----------------------------------------------------------------------------- : Keywords

static const F::KeywordInfo* find_keyword(const F::KeywordList& list, const std::string& name) {
  for (const F::KeywordInfo& k : list.keywords) if (k.keyword == name) return &k;
  return nullptr;
}

static std::string to_s(const F::TextSpan& span) {
  return "[" + std::to_string(span.start) + "," + std::to_string(span.end) + ")";
}

TEST(list_keywords_modes_and_parameter_types) {
  F::SessionHandle s = open_fixture();
  F::KeywordList list = F::listKeywords(s);
  REQUIRE_OK(list);
  // the fixture set has none of its own: the Game's two
  CHECK_EQ(list.keywords.size(), (size_t)2);
  const F::KeywordInfo* swift  = find_keyword(list, "Swiftness");
  const F::KeywordInfo* charge = find_keyword(list, "Charge");
  if (!swift || !charge) { report_failure(__FILE__, __LINE__, "the Game's keywords are not listed"); return; }
  CHECK(swift->from_game);
  CHECK(swift->valid);
  CHECK_EQ(swift->match, "Swiftness");
  CHECK_EQ(swift->reminder, "This card acts first.");
  CHECK_EQ(swift->mode, "core");
  CHECK(swift->parameters.empty());
  CHECK_EQ(swift->usage, 1); // on fixture-back
  CHECK_EQ(charge->match, "Charge <number>");
  CHECK_EQ(charge->parameters.size(), (size_t)1);
  if (!charge->parameters.empty()) CHECK_EQ(charge->parameters[0], "number");
  CHECK_EQ(charge->reminder, "Put {param1} charge counters on this card.");
  CHECK_EQ(charge->usage, 1);
  CHECK(swift->id != 0 && swift->id != charge->id);
  // the same ids every time
  F::KeywordList again = F::listKeywords(s);
  CHECK_EQ(find_keyword(again, "Swiftness")->id, swift->id);
  F::KeywordResult got = F::getKeyword(s, charge->id);
  REQUIRE_OK(got);
  CHECK_EQ(got.keyword.keyword, "Charge");
  CHECK_EQ(F::getKeyword(s, 999999).status, F::Status::NotFound);
  // the Game's modes and parameter types
  F::KeywordModeList modes = F::listKeywordModes(s);
  REQUIRE_OK(modes);
  CHECK_EQ(modes.modes.size(), (size_t)2);
  if (modes.modes.size() == 2) {
    CHECK_EQ(modes.modes[0].name, "core");
    CHECK(!modes.modes[0].is_default);
    CHECK_EQ(modes.modes[1].name, "custom");
    CHECK(modes.modes[1].is_default);
  }
  F::KeywordParamTypeList types = F::listKeywordParamTypes(s);
  REQUIRE_OK(types);
  CHECK_EQ(types.types.size(), (size_t)1);
  if (!types.types.empty()) {
    CHECK_EQ(types.types[0].name, "number");
    CHECK_EQ(types.types[0].placeholder, "N");
    CHECK_EQ(types.types[0].example, "2");
  }
  // a keyword's fields
  F::Target kw = F::Target::keyword(charge->id);
  F::FieldList fields = F::listFields(s, kw);
  REQUIRE_OK(fields);
  CHECK_EQ(fields.fields.size(), (size_t)5);
  for (const F::FieldInfo& f : fields.fields) {
    CHECK(!f.editable); // the Game's
    if (f.name == "mode") { CHECK_EQ(f.type, "choice"); CHECK(contains(f.choices, "custom")); }
    else CHECK_EQ(f.type, "text");
  }
  CHECK_EQ(F::getValue(s, kw, "mode").value, "core");
  CHECK_EQ(F::getValue(s, kw, "match").value, "Charge <atom-param>number</atom-param>");
  F::TextResult match = F::getText(s, kw, "match");
  REQUIRE_OK(match);
  CHECK_EQ(match.text, "Charge number");
  CHECK_EQ(match.locked.size(), (size_t)1);
  if (!match.locked.empty()) { CHECK_EQ(match.locked[0].start, 7); CHECK_EQ(match.locked[0].end, 13); }
  CHECK_EQ(F::getText(s, kw, "reminder").text, "Put {param1} charge counters on this card.");
  // the Game's keywords are read-only
  CHECK_EQ(F::replaceRange(s, kw, "keyword", 0, 0, "X").status, F::Status::InvalidArgument);
  CHECK_EQ(F::setValue(s, kw, "reminder", "x").status, F::Status::InvalidArgument);
  CHECK_EQ(F::setValue(s, kw, "mode", "custom").status, F::Status::InvalidArgument);
  CHECK_EQ(F::removeKeyword(s, charge->id).status, F::Status::InvalidArgument);
  CHECK_EQ(F::insertKeywordParameter(s, charge->id, 0, 0, "number").status, F::Status::InvalidArgument);
  CHECK_EQ(F::getText(s, kw, "mode").status, F::Status::InvalidArgument);
  CHECK_EQ(F::getText(s, kw, "nothing").status, F::Status::NotFound);
  CHECK(!F::sessionInfo(s).can_undo);
  F::closeSession(s);
}

TEST(add_edit_and_remove_keywords_with_undo) {
  F::SessionHandle s = open_fixture();
  F::Target wrap = F::Target::card("fixture-wrap");
  // a card that uses a keyword the set doesn't have yet
  std::string rules = F::getValue(s, wrap, "rules").value;
  REQUIRE_OK(F::setValue(s, wrap, "rules", rules + "\nGlimmer"));
  std::string before = F::getText(s, wrap, "rules").text;
  // adding a keyword no card uses: no stale cards
  F::NewKeywordResult unused = F::addKeyword(s, F::NewKeyword{"Unused", "Unused", "Nothing.", "", ""});
  REQUIRE_OK(unused);
  CHECK(unused.stale.empty());
  CHECK_OK(F::undo(s));
  // adding one that the card uses: the card is stale, and shows its reminder text
  F::NewKeywordResult add = F::addKeyword(s, F::NewKeyword{"Glimmer", "Glimmer", "It shines.", "Shiny cards.", ""});
  REQUIRE_OK(add);
  CHECK(add.id != 0);
  CHECK(!add.stale.all);
  CHECK(contains(add.stale.card_uids, "fixture-wrap"));
  CHECK(!contains(add.stale.card_uids, "fixture-back"));
  CHECK(add.dirty);
  CHECK_EQ(add.undo_name, "Add Item");
  CHECK_EQ(F::getText(s, wrap, "rules").text, before + " (It shines.)");
  F::KeywordList list = F::listKeywords(s);
  REQUIRE_OK(list);
  CHECK_EQ(list.keywords.size(), (size_t)3);
  CHECK_EQ(list.keywords[0].id, add.id); // the Set's first
  CHECK(!list.keywords[0].from_game);
  CHECK_EQ(list.keywords[0].mode, "custom"); // the default mode
  CHECK_EQ(list.keywords[0].rules, "Shiny cards.");
  CHECK_EQ(list.keywords[0].usage, 1);
  F::Target kw = F::Target::keyword(add.id);
  F::FieldList fields = F::listFields(s, kw);
  for (const F::FieldInfo& f : fields.fields) CHECK(f.editable);
  // typing in the reminder text: one undo step, and the card is stale each time
  F::TextResult t = F::getText(s, kw, "reminder");
  REQUIRE_OK(t);
  CHECK_EQ(t.text, "It shines.");
  int pos = 9; // before the "."
  for (const char* c : {" ", "a", "l", "l"}) {
    t = F::replaceRange(s, kw, "reminder", pos, pos, c);
    REQUIRE_OK(t);
    CHECK(contains(t.stale.card_uids, "fixture-wrap"));
    CHECK(!contains(t.stale.card_uids, "fixture-front"));
    pos = t.selection_end;
  }
  CHECK_EQ(t.text, "It shines all.");
  CHECK_EQ(t.undo_name, "Typing");
  CHECK_EQ(F::getText(s, wrap, "rules").text, before + " (It shines all.)");
  CHECK_EQ(F::getKeyword(s, add.id).keyword.reminder, "It shines all.");
  F::ActionResult u = F::undo(s);
  REQUIRE_OK(u);
  CHECK(contains(u.stale.card_uids, "fixture-wrap"));
  CHECK_EQ(F::getText(s, kw, "reminder").text, "It shines.");
  CHECK_EQ(F::getText(s, wrap, "rules").text, before + " (It shines.)");
  CHECK_EQ(u.undo_name, "Add Item"); // the typing was one step
  // a reminder text that doesn't compile is kept, with its error, and not used
  t = F::replaceRange(s, kw, "reminder", 0, 0, "{");
  REQUIRE_OK(t);
  CHECK(t.stale.empty());
  F::KeywordResult broken = F::getKeyword(s, add.id);
  CHECK(!broken.keyword.reminder_error.empty());
  CHECK(!broken.keyword.valid);
  CHECK_EQ(broken.keyword.reminder, "It shines.");
  CHECK_EQ(F::getText(s, wrap, "rules").text, before + " (It shines.)");
  CHECK_OK(F::undo(s));
  CHECK(F::getKeyword(s, add.id).keyword.reminder_error.empty());
  // the name: not on the card, so nothing is stale
  t = F::replaceRange(s, kw, "keyword", 7, 7, "s");
  REQUIRE_OK(t);
  CHECK_EQ(t.text, "Glimmers");
  CHECK(t.stale.empty());
  CHECK_EQ(F::getKeyword(s, add.id).keyword.keyword, "Glimmers");
  CHECK_OK(F::undo(s));
  // the mode: one undo step, none for the same mode, and only the Game's modes
  F::ActionResult m = F::setValue(s, kw, "mode", "core");
  REQUIRE_OK(m);
  CHECK_EQ(F::getValue(s, kw, "mode").value, "core");
  CHECK(m.stale.empty()); // the fixture's reminder text doesn't depend on the mode
  CHECK_EQ(m.undo_name, "Keyword Mode");
  CHECK_OK(F::setValue(s, kw, "mode", "core"));
  CHECK_EQ(F::setValue(s, kw, "mode", "nope").status, F::Status::InvalidArgument);
  CHECK_OK(F::undo(s));
  CHECK_EQ(F::getValue(s, kw, "mode").value, "custom");
  // the match: the card no longer uses the keyword
  F::ActionResult mt = F::setValue(s, kw, "match", "Shimmer");
  REQUIRE_OK(mt);
  CHECK(contains(mt.stale.card_uids, "fixture-wrap"));
  CHECK_EQ(F::getText(s, wrap, "rules").text, before);
  CHECK_EQ(F::getKeyword(s, add.id).keyword.usage, 0);
  F::ActionResult mu = F::undo(s);
  CHECK(contains(mu.stale.card_uids, "fixture-wrap"));
  CHECK_EQ(F::getText(s, wrap, "rules").text, before + " (It shines.)");
  // a parameter in the match: a locked span
  F::TextResult sp = F::replaceRange(s, kw, "match", 7, 7, " ");
  F::TextResult param = F::insertKeywordParameter(s, add.id, sp.selection_end, sp.selection_end, "number");
  REQUIRE_OK(param);
  CHECK_EQ(param.text, "Glimmer number");
  CHECK_EQ(param.locked.size(), (size_t)1);
  if (!param.locked.empty()) CHECK_EQ(to_s(param.locked[0]), "[8,14)");
  CHECK_EQ(param.selection_end, 14);
  CHECK_EQ(param.undo_name, "Insert Parameter");
  CHECK_EQ(F::getValue(s, kw, "match").value, "Glimmer <atom-param>number</atom-param>");
  F::KeywordResult with_param = F::getKeyword(s, add.id);
  CHECK_EQ(with_param.keyword.match, "Glimmer <number>");
  CHECK_EQ(with_param.keyword.parameters.size(), (size_t)1);
  CHECK_EQ(F::insertKeywordParameter(s, add.id, 0, 0, "colour").status, F::Status::InvalidArgument);
  CHECK_OK(F::undo(s));
  CHECK_OK(F::undo(s));
  CHECK_EQ(F::getValue(s, kw, "match").value, "Glimmer");
  CHECK_EQ(F::getText(s, wrap, "rules").text, before + " (It shines.)");
  // removing: the card is stale, and undo puts the keyword back with the same id
  F::ActionResult rm = F::removeKeyword(s, add.id);
  REQUIRE_OK(rm);
  CHECK(contains(rm.stale.card_uids, "fixture-wrap"));
  CHECK_EQ(F::getText(s, wrap, "rules").text, before);
  CHECK_EQ(F::listKeywords(s).keywords.size(), (size_t)2);
  CHECK_EQ(F::getKeyword(s, add.id).status, F::Status::NotFound);
  CHECK_EQ(F::removeKeyword(s, add.id).status, F::Status::NotFound);
  CHECK_EQ(F::replaceRange(s, kw, "reminder", 0, 0, "x").status, F::Status::NotFound);
  F::ActionResult back = F::undo(s);
  REQUIRE_OK(back);
  CHECK(contains(back.stale.card_uids, "fixture-wrap"));
  CHECK_EQ(F::getKeyword(s, add.id).keyword.keyword, "Glimmer");
  CHECK_EQ(F::getText(s, wrap, "rules").text, before + " (It shines.)");
  CHECK_OK(F::redo(s));
  CHECK_EQ(F::getText(s, wrap, "rules").text, before);
  CHECK_OK(F::undo(s)); // back once more
  // bad new keywords add nothing (and no undo step: the redo is still there)
  CHECK_EQ(F::addKeyword(s, F::NewKeyword{"Bad", "Bad", "{", "", ""}).status, F::Status::InvalidArgument);
  CHECK_EQ(F::addKeyword(s, F::NewKeyword{"Bad", "Bad", "", "", "nope"}).status, F::Status::InvalidArgument);
  CHECK_EQ(F::listKeywords(s).keywords.size(), (size_t)3);
  CHECK(F::sessionInfo(s).can_redo);
  // the keyword is saved with the set
  CHECK_OK(F::flush(s));
  std::string saved = read_text(work_dir / "sessions" / ("s" + std::to_string(session_counter) + ".mse-set") / "set");
  CHECK(saved.find("keyword: Glimmer") != std::string::npos);
  CHECK(saved.find("reminder: It shines.") != std::string::npos);
  F::closeSession(s);
}

TEST(locked_spans_of_keywords_in_replace_range) {
  F::SessionHandle s = open_fixture();
  F::Target back = F::Target::card("fixture-back");
  F::TextResult t = F::getText(s, back, "rules");
  REQUIRE_OK(t);
  const std::string reminder1 = " (This card acts first.)";
  const std::string line1 = "Swiftness" + reminder1;
  const std::string line2 = "Charge 2 (Put 2 charge counters on this card.)";
  const std::string line3 = "The other side of U Front Face.";
  CHECK_EQ(t.text, line1 + "\n" + line2 + "\n" + line3);
  // each reminder text is one locked span
  CHECK_EQ(t.locked.size(), (size_t)2);
  if (t.locked.size() == 2) {
    CHECK_EQ(to_s(t.locked[0]), "[9,33)");
    CHECK_EQ(to_s(t.locked[1]), "[42,80)");
  }
  // a position inside a span snaps to its nearest edge: near the end, so after it
  F::TextResult after = F::replaceRange(s, back, "rules", 30, 30, "!");
  REQUIRE_OK(after);
  CHECK_EQ(after.text, line1 + "!\n" + line2 + "\n" + line3);
  CHECK_EQ(after.selection_end, 34);
  CHECK_OK(F::undo(s));
  // near the start, so before it; the script puts the reminder text right after the keyword
  // again, and the caret follows the text typed
  F::TextResult before = F::replaceRange(s, back, "rules", 10, 10, " ");
  REQUIRE_OK(before);
  CHECK_EQ(before.text, line1 + " \n" + line2 + "\n" + line3);
  CHECK_EQ(before.selection_end, 34);
  CHECK_OK(F::undo(s));
  CHECK_EQ(F::getText(s, back, "rules").text, t.text);
  // a deletion that ends inside a span takes the whole span
  F::TextResult cut = F::replaceRange(s, back, "rules", 5, 25, "");
  REQUIRE_OK(cut);
  CHECK_EQ(cut.text, "Swift\n" + line2 + "\n" + line3);
  CHECK_EQ(cut.selection_end, 5);
  CHECK_EQ(cut.locked.size(), (size_t)1);
  CHECK_OK(F::undo(s));
  // replacing a selection that covers part of a span replaces the whole span
  F::TextResult repl = F::replaceRange(s, back, "rules", 41, 70, "3");
  REQUIRE_OK(repl);
  CHECK_EQ(repl.text, line1 + "\nCharge 3 (Put 3 charge counters on this card.)\n" + line3);
  CHECK_OK(F::undo(s));
  F::closeSession(s);
}

TEST(prefix_and_suffix_are_not_editable) {
  // <prefix> and <suffix> (as combined_editor writes them) are not part of the visible text, and
  // edits never reach into them
  F::SessionHandle s = open_fixture();
  F::Target front = F::Target::card("fixture-front");
  REQUIRE_OK(F::setValue(s, front, "name", "<prefix>Pre: </prefix>Body<suffix> :Suf</suffix>"));
  F::TextResult t = F::getText(s, front, "name");
  REQUIRE_OK(t);
  CHECK_EQ(t.text, "Body");
  CHECK_EQ(t.selection_end, 4);
  F::TextResult start = F::replaceRange(s, front, "name", 0, 0, "A");
  REQUIRE_OK(start);
  CHECK_EQ(start.text, "ABody");
  CHECK_EQ(F::getValue(s, front, "name").value, "<prefix>Pre: </prefix>ABody<suffix> :Suf</suffix>");
  F::TextResult end = F::replaceRange(s, front, "name", 5, 5, "Z");
  CHECK_EQ(F::getValue(s, front, "name").value, "<prefix>Pre: </prefix>ABodyZ<suffix> :Suf</suffix>");
  // deleting everything leaves them
  F::TextResult all = F::replaceRange(s, front, "name", 0, 1000, "");
  REQUIRE_OK(all);
  CHECK_EQ(all.text, "");
  CHECK_EQ(F::getValue(s, front, "name").value, "<prefix>Pre: </prefix><suffix> :Suf</suffix>");
  F::closeSession(s);
}

// ----------------------------------------------------------------------------- : Changing the card list

/// The uids of a session's cards, in order
static std::vector<std::string> card_order(F::SessionHandle s) {
  std::vector<std::string> uids;
  for (const F::CardInfo& c : F::listCards(s).cards) uids.push_back(c.uid);
  return uids;
}
static std::vector<std::string> uids(std::initializer_list<std::string> l) { return l; }
static std::string to_s(const std::vector<std::string>& v) {
  std::string out = "[";
  for (const std::string& x : v) out += (out.size() > 1 ? ", " : "") + x;
  return out + "]";
}

/// The fixture's `number` field: the card's position in the set, from a card list script
static std::string card_number(F::SessionHandle s, const std::string& uid) {
  return F::getValue(s, F::Target::card(uid), "number").value;
}

TEST(add_card) {
  F::SessionHandle s = open_fixture();
  F::NewCardsResult a = F::addCard(s);
  REQUIRE_OK(a);
  CHECK_EQ(a.uids.size(), (size_t)1);
  if (a.uids.size() != 1) return;
  std::string added = a.uids[0];
  CHECK_EQ(card_order(s), uids({"fixture-wrap", "fixture-front", "fixture-back", added}));
  CHECK(a.stale.card_list);
  CHECK(!a.stale.all);
  CHECK_EQ(a.stale.card_uids, uids({added})); // the other cards keep their number
  CHECK(a.dirty);
  CHECK(!a.undo_name.empty());
  // the Game's initial values, with the Set's Style
  CHECK_EQ(F::getValue(s, F::Target::card(added), "frame").value, "grey");
  CHECK_EQ(F::getValue(s, F::Target::card(added), "name").value, "");
  CHECK_EQ(card_number(s, added), "4");
  F::CardList listed = F::listCards(s);
  CHECK_EQ(listed.cards[3].stylesheet, "fixture-standard.mse-style");
  // undo and redo
  F::ActionResult u = F::undo(s);
  REQUIRE_OK(u);
  CHECK(u.stale.card_list);
  CHECK(!u.can_undo);
  CHECK(!u.dirty);
  CHECK_EQ(card_order(s), uids({"fixture-wrap", "fixture-front", "fixture-back"}));
  F::ActionResult r = F::redo(s);
  REQUIRE_OK(r);
  CHECK(r.stale.card_list);
  CHECK(contains(r.stale.card_uids, added));
  CHECK_EQ(card_order(s), uids({"fixture-wrap", "fixture-front", "fixture-back", added}));
  // at a position: the cards after it get a new number, and are stale
  F::NewCardsResult first = F::addCard(s, 0);
  REQUIRE_OK(first);
  std::string at0 = first.uids.empty() ? "" : first.uids[0];
  CHECK_EQ(card_order(s), uids({at0, "fixture-wrap", "fixture-front", "fixture-back", added}));
  CHECK(!first.stale.all);
  for (const std::string& uid : card_order(s)) CHECK(contains(first.stale.card_uids, uid));
  CHECK_EQ(card_number(s, at0), "1");
  CHECK_EQ(card_number(s, "fixture-wrap"), "2");
  CHECK_EQ(card_number(s, added), "5");
  F::ActionResult u0 = F::undo(s); // one undo step
  CHECK(u0.can_undo);
  CHECK_EQ(card_order(s), uids({"fixture-wrap", "fixture-front", "fixture-back", added}));
  CHECK_EQ(card_number(s, "fixture-wrap"), "1");
  CHECK(contains(u0.stale.card_uids, "fixture-wrap"));
  F::NewCardsResult middle = F::addCard(s, 2);
  REQUIRE_OK(middle);
  std::vector<std::string> order = card_order(s);
  CHECK_EQ(order[2], middle.uids.at(0));
  // errors
  CHECK_EQ(F::addCard(s, 6).status, F::Status::InvalidArgument);
  CHECK_EQ(F::addCard(s, -2).status, F::Status::InvalidArgument);
  CHECK_EQ(F::addCard(999).status, F::Status::NotFound);
  CHECK_EQ(card_order(s).size(), (size_t)5);
  F::closeSession(s);
}

TEST(duplicate_cards) {
  F::SessionHandle s = open_fixture();
  CHECK_OK(F::setValue(s, F::Target::card("fixture-wrap"), "frame", "blue"));
  F::NewCardsResult d = F::duplicateCards(s, {"fixture-wrap"});
  REQUIRE_OK(d);
  CHECK_EQ(d.uids.size(), (size_t)1);
  if (d.uids.size() != 1) return;
  std::string copy = d.uids[0];
  CHECK(copy != "fixture-wrap");
  CHECK_EQ(card_order(s), uids({"fixture-wrap", copy, "fixture-front", "fixture-back"}));
  for (const char* field : {"name", "rules", "frame", "level", "picture"}) {
    CHECK_EQ(F::getValue(s, F::Target::card(copy), field).value, F::getValue(s, F::Target::card("fixture-wrap"), field).value);
  }
  CHECK(d.stale.card_list);
  CHECK(!d.stale.all);
  CHECK(contains(d.stale.card_uids, copy));
  CHECK(contains(d.stale.card_uids, "fixture-back")); // its number changed
  CHECK(!contains(d.stale.card_uids, "fixture-wrap"));
  CHECK_EQ(card_number(s, "fixture-back"), "4");
  // the copy is a card of its own
  CHECK_OK(F::setValue(s, F::Target::card(copy), "frame", "red"));
  CHECK_EQ(F::getValue(s, F::Target::card("fixture-wrap"), "frame").value, "blue");
  CHECK_OK(F::undo(s));
  F::ActionResult u = F::undo(s);
  REQUIRE_OK(u);
  CHECK(u.stale.card_list);
  CHECK_EQ(card_order(s), uids({"fixture-wrap", "fixture-front", "fixture-back"}));
  CHECK_EQ(card_number(s, "fixture-back"), "3");
  F::ActionResult r = F::redo(s);
  REQUIRE_OK(r);
  CHECK_EQ(card_order(s), uids({"fixture-wrap", copy, "fixture-front", "fixture-back"}));
  CHECK_OK(F::undo(s));
  // a Linked card comes with the card it is linked to, and the copies are linked to each other
  F::NewCardsResult pair = F::duplicateCards(s, {"fixture-front"});
  REQUIRE_OK(pair);
  CHECK_EQ(pair.uids.size(), (size_t)2);
  if (pair.uids.size() != 2) return;
  CHECK_EQ(card_order(s), uids({"fixture-wrap", "fixture-front", "fixture-back", pair.uids[0], pair.uids[1]}));
  F::CardList cards = F::listCards(s);
  const F::CardInfo* front = find_card(cards, "fixture-front");
  const F::CardInfo* back  = find_card(cards, "fixture-back");
  const F::CardInfo* front2 = find_card(cards, pair.uids[0]);
  const F::CardInfo* back2  = find_card(cards, pair.uids[1]);
  CHECK(front && front->links.size() == 1 && front->links[0].uid == "fixture-back");
  CHECK(back && back->links.size() == 1 && back->links[0].uid == "fixture-front");
  CHECK(front2 && front2->name == "Front Face" && front2->links.size() == 1 && front2->links[0].uid == pair.uids[1] && front2->links[0].relation == "Back Face");
  CHECK(back2 && back2->name == "Back Face" && back2->links.size() == 1 && back2->links[0].uid == pair.uids[0] && back2->links[0].relation == "Front Face");
  CHECK_OK(F::undo(s));
  CHECK_EQ(card_order(s).size(), (size_t)3);
  // several cards at once, one undo step: the copies go after the last of them
  F::NewCardsResult two = F::duplicateCards(s, {"fixture-back", "fixture-wrap"});
  REQUIRE_OK(two);
  CHECK_EQ(two.uids.size(), (size_t)3); // fixture-front is linked to fixture-back
  CHECK_EQ(card_order(s).size(), (size_t)6);
  F::CardList listed = F::listCards(s);
  CHECK_EQ(listed.cards[3].name, "Long Wrapping Text");
  CHECK_OK(F::undo(s));
  CHECK_EQ(card_order(s).size(), (size_t)3);
  // errors
  CHECK_EQ(F::duplicateCards(s, {}).status, F::Status::InvalidArgument);
  CHECK_EQ(F::duplicateCards(s, {"nope"}).status, F::Status::NotFound);
  CHECK_EQ(F::duplicateCards(s, {"fixture-wrap", "fixture-wrap"}).status, F::Status::InvalidArgument);
  CHECK_EQ(card_order(s).size(), (size_t)3);
  F::closeSession(s);
}

TEST(remove_cards) {
  F::SessionHandle s = open_fixture();
  F::ActionResult r = F::removeCards(s, {"fixture-wrap"});
  REQUIRE_OK(r);
  CHECK_EQ(card_order(s), uids({"fixture-front", "fixture-back"}));
  CHECK(r.stale.card_list);
  CHECK(!r.stale.all);
  CHECK(!r.undo_name.empty());
  CHECK(contains(r.stale.card_uids, "fixture-front")); // renumbered
  CHECK_EQ(card_number(s, "fixture-front"), "1");
  CHECK(r.dirty);
  F::ActionResult u = F::undo(s);
  REQUIRE_OK(u);
  CHECK(!u.dirty);
  CHECK(contains(u.stale.card_uids, "fixture-wrap")); // back again
  CHECK_EQ(card_order(s), uids({"fixture-wrap", "fixture-front", "fixture-back"}));
  CHECK_EQ(card_number(s, "fixture-front"), "2");
  CHECK_OK(F::redo(s));
  CHECK_EQ(card_order(s), uids({"fixture-front", "fixture-back"}));
  CHECK_OK(F::undo(s));
  // several cards in one step
  F::ActionResult two = F::removeCards(s, {"fixture-back", "fixture-wrap"});
  REQUIRE_OK(two);
  CHECK_EQ(card_order(s), uids({"fixture-front"}));
  CHECK(!two.undo_name.empty());
  F::ActionResult u2 = F::undo(s);
  CHECK(!u2.can_undo);
  CHECK_EQ(card_order(s), uids({"fixture-wrap", "fixture-front", "fixture-back"}));
  CHECK_EQ(find_card(F::listCards(s), "fixture-front")->links.size(), (size_t)1);
  // errors
  CHECK_EQ(F::removeCards(s, {}).status, F::Status::InvalidArgument);
  CHECK_EQ(F::removeCards(s, {"fixture-wrap", "nope"}).status, F::Status::NotFound);
  CHECK_EQ(card_order(s).size(), (size_t)3);
  CHECK(!F::sessionInfo(s).can_undo);
  F::closeSession(s);
}

TEST(remove_linked_card) {
  F::SessionHandle s = open_fixture();
  F::ActionResult r = F::removeCards(s, {"fixture-back"});
  REQUIRE_OK(r);
  CHECK_EQ(card_order(s), uids({"fixture-wrap", "fixture-front"}));
  // no dangling link on the card that stays, which is stale
  CHECK(find_card(F::listCards(s), "fixture-front")->links.empty());
  CHECK(contains(r.stale.card_uids, "fixture-front"));
  CHECK_EQ(r.undo_name, F::removeCards(s, {"fixture-wrap"}).undo_name); // named as a removal, not as unlinking
  CHECK_OK(F::undo(s));
  // undo brings back the card and the link
  F::ActionResult u = F::undo(s);
  REQUIRE_OK(u);
  CHECK(!u.can_undo);
  CHECK(contains(u.stale.card_uids, "fixture-back"));
  CHECK(contains(u.stale.card_uids, "fixture-front"));
  F::CardList cards = F::listCards(s);
  CHECK_EQ(card_order(s), uids({"fixture-wrap", "fixture-front", "fixture-back"}));
  const F::CardInfo* front = find_card(cards, "fixture-front");
  const F::CardInfo* back  = find_card(cards, "fixture-back");
  CHECK(front && front->links.size() == 1 && front->links[0].uid == "fixture-back" && front->links[0].relation == "Back Face");
  CHECK(back && back->links.size() == 1 && back->links[0].uid == "fixture-front" && back->links[0].relation == "Front Face");
  CHECK_OK(F::redo(s));
  CHECK(find_card(F::listCards(s), "fixture-front")->links.empty());
  // and the link stays gone in the saved set
  fs::path out = work_dir / "removed-linked.mse-set";
  int fd = ::open(out.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  REQUIRE_OK(F::saveTo(s, fd));
  ::close(fd);
  F::OpenResult again = F::openSession(out.string(), "");
  REQUIRE_OK(again);
  CHECK(find_card(F::listCards(again.session), "fixture-front")->links.empty());
  F::closeSession(again.session);
  // removing both cards of a pair
  CHECK_OK(F::undo(s));
  CHECK_OK(F::removeCards(s, {"fixture-front", "fixture-back"}));
  CHECK_EQ(card_order(s), uids({"fixture-wrap"}));
  CHECK_OK(F::undo(s));
  CHECK_EQ(find_card(F::listCards(s), "fixture-front")->links.size(), (size_t)1);
  CHECK_EQ(find_card(F::listCards(s), "fixture-back")->links.size(), (size_t)1);
  F::closeSession(s);
}

TEST(move_cards) {
  F::SessionHandle s = open_fixture();
  F::ActionResult m = F::moveCards(s, {"fixture-back"}, 0);
  REQUIRE_OK(m);
  CHECK_EQ(card_order(s), uids({"fixture-back", "fixture-wrap", "fixture-front"}));
  CHECK(m.stale.card_list);
  CHECK(!m.stale.all);
  for (const std::string& uid : card_order(s)) CHECK(contains(m.stale.card_uids, uid)); // all renumbered
  CHECK_EQ(card_number(s, "fixture-back"), "1");
  CHECK(m.dirty);
  CHECK(!m.undo_name.empty());
  F::ActionResult u = F::undo(s);
  REQUIRE_OK(u);
  CHECK(!u.can_undo);
  CHECK(!u.dirty);
  CHECK(u.stale.card_list);
  CHECK_EQ(card_order(s), uids({"fixture-wrap", "fixture-front", "fixture-back"}));
  CHECK_EQ(card_number(s, "fixture-back"), "3");
  CHECK_OK(F::redo(s));
  CHECK_EQ(card_order(s), uids({"fixture-back", "fixture-wrap", "fixture-front"}));
  CHECK_OK(F::undo(s));
  // several cards, in their order in the Set, one undo step; cards that keep their number aren't stale
  F::ActionResult two = F::moveCards(s, {"fixture-back", "fixture-wrap"}, 1);
  REQUIRE_OK(two);
  CHECK_EQ(card_order(s), uids({"fixture-front", "fixture-wrap", "fixture-back"}));
  CHECK(!contains(two.stale.card_uids, "fixture-back"));
  CHECK(contains(two.stale.card_uids, "fixture-wrap"));
  CHECK_OK(F::undo(s));
  CHECK_EQ(card_order(s), uids({"fixture-wrap", "fixture-front", "fixture-back"}));
  // to where they are: no undo step
  F::ActionResult same = F::moveCards(s, {"fixture-front", "fixture-back"}, 1);
  REQUIRE_OK(same);
  CHECK(same.stale.empty());
  CHECK(!same.can_undo);
  // links follow the cards
  CHECK_OK(F::moveCards(s, {"fixture-front"}, 2));
  CHECK_EQ(find_card(F::listCards(s), "fixture-front")->links.size(), (size_t)1);
  // errors
  CHECK_EQ(F::moveCards(s, {"fixture-wrap"}, 3).status, F::Status::InvalidArgument);
  CHECK_EQ(F::moveCards(s, {"fixture-wrap", "fixture-back"}, 2).status, F::Status::InvalidArgument);
  CHECK_EQ(F::moveCards(s, {"fixture-wrap"}, -1).status, F::Status::InvalidArgument);
  CHECK_EQ(F::moveCards(s, {}, 0).status, F::Status::InvalidArgument);
  CHECK_EQ(F::moveCards(s, {"nope"}, 0).status, F::Status::NotFound);
  F::closeSession(s);
}

/// Edit the card list, save with saveTo and check that the saved set has the same cards in the same
/// order. The test `facade-card-list-desktop` then opens the saved set in the desktop build and
/// runs the commands written here, which check the same.
TEST(card_list_edits_save_and_reopen) {
  F::SessionHandle s = open_fixture();
  F::NewCardsResult added = F::addCard(s, 1);
  REQUIRE_OK(added);
  CHECK_OK(F::setValue(s, F::Target::card(added.uids.at(0)), "name", "Added Card"));
  F::NewCardsResult copies = F::duplicateCards(s, {"fixture-front"});
  REQUIRE_OK(copies);
  CHECK_OK(F::removeCards(s, {"fixture-wrap"}));
  CHECK_OK(F::moveCards(s, {copies.uids.at(1)}, 0));
  CHECK(F::sessionInfo(s).dirty);
  fs::path out = work_dir / "card-list-edits.mse-set";
  int fd = ::open(out.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  F::ActionResult saved = F::saveTo(s, fd);
  ::close(fd);
  REQUIRE_OK(saved);
  CHECK(!saved.dirty);
  F::CardList a = F::listCards(s);
  REQUIRE_OK(a);
  CHECK_EQ(a.cards.size(), (size_t)5);
  F::OpenResult again = F::openSession(out.string(), "");
  REQUIRE_OK(again);
  F::CardList b = F::listCards(again.session);
  CHECK_EQ(card_order(s), card_order(again.session));
  for (size_t i = 0 ; i < a.cards.size() && i < b.cards.size() ; ++i) {
    CHECK_EQ(a.cards[i].name, b.cards[i].name);
    CHECK_EQ(a.cards[i].links.size(), b.cards[i].links.size());
    for (size_t j = 0 ; j < a.cards[i].links.size() && j < b.cards[i].links.size() ; ++j) {
      CHECK_EQ(a.cards[i].links[j].uid, b.cards[i].links[j].uid);
      CHECK_EQ(a.cards[i].links[j].relation, b.cards[i].links[j].relation);
    }
  }
  F::closeSession(again.session);
  // the same checks for the desktop build, as commands for `magicseteditor --cli`
  std::ofstream cli(work_dir / "card-list-edits.mse-cli", std::ios::binary);
  cli << "assert(length(set.cards) == " << a.cards.size() << ")\n";
  for (size_t i = 0 ; i < a.cards.size() ; ++i) {
    const F::CardInfo& c = a.cards[i];
    std::string card = "set.cards[" + std::to_string(i) + "]";
    cli << "assert(" << card << ".uid == \"" << c.uid << "\")\n";
    cli << "assert(" << card << ".name == \"" << c.name << "\")\n";
    cli << "assert(" << card << ".linked_card_1 == \"" << (c.links.empty() ? "" : c.links[0].uid) << "\")\n";
  }
  F::closeSession(s);
}

// ----------------------------------------------------------------------------- : Saving

TEST(save_to_fd_and_reopen) {
  F::SessionHandle s = open_fixture();
  CHECK_OK(F::setValue(s, F::Target::card("fixture-wrap"), "name", "Saved <b>Name</b>"));
  CHECK_OK(F::link(s, "fixture-wrap", "fixture-back", "Front Face", "Back Face"));
  fs::path out = work_dir / "saved.mse-set";
  int fd = ::open(out.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  REQUIRE_OK(F::saveTo(s, fd));
  ::close(fd);
  CHECK(fs::file_size(out) > 0);
  F::OpenResult again = F::openSession(out.string(), "");
  REQUIRE_OK(again);
  F::CardList a = F::listCards(s), b = F::listCards(again.session);
  CHECK_EQ(a.cards.size(), b.cards.size());
  for (size_t i = 0 ; i < a.cards.size() && i < b.cards.size() ; ++i) {
    CHECK_EQ(a.cards[i].uid, b.cards[i].uid);
    CHECK_EQ(a.cards[i].name, b.cards[i].name);
    CHECK_EQ(a.cards[i].links.size(), b.cards[i].links.size());
    for (const char* field : {"name", "rules", "frame", "level", "picture"}) {
      CHECK_EQ(F::getValue(s, F::Target::card(a.cards[i].uid), field).value,
               F::getValue(again.session, F::Target::card(b.cards[i].uid), field).value);
    }
  }
  CHECK_EQ(F::getValue(again.session, F::Target::card("fixture-wrap"), "name").value, "Saved <b>Name</b>");
  // the picture came along inside the zip
  std::vector<std::uint8_t> buf(300 * 420 * 4);
  CHECK_OK(F::renderCard(again.session, "fixture-wrap", buf.data(), 300, 420, 1200));
  CHECK_EQ(F::saveTo(s, -1).status, F::Status::InvalidArgument);
  F::closeSession(again.session);
  F::closeSession(s);
}

// ----------------------------------------------------------------------------- : Export

TEST(export_card_image) {
  F::SessionHandle s = open_fixture();
  F::BytesResult def = F::exportCardImage(s, "fixture-wrap");
  REQUIRE_OK(def);
  CHECK(def.data.size() > 8 && def.data[1] == 'P' && def.data[2] == 'N' && def.data[3] == 'G');
  Image img;
  CHECK(decode_png(def.data, img));
  CHECK(img.GetWidth() >= 300);
  // explicit settings
  F::ExportOptions o;
  o.zoom = 1.0; o.angle_degrees = 90; o.bleed = false; o.both_faces = false;
  F::BytesResult rotated = F::exportCardImage(s, "fixture-wrap", o);
  REQUIRE_OK(rotated);
  CHECK(decode_png(rotated.data, img));
  CHECK_EQ(img.GetWidth(), 420);
  CHECK_EQ(img.GetHeight(), 300);
  o.angle_degrees = 0; o.bleed = true; // 36 pixels at 300 dpi, the fixture is 150 dpi
  F::BytesResult bled = F::exportCardImage(s, "fixture-wrap", o);
  CHECK(decode_png(bled.data, img));
  CHECK_EQ(img.GetWidth(), 300 + 2 * 18);
  CHECK_EQ(img.GetHeight(), 420 + 2 * 18);
  o.bleed = false; o.zoom = 2.0;
  CHECK(decode_png(F::exportCardImage(s, "fixture-wrap", o).data, img));
  CHECK_EQ(img.GetWidth(), 600);
  // both faces of a linked pair side by side (2 pixels apart)
  o.zoom = 1.0; o.both_faces = true;
  CHECK(decode_png(F::exportCardImage(s, "fixture-back", o).data, img));
  CHECK_EQ(img.GetWidth(), 2 * 300 + 2);
  o.angle_degrees = 45;
  CHECK_EQ(F::exportCardImage(s, "fixture-back", o).status, F::Status::InvalidArgument);
  F::closeSession(s);
}

// ----------------------------------------------------------------------------- : Errors

static fs::path copy_fixture_set(const std::string& name, const std::string& from, const std::string& to) {
  fs::path dir = work_dir / name;
  fs::remove_all(dir);
  fs::copy(fixture_set(), dir, fs::copy_options::recursive);
  std::string set = read_text(dir / "set");
  size_t pos = set.find(from);
  if (pos != std::string::npos) set.replace(pos, from.size(), to);
  std::ofstream(dir / "set", std::ios::binary | std::ios::trunc) << set;
  return dir;
}

TEST(missing_template_is_a_structured_error) {
  fs::path dir = copy_fixture_set("missing-style.mse-set", "stylesheet: standard", "stylesheet: nonexistent");
  F::OpenResult r = F::openSession(dir.string(), "");
  CHECK_EQ(r.status, F::Status::MissingTemplate);
  CHECK_EQ(r.missing_package, "fixture-nonexistent.mse-style");
  CHECK_EQ(r.session, 0);
  CHECK(has_message(r, "fixture-nonexistent"));
  // the messages belonged to that call only
  F::CardList next = F::listCards(12345);
  CHECK(next.messages.empty());
  fs::path dir2 = copy_fixture_set("missing-game.mse-set", "game: fixture", "game: nonexistent");
  F::OpenResult r2 = F::openSession(dir2.string(), "");
  CHECK_EQ(r2.status, F::Status::MissingTemplate);
  CHECK_EQ(r2.missing_package, "nonexistent.mse-game");
}

TEST(messages_are_returned_per_call) {
  F::SessionHandle s = open_fixture();
  // an image that can't be read: the call fails with its own messages
  F::ActionResult a = F::setValue(s, F::Target::card("fixture-wrap"), "picture", (work_dir / "no-such-image.png").string());
  CHECK_EQ(a.status, F::Status::InvalidArgument);
  // a call that queues a message: a broken script in a set value would; here, a corrupt set
  fs::path dir = copy_fixture_set("corrupt.mse-set", "card:", "card:\n\tlevel: 3\n\tlevel\n<<<");
  F::OpenResult r = F::openSession(dir.string(), "");
  F::CardList after = F::listCards(s);
  CHECK_OK(after);
  CHECK(after.messages.empty());
  if (r.ok()) F::closeSession(r.session);
  F::closeSession(s);
}

TEST(no_exception_escapes) {
  CHECK_EQ(F::listCards(999).status, F::Status::NotFound);
  CHECK_EQ(F::undo(999).status, F::Status::NotFound);
  F::SessionHandle s = open_fixture();
  CHECK_EQ(F::getValue(s, F::Target::card("nope"), "name").status, F::Status::NotFound);
  CHECK_EQ(F::getValue(s, F::Target::card("fixture-wrap"), "nope").status, F::Status::NotFound);
  CHECK_EQ(F::undo(s).status, F::Status::InvalidArgument);
  CHECK_EQ(F::openSession((work_dir / "nothing-here.mse-set").string(), "").status, F::Status::NotFound);
  // a file that isn't a set
  fs::path junk = work_dir / "junk.mse-set";
  std::ofstream(junk, std::ios::binary) << "this is not a zip file";
  F::OpenResult j = F::openSession(junk.string(), "");
  CHECK(!j.ok());
  CHECK(!j.error.empty());
  // the working copy must not exist yet
  fs::create_directories(work_dir / "existing.mse-set");
  CHECK_EQ(F::openSession(fixture_set().string(), (work_dir / "existing.mse-set").string()).status, F::Status::InvalidArgument);
  // calls from another thread are refused
  F::Status other_thread = F::Status::Ok;
  std::thread([&] { other_thread = F::listCards(s).status; }).join();
  CHECK_EQ(other_thread, F::Status::WrongThread);
  F::closeSession(s);
}

// ----------------------------------------------------------------------------- : Installed Templates

static const F::TemplateInfo* find_template(const F::TemplateList& list, const std::string& name) {
  for (const F::TemplateInfo& t : list.templates) if (t.name == name) return &t;
  return nullptr;
}

TEST(list_templates) {
  F::TemplateList all = F::listTemplates();
  REQUIRE_OK(all);
  CHECK(all.messages.empty());
  const F::TemplateInfo* game = find_template(all, "fixture.mse-game");
  CHECK(game != nullptr);
  if (game) {
    CHECK_EQ(game->type, "game");
    CHECK_EQ(game->short_name, "Fixture");
    CHECK_EQ(game->full_name, "Test fixture game");
    CHECK_EQ(game->version, "2026-11-19");
    CHECK(game->game.empty());
  }
  const F::TemplateInfo* standard = find_template(all, "fixture-standard.mse-style");
  CHECK(standard != nullptr);
  if (standard) {
    CHECK_EQ(standard->type, "style");
    CHECK_EQ(standard->short_name, "Standard");
    CHECK_EQ(standard->game, "fixture.mse-game");
    CHECK_EQ(standard->card_width, 300.0);
    CHECK_EQ(standard->card_height, 420.0);
    CHECK_EQ(standard->card_dpi, 150.0);
    CHECK(standard->icon_path.empty());
  }
  const F::TemplateInfo* alternate = find_template(all, "fixture-alternate.mse-style");
  CHECK(alternate != nullptr);
  if (alternate) {
    CHECK_EQ(alternate->full_name, "Test fixture alternate style");
    CHECK_EQ(alternate->version, "2026-11-20");
    CHECK_EQ(alternate->game, "fixture.mse-game");
    CHECK_EQ(alternate->card_width, 280.0);
    CHECK_EQ(alternate->card_height, 400.0);
    CHECK(!alternate->icon_path.empty() && fs::exists(alternate->icon_path));
    CHECK_EQ(fs::path(alternate->icon_path).filename().string(), "icon.png");
  }
  const F::TemplateInfo* symbols = find_template(all, "fixture-symbols.mse-symbol-font");
  CHECK(symbols != nullptr && symbols->type == "symbol-font");
  // by name
  for (size_t i = 1 ; i < all.templates.size() ; ++i) CHECK(all.templates[i - 1].name < all.templates[i].name);
  // one type
  F::TemplateList styles = F::listTemplates("style");
  REQUIRE_OK(styles);
  CHECK_EQ(styles.templates.size(), (size_t)2);
  for (const F::TemplateInfo& t : styles.templates) CHECK_EQ(t.type, "style");
  // a package that can't be read is left out, with a warning
  fs::create_directories(templates_dir / "fixture-broken.mse-style");
  F::TemplateList with_broken = F::listTemplates("style");
  CHECK_OK(with_broken);
  CHECK_EQ(with_broken.templates.size(), (size_t)2);
  CHECK(has_message(with_broken, "fixture-broken.mse-style"));
  fs::remove_all(templates_dir / "fixture-broken.mse-style");
  // only on the core thread
  F::Status other_thread = F::Status::Ok;
  std::thread([&] { other_thread = F::listTemplates().status; }).join();
  CHECK_EQ(other_thread, F::Status::WrongThread);
}

// ----------------------------------------------------------------------------- : New sets

/// Creates a Set, edits it and saves it; reopens it through the facade. The test
/// `facade-new-set-desktop` then opens the saved set in the desktop build and runs the commands
/// written here, which check the same.
TEST(new_set_save_and_reopen) {
  fs::path work = work_dir / "sessions" / "new-set.mse-set";
  F::OpenResult r = F::newSession("fixture.mse-game", "fixture-standard.mse-style", work.string());
  REQUIRE_OK(r);
  F::SessionHandle s = r.session;
  CHECK(fs::exists(work / "set"));
  F::SessionInfo info = F::sessionInfo(s);
  CHECK_EQ(info.game, "fixture.mse-game");
  CHECK_EQ(info.stylesheet, "fixture-standard.mse-style");
  CHECK(!info.dirty);
  CHECK(!info.can_undo);
  // as on the desktop, a new set has one card with the Game's initial values
  F::CardList cards = F::listCards(s);
  REQUIRE_OK(cards);
  CHECK_EQ(cards.cards.size(), (size_t)1);
  if (cards.cards.size() != 1) return;
  std::string first = cards.cards[0].uid;
  CHECK(!first.empty());
  CHECK_EQ(cards.cards[0].stylesheet, "fixture-standard.mse-style");
  CHECK_EQ(F::getValue(s, F::Target::card(first), "frame").value, "grey");
  CHECK_EQ(F::getValue(s, F::Target::card(first), "level").value, "1");
  CHECK_EQ(F::getValue(s, F::Target::set(), "title").value, "");
  std::vector<std::uint8_t> buf(300 * 420 * 4);
  F::RenderResult rendered = F::renderCard(s, first, buf.data(), 300, 420, 1200);
  CHECK_OK(rendered);
  CHECK(rendered.messages.empty());
  // edit it
  CHECK_OK(F::setValue(s, F::Target::set(), "title", "New Set"));
  CHECK_OK(F::setValue(s, F::Target::card(first), "name", "First Card"));
  F::NewCardsResult added = F::addCard(s);
  REQUIRE_OK(added);
  std::string second = added.uids.at(0);
  CHECK_OK(F::setValue(s, F::Target::card(second), "name", "Second Card"));
  CHECK_OK(F::setCardStyle(s, second, "fixture-alternate.mse-style"));
  CHECK_OK(F::setValue(s, F::Target::cardStyle(second), "accent", "blue"));
  CHECK_OK(F::flush(s));
  CHECK(F::sessionInfo(s).dirty);
  // its first save: the host's "Save as"
  fs::path out = work_dir / "new-set.mse-set";
  int fd = ::open(out.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  F::ActionResult saved = F::saveTo(s, fd);
  ::close(fd);
  REQUIRE_OK(saved);
  CHECK(!saved.dirty);
  F::OpenResult again = F::openSession(out.string(), "");
  REQUIRE_OK(again);
  CHECK_EQ(F::sessionInfo(again.session).stylesheet, "fixture-standard.mse-style");
  CHECK_EQ(card_order(again.session), uids({first, second}));
  CHECK_EQ(F::getValue(again.session, F::Target::set(), "title").value, "New Set");
  CHECK_EQ(F::getValue(again.session, F::Target::card(first), "name").value, "First Card");
  F::CardList b = F::listCards(again.session);
  if (b.cards.size() == 2) {
    CHECK_EQ(b.cards[1].stylesheet, "fixture-alternate.mse-style");
    CHECK(b.cards[1].own_stylesheet);
  }
  CHECK_EQ(F::getValue(again.session, F::Target::cardStyle(second), "accent").value, "blue");
  F::closeSession(again.session);
  F::closeSession(s);
  // the same checks for the desktop build, as commands for `magicseteditor --cli`
  std::ofstream cli(work_dir / "new-set.mse-cli", std::ios::binary);
  cli << "assert(set.title == \"New Set\")\n";
  cli << "assert(length(set.cards) == 2)\n";
  cli << "assert(set.cards[0].uid == \"" << first << "\")\n";
  cli << "assert(set.cards[0].name == \"First Card\")\n";
  cli << "assert(set.cards[1].name == \"Second Card\")\n";
  cli << "assert(set.cards[1].stylesheet.short_name == \"Alternate\")\n";
  cli << "write_image_file(set.cards[1], file:\"new-set-card-2.png\")\n";
}

TEST(new_set_errors) {
  F::OpenResult no_game = F::newSession("nonexistent.mse-game", "nonexistent-standard.mse-style", "");
  CHECK_EQ(no_game.status, F::Status::MissingTemplate);
  CHECK_EQ(no_game.missing_package, "nonexistent.mse-game");
  CHECK_EQ(no_game.session, 0);
  F::OpenResult no_style = F::newSession("fixture.mse-game", "fixture-nonexistent.mse-style", "");
  CHECK_EQ(no_style.status, F::Status::MissingTemplate);
  CHECK_EQ(no_style.missing_package, "fixture-nonexistent.mse-style");
  CHECK_EQ(F::newSession("fixture-standard.mse-style", "fixture-standard.mse-style", "").status, F::Status::InvalidArgument);
  CHECK_EQ(F::newSession("fixture.mse-game", "fixture.mse-game", "").status, F::Status::InvalidArgument);
  // the working copy must not exist yet
  fs::create_directories(work_dir / "existing-new.mse-set");
  CHECK_EQ(F::newSession("fixture.mse-game", "fixture-standard.mse-style", (work_dir / "existing-new.mse-set").string()).status,
           F::Status::InvalidArgument);
  // after a failure, the same Templates still open
  F::OpenResult ok = F::newSession("fixture.mse-game", "fixture-alternate.mse-style", "");
  REQUIRE_OK(ok);
  CHECK_EQ(F::sessionInfo(ok.session).stylesheet, "fixture-alternate.mse-style");
  F::closeSession(ok.session);
}

// ----------------------------------------------------------------------------- : Styles

static F::CardInfo card_info(F::SessionHandle s, const std::string& uid) {
  F::CardList list = F::listCards(s);
  const F::CardInfo* c = find_card(list, uid);
  return c ? *c : F::CardInfo();
}

TEST(change_card_style) {
  F::SessionHandle s = open_fixture();
  F::ActionResult a = F::setCardStyle(s, "fixture-wrap", "fixture-alternate.mse-style");
  REQUIRE_OK(a);
  CHECK(!a.stale.all);
  CHECK(!a.stale.card_list);
  CHECK(contains(a.stale.card_uids, "fixture-wrap"));
  CHECK(!contains(a.stale.card_uids, "fixture-back"));
  CHECK(a.dirty);
  CHECK(a.can_undo);
  CHECK(!a.undo_name.empty());
  F::CardInfo wrap = card_info(s, "fixture-wrap");
  CHECK_EQ(wrap.stylesheet, "fixture-alternate.mse-style");
  CHECK(wrap.own_stylesheet);
  CHECK(!card_info(s, "fixture-front").own_stylesheet);
  CHECK_EQ(F::sessionInfo(s).stylesheet, "fixture-standard.mse-style");
  F::CardSize size = F::cardSize(s, "fixture-wrap");
  CHECK_EQ(size.width, 280.0);
  CHECK_EQ(size.height, 400.0);
  std::vector<std::uint8_t> buf(280 * 400 * 4);
  F::RenderResult rendered = F::renderCard(s, "fixture-wrap", buf.data(), 280, 400, 280 * 4);
  CHECK_OK(rendered);
  CHECK(rendered.messages.empty());
  // undo and redo
  F::ActionResult u = F::undo(s);
  REQUIRE_OK(u);
  CHECK(!u.stale.all);
  CHECK(contains(u.stale.card_uids, "fixture-wrap"));
  CHECK(!u.dirty);
  CHECK_EQ(card_info(s, "fixture-wrap").stylesheet, "fixture-standard.mse-style");
  CHECK(!card_info(s, "fixture-wrap").own_stylesheet);
  CHECK_EQ(F::cardSize(s, "fixture-wrap").width, 300.0);
  F::ActionResult re = F::redo(s);
  CHECK(contains(re.stale.card_uids, "fixture-wrap"));
  CHECK_EQ(card_info(s, "fixture-wrap").stylesheet, "fixture-alternate.mse-style");
  // the Set's Style (or "") makes the card follow the Set's Style again
  F::ActionResult back = F::setCardStyle(s, "fixture-wrap", "fixture-standard.mse-style");
  REQUIRE_OK(back);
  CHECK(!card_info(s, "fixture-wrap").own_stylesheet);
  CHECK_EQ(card_info(s, "fixture-wrap").stylesheet, "fixture-standard.mse-style");
  // no change: no undo step
  CHECK_OK(F::setCardStyle(s, "fixture-wrap", ""));
  CHECK_OK(F::undo(s));
  CHECK_EQ(card_info(s, "fixture-wrap").stylesheet, "fixture-alternate.mse-style");
  CHECK_OK(F::undo(s));
  CHECK(!F::sessionInfo(s).can_undo);
  // errors
  CHECK_EQ(F::setCardStyle(s, "fixture-wrap", "fixture.mse-game").status, F::Status::InvalidArgument);
  F::ActionResult missing = F::setCardStyle(s, "fixture-wrap", "fixture-nonexistent.mse-style");
  CHECK_EQ(missing.status, F::Status::MissingTemplate);
  CHECK_EQ(missing.missing_package, "fixture-nonexistent.mse-style");
  CHECK_EQ(F::setCardStyle(s, "nope", "fixture-alternate.mse-style").status, F::Status::NotFound);
  CHECK(!F::sessionInfo(s).can_undo);
  F::closeSession(s);
}

static const F::FieldInfo* find_field_info(const F::FieldList& list, const std::string& name) {
  for (const F::FieldInfo& f : list.fields) if (f.name == name) return &f;
  return nullptr;
}

TEST(set_symbol_from_a_file) {
  F::SessionHandle s = open_fixture();
  F::FieldList fields = F::listFields(s, F::Target::set());
  REQUIRE_OK(fields);
  const F::FieldInfo* symbol = find_field_info(fields, "symbol");
  REQUIRE(symbol);
  CHECK_EQ(symbol->type, std::string("symbol"));
  CHECK_EQ(F::getValue(s, F::Target::set(), "symbol").value, std::string());
  // a Template's .mse-symbol file, copied into the Set
  fs::path file = fixture_dir / "data" / "fixture-alternate.mse-style" / "triangle.mse-symbol";
  F::ActionResult a = F::setValue(s, F::Target::set(), "symbol", file.string());
  REQUIRE_OK(a);
  CHECK(a.stale.all);
  std::string stored = F::getValue(s, F::Target::set(), "symbol").value;
  CHECK(!stored.empty());
  // saved and opened again, it is still there
  fs::path out = work_dir / "set-symbol.mse-set";
  int fd = ::open(out.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  REQUIRE_OK(F::saveTo(s, fd));
  ::close(fd);
  F::OpenResult again = F::openSession(out.string(), "");
  REQUIRE_OK(again);
  CHECK_EQ(F::getValue(again.session, F::Target::set(), "symbol").value, stored);
  CHECK_OK(F::closeSession(again.session));
  // removing it is one undo step
  REQUIRE_OK(F::setValue(s, F::Target::set(), "symbol", ""));
  CHECK_EQ(F::getValue(s, F::Target::set(), "symbol").value, std::string());
  CHECK_OK(F::undo(s));
  CHECK_EQ(F::getValue(s, F::Target::set(), "symbol").value, stored);
  CHECK_OK(F::closeSession(s));
}

TEST(field_details_and_extra_card_fields) {
  F::SessionHandle s = open_fixture();
  F::FieldList fields = F::listFields(s, F::Target::card("fixture-wrap"));
  REQUIRE_OK(fields);
  const F::FieldInfo* frame = find_field_info(fields, "frame");
  const F::FieldInfo* level = find_field_info(fields, "level");
  REQUIRE(frame && level);
  CHECK_EQ(frame->type, std::string("choice"));
  CHECK(frame->choices == (std::vector<std::string>{"red", "blue", "grey"}));
  CHECK_EQ(level->type, std::string("slider"));
  CHECK(level->choices == (std::vector<std::string>{"0", "1", "2", "3", "4", "5"}));
  CHECK(find_field_info(fields, "chapters") == nullptr); // (the standard Style has no extra fields)

  // the alternate Style's extra card fields: listed, read and set like the Game's own
  REQUIRE_OK(F::setCardStyle(s, "fixture-wrap", "fixture-alternate.mse-style"));
  fields = F::listFields(s, F::Target::card("fixture-wrap"));
  const F::FieldInfo* chapters = find_field_info(fields, "chapters");
  const F::FieldInfo* border = find_field_info(fields, "border");
  REQUIRE(chapters && border);
  CHECK_EQ(chapters->type, std::string("slider"));
  CHECK(chapters->choices == (std::vector<std::string>{"1", "2", "3"}));
  CHECK_EQ(border->type, std::string("color"));
  REQUIRE(border->colors.size() == 2);
  CHECK_EQ(border->colors[1].name, std::string("white"));
  CHECK_EQ(border->colors[1].color, std::string("rgb(255,255,255)"));
  CHECK(border->allow_custom);
  // a package choice style option lists the installed packages that match
  F::FieldList options = F::listFields(s, F::Target::cardStyle("fixture-wrap"));
  REQUIRE_OK(options);
  const F::FieldInfo* symbols = find_field_info(options, "symbols");
  REQUIRE(symbols);
  CHECK_EQ(symbols->type, std::string("package choice"));
  CHECK(symbols->choices == (std::vector<std::string>{"fixture-symbols.mse-symbol-font"}));
  CHECK_EQ(F::getValue(s, F::Target::card("fixture-wrap"), "chapters").value, std::string("2"));
  F::ActionResult set = F::setValue(s, F::Target::card("fixture-wrap"), "chapters", "3");
  REQUIRE_OK(set);
  CHECK(contains(set.stale.card_uids, "fixture-wrap"));
  CHECK_EQ(F::getValue(s, F::Target::card("fixture-wrap"), "chapters").value, std::string("3"));
  REQUIRE_OK(F::setValue(s, F::Target::card("fixture-wrap"), "border", "rgb(255,255,255)"));
  CHECK_EQ(F::getValue(s, F::Target::card("fixture-wrap"), "border").value, std::string("rgb(255,255,255)"));
  CHECK_EQ(F::fieldAt(s, "fixture-wrap", 30, 380, 280).field, std::string("chapters"));
  // the color's frame is 4 pixels wide: its inside is the fields under it, or nothing
  CHECK_EQ(F::fieldAt(s, "fixture-wrap", 2, 200, 280).field, std::string("border"));
  CHECK(F::fieldAt(s, "fixture-wrap", 10, 190, 280).field != "border");
  // one undo step each
  CHECK_OK(F::undo(s));
  CHECK_EQ(F::getValue(s, F::Target::card("fixture-wrap"), "border").value, std::string("rgb(0,0,0)"));
  CHECK_OK(F::undo(s));
  CHECK_EQ(F::getValue(s, F::Target::card("fixture-wrap"), "chapters").value, std::string("2"));
  CHECK_OK(F::closeSession(s));
}

TEST(change_set_style) {
  F::SessionHandle s = open_fixture();
  F::ActionResult a = F::setSetStyle(s, "fixture-alternate.mse-style");
  REQUIRE_OK(a);
  CHECK(a.stale.all);
  CHECK(a.dirty);
  CHECK_EQ(F::sessionInfo(s).stylesheet, "fixture-alternate.mse-style");
  for (const F::CardInfo& c : F::listCards(s).cards) {
    CHECK_EQ(c.stylesheet, "fixture-alternate.mse-style");
    CHECK(!c.own_stylesheet);
    CHECK_EQ(F::cardSize(s, c.uid).width, 280.0);
  }
  // the Set's style options are those of its Style
  F::FieldList options = F::listFields(s, F::Target::setStyle());
  REQUIRE_OK(options);
  CHECK_EQ(options.fields.size(), (size_t)3); // accent, footer, symbols
  CHECK_EQ(F::getValue(s, F::Target::setStyle(), "accent").value, "red");
  // undo and redo
  F::ActionResult u = F::undo(s);
  CHECK(u.stale.all);
  CHECK(!u.dirty);
  CHECK_EQ(F::sessionInfo(s).stylesheet, "fixture-standard.mse-style");
  CHECK_EQ(F::cardSize(s, "fixture-back").width, 300.0);
  CHECK_EQ(F::listFields(s, F::Target::setStyle()).fields.size(), (size_t)0);
  CHECK(F::redo(s).stale.all);
  CHECK_EQ(F::sessionInfo(s).stylesheet, "fixture-alternate.mse-style");
  // a card with a Style of its own keeps it
  CHECK_OK(F::setCardStyle(s, "fixture-front", "fixture-standard.mse-style"));
  CHECK_OK(F::setSetStyle(s, "fixture-standard.mse-style"));
  CHECK_EQ(card_info(s, "fixture-wrap").stylesheet, "fixture-standard.mse-style");
  CHECK_EQ(card_info(s, "fixture-front").stylesheet, "fixture-standard.mse-style");
  CHECK_OK(F::undo(s));
  CHECK_EQ(card_info(s, "fixture-wrap").stylesheet, "fixture-alternate.mse-style");
  CHECK_EQ(card_info(s, "fixture-front").stylesheet, "fixture-standard.mse-style");
  // no change: no undo step
  F::ActionResult same = F::setSetStyle(s, "fixture-alternate.mse-style");
  CHECK_OK(same);
  CHECK(same.stale.empty());
  CHECK_OK(F::undo(s)); // the card's Style
  CHECK_OK(F::undo(s)); // the Set's Style
  CHECK(!F::sessionInfo(s).can_undo);
  // errors
  CHECK_EQ(F::setSetStyle(s, "").status, F::Status::InvalidArgument);
  CHECK_EQ(F::setSetStyle(s, "fixture-nonexistent.mse-style").status, F::Status::MissingTemplate);
  F::closeSession(s);
}

TEST(own_styling_toggle) {
  F::SessionHandle s = open_fixture();
  REQUIRE_OK(F::setSetStyle(s, "fixture-alternate.mse-style"));
  REQUIRE_OK(F::setValue(s, F::Target::setStyle(), "accent", "blue"));
  // without options of its own, a card uses the Set's
  CHECK(!card_info(s, "fixture-wrap").own_styling);
  CHECK_EQ(F::getValue(s, F::Target::cardStyle("fixture-wrap"), "accent").value, "blue");
  // switched on: a copy of the Set's
  F::ActionResult on = F::setCardOwnStyling(s, "fixture-wrap", true);
  REQUIRE_OK(on);
  CHECK(!on.stale.all);
  CHECK(contains(on.stale.card_uids, "fixture-wrap"));
  CHECK(!contains(on.stale.card_uids, "fixture-front"));
  CHECK(card_info(s, "fixture-wrap").own_styling);
  CHECK(!card_info(s, "fixture-front").own_styling);
  CHECK_EQ(F::getValue(s, F::Target::cardStyle("fixture-wrap"), "accent").value, "blue");
  // its own options change without the Set's
  CHECK_OK(F::setValue(s, F::Target::cardStyle("fixture-wrap"), "accent", "red"));
  CHECK_OK(F::setValue(s, F::Target::cardStyle("fixture-wrap"), "footer", "Mine"));
  CHECK_EQ(F::getValue(s, F::Target::cardStyle("fixture-wrap"), "accent").value, "red");
  CHECK_EQ(F::getValue(s, F::Target::cardStyle("fixture-wrap"), "footer").value, "Mine");
  CHECK_EQ(F::getValue(s, F::Target::setStyle(), "accent").value, "blue");
  CHECK_EQ(F::getValue(s, F::Target::setStyle(), "footer").value, "");
  CHECK_EQ(F::getValue(s, F::Target::cardStyle("fixture-front"), "accent").value, "blue");
  std::vector<std::uint8_t> buf(280 * 400 * 4);
  F::RenderResult rendered = F::renderCard(s, "fixture-wrap", buf.data(), 280, 400, 280 * 4);
  CHECK_OK(rendered);
  CHECK(rendered.messages.empty());
  // already on: no undo step
  F::ActionResult again = F::setCardOwnStyling(s, "fixture-wrap", true);
  CHECK_OK(again);
  CHECK(again.stale.empty());
  // switched off: the Set's again
  F::ActionResult off = F::setCardOwnStyling(s, "fixture-wrap", false);
  REQUIRE_OK(off);
  CHECK(contains(off.stale.card_uids, "fixture-wrap"));
  CHECK(!card_info(s, "fixture-wrap").own_styling);
  CHECK_EQ(F::getValue(s, F::Target::cardStyle("fixture-wrap"), "accent").value, "blue");
  CHECK_EQ(F::getValue(s, F::Target::cardStyle("fixture-wrap"), "footer").value, "");
  // undo brings back the card's own options, redo drops them again
  F::ActionResult u = F::undo(s);
  CHECK(contains(u.stale.card_uids, "fixture-wrap"));
  CHECK(card_info(s, "fixture-wrap").own_styling);
  CHECK_EQ(F::getValue(s, F::Target::cardStyle("fixture-wrap"), "accent").value, "red");
  CHECK_OK(F::redo(s));
  CHECK_EQ(F::getValue(s, F::Target::cardStyle("fixture-wrap"), "accent").value, "blue");
  CHECK_OK(F::undo(s));
  CHECK_OK(F::undo(s)); // footer
  CHECK_OK(F::undo(s)); // accent
  CHECK_EQ(F::getValue(s, F::Target::cardStyle("fixture-wrap"), "accent").value, "blue");
  CHECK_OK(F::undo(s)); // switched on
  CHECK(!card_info(s, "fixture-wrap").own_styling);
  // a card's own Style starts without options of its own
  CHECK_OK(F::setCardOwnStyling(s, "fixture-wrap", true));
  CHECK_OK(F::setCardStyle(s, "fixture-wrap", "fixture-standard.mse-style"));
  CHECK(!card_info(s, "fixture-wrap").own_styling);
  CHECK_OK(F::undo(s));
  CHECK(card_info(s, "fixture-wrap").own_styling);
  // changing the Set's Style drops the options of cards that follow it, as on the desktop
  CHECK_OK(F::setSetStyle(s, "fixture-standard.mse-style"));
  CHECK(!card_info(s, "fixture-wrap").own_styling);
  CHECK_OK(F::undo(s));
  CHECK(card_info(s, "fixture-wrap").own_styling);
  CHECK_EQ(F::setCardOwnStyling(s, "nope", true).status, F::Status::NotFound);
  F::closeSession(s);
}

// ----------------------------------------------------------------------------- : Template import

static void write_zip_entry(ZipWriter& zip, const std::string& name, const std::string& data) {
  unique_ptr<OutputStream> out = zip.openEntry(decode_utf8(name));
  out->write(data);
  out->close();
}

static fs::path make_pack(const std::string& name, const std::string& version, const std::string& mse_version) {
  fs::path zip_path = work_dir / name;
  ZipWriter zip(decode_utf8(zip_path.string()));
  fs::path src = fixture_dir / "data" / "fixture-symbols.mse-symbol-font";
  std::string font = read_text(src / "symbol-font");
  font.replace(font.find("version: 2026-11-19"), 19, "version: " + version);
  font.replace(font.find("mse version: 2.6.0"), 18, "mse version: " + mse_version);
  write_zip_entry(zip, "My Pack-main/data/imported-symbols.mse-symbol-font/symbol-font", font);
  write_zip_entry(zip, "My Pack-main/data/imported-symbols.mse-symbol-font/red.png", read_text(src / "red.png"));
  write_zip_entry(zip, "My Pack-main/data/imported-symbols.mse-symbol-font/blue.png", read_text(src / "blue.png"));
  write_zip_entry(zip, "My Pack-main/Magic - Fonts/ImportedFont.ttf", read_text(fixture_dir / "fonts" / "DejaVuSans.ttf"));
  write_zip_entry(zip, "My Pack-main/README.md", "ignored");
  zip.close();
  return zip_path;
}

/// importTemplates runs off the core thread
static F::ImportResult import_on_thread(const fs::path& zip) {
  F::ImportResult r;
  std::thread([&] { r = F::importTemplates(zip.string()); }).join();
  return r;
}

TEST(template_import) {
  F::ImportResult r = import_on_thread(make_pack("pack1.zip", "2026-11-19", "2.6.0"));
  REQUIRE_OK(r);
  CHECK_EQ(r.packages.size(), (size_t)1);
  if (r.packages.size() == 1) {
    CHECK_EQ(r.packages[0].name, "imported-symbols.mse-symbol-font");
    CHECK_EQ(r.packages[0].version, "2026-11-19");
    CHECK(r.packages[0].outcome == F::ImportedPackage::Outcome::Installed);
  }
  CHECK(fs::exists(templates_dir / "imported-symbols.mse-symbol-font" / "red.png"));
  CHECK_EQ(r.fonts.size(), (size_t)1);
  CHECK(fs::exists(font_store_dir / "ImportedFont.ttf"));
  // the same version again: skipped
  r = import_on_thread(make_pack("pack2.zip", "2026-11-19", "2.6.0"));
  CHECK(r.packages.size() == 1 && r.packages[0].outcome == F::ImportedPackage::Outcome::SkippedNotNewer);
  CHECK(r.fonts.empty());
  // newer: replaced
  r = import_on_thread(make_pack("pack3.zip", "2026-12-01", "2.6.0"));
  CHECK(r.packages.size() == 1 && r.packages[0].outcome == F::ImportedPackage::Outcome::Replaced);
  CHECK(read_text(templates_dir / "imported-symbols.mse-symbol-font" / "symbol-font").find("2026-12-01") != std::string::npos);
  // for a newer program: refused
  r = import_on_thread(make_pack("pack4.zip", "2027-01-01", "99.0.0"));
  CHECK(r.packages.size() == 1 && r.packages[0].outcome == F::ImportedPackage::Outcome::Refused);
  // no temporary directories left behind
  for (auto& e : fs::directory_iterator(templates_dir)) CHECK(e.path().filename().string()[0] != '.');
  CHECK_OK(F::refreshTemplates());
  // not a zip
  CHECK_EQ(import_on_thread(work_dir / "junk.mse-set").status, F::Status::InvalidArgument);
}

// Synthetic Templates made from the fixture's symbol font, with their own names, versions and
// dependencies

static std::string symbol_font_data(const std::string& version, const std::string& extra = "", const std::string& mse_version = "2.6.0") {
  std::string font = read_text(fixture_dir / "data" / "fixture-symbols.mse-symbol-font" / "symbol-font");
  font.replace(font.find("version: 2026-11-19"), 19, "version: " + version);
  font.replace(font.find("mse version: 2.6.0"), 18, "mse version: " + mse_version);
  return font + extra;
}

static std::string depends_on(const std::string& package, const std::string& version) {
  return "depends on:\n\tpackage: " + package + "\n\tversion: " + version + "\n";
}

/// The entries of a zip to write
struct ZipBuilder {
  std::vector<std::pair<std::string, std::string>> entries;

  void add(const std::string& name, const std::string& data) { entries.emplace_back(name, data); }
  /// A symbol font package directory at prefix (which ends in '/' or is empty)
  void package(const std::string& prefix, const std::string& name, const std::string& data_file) {
    fs::path src = fixture_dir / "data" / "fixture-symbols.mse-symbol-font";
    add(prefix + name + "/symbol-font", data_file);
    add(prefix + name + "/red.png", read_text(src / "red.png"));
    add(prefix + name + "/blue.png", read_text(src / "blue.png"));
  }
  fs::path write(const std::string& name) const {
    fs::path path = work_dir / "zips" / name;
    fs::create_directories(path.parent_path());
    ZipWriter zip(decode_utf8(path.string()));
    for (auto& e : entries) write_zip_entry(zip, e.first, e.second);
    zip.close();
    return path;
  }
  std::string bytes(const std::string& name) const { return read_text(write(name)); }
};

/// The size of a symbol font package with this data file, when extracted
static std::uint64_t symbol_font_size(const std::string& data_file) {
  fs::path src = fixture_dir / "data" / "fixture-symbols.mse-symbol-font";
  return data_file.size() + fs::file_size(src / "red.png") + fs::file_size(src / "blue.png");
}

static const F::ScannedPackage* find_scanned(const F::ImportScan& scan, const std::string& name) {
  for (const F::ScannedPackage& p : scan.packages) if (p.name == name) return &p;
  return nullptr;
}

static const F::ImportedPackage* find_imported(const F::ImportResult& r, const std::string& name) {
  for (const F::ImportedPackage& p : r.packages) if (p.name == name) return &p;
  return nullptr;
}

/// No temporary files left in the template store or the font store
static bool no_temp_files() {
  for (const fs::path& dir : {templates_dir, font_store_dir}) {
    if (!fs::exists(dir)) continue;
    for (auto& e : fs::directory_iterator(dir)) {
      if (e.path().filename().string()[0] == '.') return false;
    }
  }
  return true;
}

static std::string installed_version(const std::string& name) {
  std::string data = read_text(templates_dir / name / "symbol-font");
  size_t pos = data.find("\nversion: ");
  return pos == std::string::npos ? "" : data.substr(pos + 10, data.find('\n', pos + 1) - pos - 10);
}

/// The zip of the scan and install tests: packages at depth, in an installer, zipped, with
/// dependencies, refused ones, a duplicate, loose fonts and things to ignore
static fs::path scan_test_zip() {
  ZipBuilder installer;
  installer.add("installer", "mse version: 2.6.0\n");
  installer.package("", "t1-f.mse-symbol-font", symbol_font_data("2026-02-01"));
  ZipBuilder zipped; // a package as a zip file: its files at the root
  fs::path src = fixture_dir / "data" / "fixture-symbols.mse-symbol-font";
  zipped.add("symbol-font", symbol_font_data("2026-01-01"));
  zipped.add("red.png", read_text(src / "red.png"));
  zipped.add("blue.png", read_text(src / "blue.png"));
  ZipBuilder z;
  z.package("Pack-main/data/deep/er/", "t1-a.mse-symbol-font", symbol_font_data("2026-01-01"));
  z.package("Pack-main/data/", "t1-b.mse-symbol-font", symbol_font_data("2026-01-01",
    "installer group: tests/b\n" + depends_on("t1-a.mse-symbol-font", "2026-01-01") + depends_on("Magic Set Editor", "2.0.0")));
  z.package("Pack-main/data/", "t1-c.mse-symbol-font", symbol_font_data("2026-01-01", depends_on("missing.mse-game", "2026-01-01")));
  z.package("Pack-main/data/", "t1-d.mse-symbol-font", symbol_font_data("2026-01-01", "", "99.0.0"));
  z.package("Pack-main/data/", "t1-e.mse-symbol-font", symbol_font_data("2026-01-01", depends_on("t1-d.mse-symbol-font", "2026-01-01")));
  z.package("Pack-main/data/", "t1-f.mse-symbol-font", symbol_font_data("2026-01-01")); // older than the one in the installer
  z.package("Pack-main/data/", "fixture-symbols.mse-symbol-font", symbol_font_data("2026-11-19")); // as installed
  z.add("Pack-main/installers/t1.mse-installer", installer.bytes("t1.mse-installer"));
  z.add("Pack-main/zipped/t1-g.mse-symbol-font", zipped.bytes("t1-g.zip"));
  z.add("Pack-main/Magic - Fonts/sub dir/LooseA.ttf", read_text(fixture_dir / "fonts" / "DejaVuSans.ttf"));
  z.add("Pack-main/Magic - Fonts/LooseB.OTF", read_text(fixture_dir / "fonts" / "DejaVuSans-Bold.ttf"));
  z.add("Pack-main/sets/sample.mse-set/set", "ignored");
  z.add("Pack-main/sets/sample.mse-set/font.ttf", "ignored: inside a Set");
  z.add("Pack-main/data/t1-a.mse-symbol-font-notes.txt", "ignored");
  z.add("Pack-main/README.md", "ignored");
  z.add("Pack-main/data/dictionaries/xx_TEST.dic", "1\nword\n");
  z.add("Pack-main/data/dictionaries/xx_TEST.aff", "SET UTF-8\n");
  z.add("Pack-main/data/other/yy_TEST.dic", "ignored: not in a dictionaries directory");
  return z.write("scan-test.zip");
}

TEST(template_import_scan) {
  using V = F::ScannedPackage::Verdict;
  fs::path zip = scan_test_zip();
  int fd = ::open(zip.c_str(), O_RDONLY);
  CHECK(fd >= 0);
  F::ImportScan scan;
  std::thread([&] { scan = F::scanTemplates(fd); }).join(); // off the core thread
  REQUIRE_OK(scan);
  CHECK_EQ(lseek(fd, 0, SEEK_CUR), (off_t)0); // the descriptor's position is not used
  ::close(fd);
  CHECK_EQ(scan.packages.size(), (size_t)8);
  // at depth
  const F::ScannedPackage* a = find_scanned(scan, "t1-a.mse-symbol-font");
  CHECK(a != nullptr);
  if (a) {
    CHECK_EQ(a->type, "symbol-font");
    CHECK_EQ(a->version, "2026-01-01");
    CHECK_EQ(a->full_name, "Fixture symbols");
    CHECK_EQ(a->installer_group, "unclassified/t1/Fixture symbols"); // as MSE's installer derives it
    CHECK_EQ(a->size, symbol_font_size(symbol_font_data("2026-01-01")));
    CHECK(a->verdict == V::Install);
    CHECK(a->installed_version.empty());
  }
  const F::ScannedPackage* b = find_scanned(scan, "t1-b.mse-symbol-font");
  CHECK(b != nullptr);
  if (b) {
    CHECK_EQ(b->installer_group, "tests/b");
    CHECK_EQ(b->dependencies.size(), (size_t)2);
    if (b->dependencies.size() == 2) {
      CHECK_EQ(b->dependencies[0].package, "t1-a.mse-symbol-font");
      CHECK_EQ(b->dependencies[0].version, "2026-01-01");
      CHECK_EQ(b->dependencies[1].package, "Magic Set Editor"); // the program itself
    }
    CHECK(b->verdict == V::Install);
  }
  // refused: missing dependency, too new, depends on a refused one
  const F::ScannedPackage* c = find_scanned(scan, "t1-c.mse-symbol-font");
  CHECK(c && c->verdict == V::Refuse && c->reason.find("missing.mse-game") != std::string::npos);
  const F::ScannedPackage* d = find_scanned(scan, "t1-d.mse-symbol-font");
  CHECK(d && d->verdict == V::Refuse && d->reason.find("mse version 99.0.0") != std::string::npos);
  const F::ScannedPackage* e = find_scanned(scan, "t1-e.mse-symbol-font");
  CHECK(e && e->verdict == V::Refuse && e->reason.find("t1-d.mse-symbol-font") != std::string::npos);
  // in a nested installer, and newer than the copy elsewhere in the zip: one version per package
  const F::ScannedPackage* f = find_scanned(scan, "t1-f.mse-symbol-font");
  CHECK(f && f->version == "2026-02-01" && f->verdict == V::Install);
  // a zipped package
  const F::ScannedPackage* g = find_scanned(scan, "t1-g.mse-symbol-font");
  CHECK(g && g->verdict == V::Install && g->size == symbol_font_size(symbol_font_data("2026-01-01")));
  // already installed
  const F::ScannedPackage* same = find_scanned(scan, "fixture-symbols.mse-symbol-font");
  CHECK(same && same->verdict == V::SkipNotNewer && same->installed_version == "2026-11-19");
  // loose fonts, not the one in a Set
  CHECK_EQ(scan.fonts.size(), (size_t)2);
  CHECK(contains(scan.fonts, "LooseA.ttf") && contains(scan.fonts, "LooseB.OTF"));
  CHECK(scan.fonts_size > 0);
  // spelling dictionaries
  CHECK_EQ(scan.dictionaries.size(), (size_t)2);
  // scanning installs nothing
  CHECK(!fs::exists(templates_dir / "t1-a.mse-symbol-font"));
  CHECK(!fs::exists(font_store_dir / "LooseA.ttf"));
  CHECK(no_temp_files());
  // the path variant, and things that aren't zips
  CHECK_EQ(F::scanTemplates(zip.string()).packages.size(), (size_t)8);
  CHECK_EQ(F::scanTemplates(-1).status, F::Status::InvalidArgument);
  CHECK_EQ(F::scanTemplates((work_dir / "junk.mse-set").string()).status, F::Status::InvalidArgument);
}

TEST(template_import_install) {
  using O = F::ImportedPackage::Outcome;
  fs::path zip = scan_test_zip();
  int fd = ::open(zip.c_str(), O_RDONLY);
  CHECK(fd >= 0);
  // a dependency in the zip is selected with the package that needs it, and installed first
  F::ImportSelection sel;
  sel.packages = {"t1-b.mse-symbol-font"};
  F::ImportResult r;
  std::thread([&] { r = F::installTemplates(fd, sel); }).join();
  REQUIRE_OK(r);
  CHECK_EQ(r.packages.size(), (size_t)2);
  if (r.packages.size() == 2) {
    CHECK_EQ(r.packages[0].name, "t1-a.mse-symbol-font");
    CHECK(r.packages[0].outcome == O::Installed && r.packages[0].dependency);
    CHECK_EQ(r.packages[1].name, "t1-b.mse-symbol-font");
    CHECK(r.packages[1].outcome == O::Installed && !r.packages[1].dependency);
  }
  CHECK(fs::exists(templates_dir / "t1-a.mse-symbol-font" / "red.png"));
  CHECK(fs::exists(templates_dir / "t1-b.mse-symbol-font" / "symbol-font"));
  CHECK(!fs::exists(templates_dir / "t1-f.mse-symbol-font"));
  // the loose fonts, from any depth
  CHECK_EQ(r.fonts.size(), (size_t)2);
  CHECK(fs::exists(font_store_dir / "LooseA.ttf"));
  CHECK(fs::exists(font_store_dir / "LooseB.OTF"));
  CHECK_EQ(r.dictionaries.size(), (size_t)2);
  CHECK_EQ(read_text(templates_dir / "dictionaries" / "xx_TEST.dic"), "1\nword\n");
  CHECK(!fs::exists(templates_dir / "dictionaries" / "yy_TEST.dic"));
  // the rest
  sel.packages = {"t1-c.mse-symbol-font", "t1-e.mse-symbol-font", "t1-f.mse-symbol-font", "t1-g.mse-symbol-font",
                  "fixture-symbols.mse-symbol-font", "nope.mse-style", "t1-b.mse-symbol-font"};
  sel.fonts = false;
  r = F::installTemplates(fd, sel);
  CHECK(r.dictionaries.empty()); // already there
  ::close(fd);
  REQUIRE_OK(r);
  CHECK_EQ(r.packages.size(), (size_t)7);
  const F::ImportedPackage* p;
  CHECK((p = find_imported(r, "t1-c.mse-symbol-font")) && p->outcome == O::Refused && !p->error.empty());
  CHECK((p = find_imported(r, "t1-e.mse-symbol-font")) && p->outcome == O::Refused);
  CHECK((p = find_imported(r, "t1-f.mse-symbol-font")) && p->outcome == O::Installed && p->version == "2026-02-01");
  CHECK((p = find_imported(r, "t1-g.mse-symbol-font")) && p->outcome == O::Installed);
  CHECK((p = find_imported(r, "fixture-symbols.mse-symbol-font")) && p->outcome == O::SkippedNotNewer);
  CHECK((p = find_imported(r, "nope.mse-style")) && p->outcome == O::Failed);
  CHECK((p = find_imported(r, "t1-b.mse-symbol-font")) && p->outcome == O::SkippedNotNewer);
  CHECK(!fs::exists(templates_dir / "t1-c.mse-symbol-font"));
  CHECK(!fs::exists(templates_dir / "t1-d.mse-symbol-font"));
  CHECK_EQ(installed_version("t1-f.mse-symbol-font"), "2026-02-01");
  CHECK(fs::exists(templates_dir / "t1-g.mse-symbol-font" / "blue.png"));
  CHECK(r.fonts.empty());
  CHECK(no_temp_files());
  // the installed Templates load
  CHECK_OK(F::refreshTemplates());
}

TEST(working_copy_reopened) {
  // a session's working copy, autosaved, then reopened as a later process would
  fs::path work = work_dir / "sessions" / "reopened.mse-set";
  F::OpenResult open = F::openSession(fixture_set().string(), work.string());
  REQUIRE_OK(open);
  F::ActionResult a = F::setValue(open.session, F::Target::set(), "title", "Changed before the kill");
  REQUIRE_OK(a);
  CHECK(a.dirty);
  REQUIRE_OK(F::flush(open.session));
  REQUIRE_OK(F::closeSession(open.session));
  F::OpenResult again = F::openWorkingCopy(work.string(), true);
  REQUIRE_OK(again);
  F::ValueResult title = F::getValue(again.session, F::Target::set(), "title");
  CHECK_EQ(title.value, "Changed before the kill");
  F::SessionInfo info = F::sessionInfo(again.session);
  CHECK(info.dirty);
  // edits keep going to the same working copy
  REQUIRE_OK(F::setValue(again.session, F::Target::set(), "title", "And after"));
  REQUIRE_OK(F::flush(again.session));
  CHECK(read_text(work / "set").find("And after") != std::string::npos);
  // saved: clean
  fs::path saved = work_dir / "reopened-saved.mse-set";
  int fd = ::open(saved.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  F::ActionResult s = F::saveTo(again.session, fd);
  ::close(fd);
  REQUIRE_OK(s);
  CHECK(!s.dirty);
  REQUIRE_OK(F::closeSession(again.session));
  // reopened as saved: clean; a missing one is NotFound
  F::OpenResult clean = F::openWorkingCopy(work.string(), false);
  REQUIRE_OK(clean);
  CHECK(!F::sessionInfo(clean.session).dirty);
  F::closeSession(clean.session);
  CHECK(F::openWorkingCopy((work_dir / "nope").string(), true).status == F::Status::NotFound);
}

TEST(template_import_progress_and_cancel) {
  using O = F::ImportedPackage::Outcome;
  ZipBuilder z;
  z.package("", "progress-a.mse-symbol-font", symbol_font_data("2026-01-01"));
  z.package("", "progress-b.mse-symbol-font", symbol_font_data("2026-01-01"));
  z.add("fonts/LooseC.ttf", read_text(fixture_dir / "fonts" / "DejaVuSans-Oblique.ttf"));
  fs::path zip = z.write("progress-test.zip");
  F::ImportSelection sel;
  sel.packages = {"progress-a.mse-symbol-font", "progress-b.mse-symbol-font"};
  // cancelled as soon as the first file is extracted: nothing is installed, nothing half-installed
  F::ImportResult r = F::installTemplates(zip.string(), sel, [](std::uint64_t done, std::uint64_t) { return done == 0; });
  REQUIRE_OK(r);
  CHECK(r.cancelled);
  CHECK_EQ(r.packages.size(), (size_t)2);
  for (const F::ImportedPackage& p : r.packages) CHECK(p.outcome == O::Cancelled);
  CHECK(r.fonts.empty());
  CHECK(!fs::exists(templates_dir / "progress-a.mse-symbol-font"));
  CHECK(!fs::exists(templates_dir / "progress-b.mse-symbol-font"));
  CHECK(!fs::exists(font_store_dir / "LooseC.ttf"));
  // progress: from 0 to the total, never backwards
  std::vector<std::pair<std::uint64_t, std::uint64_t>> calls;
  r = F::installTemplates(zip.string(), sel, [&](std::uint64_t done, std::uint64_t total) {
    calls.emplace_back(done, total);
    return true;
  });
  REQUIRE_OK(r);
  CHECK(!r.cancelled);
  CHECK_EQ(r.packages.size(), (size_t)2);
  for (const F::ImportedPackage& p : r.packages) CHECK(p.outcome == O::Installed);
  CHECK(fs::exists(font_store_dir / "LooseC.ttf"));
  F::ImportScan again = F::scanTemplates(zip.string());
  CHECK_EQ(again.fonts.size(), (size_t)1);
  CHECK(again.new_fonts.empty() && again.new_fonts_size == 0);
  CHECK(calls.size() > 2);
  if (!calls.empty()) {
    CHECK_EQ(calls.front().first, (std::uint64_t)0);
    CHECK(calls.back().first == calls.back().second && calls.back().second > 0);
    for (size_t i = 1 ; i < calls.size() ; ++i) CHECK(calls[i].first >= calls[i-1].first && calls[i].second == calls[0].second);
  }
}

TEST(template_import_installer_file) {
  // a standalone .mse-installer as the input
  ZipBuilder z;
  z.add("installer", "mse version: 2.6.0\n");
  z.package("", "t2-a.mse-symbol-font", symbol_font_data("2026-03-01"));
  fs::path path = z.write("t2.mse-installer");
  F::ImportScan scan = F::scanTemplates(path.string());
  REQUIRE_OK(scan);
  CHECK(scan.packages.size() == 1 && scan.packages[0].name == "t2-a.mse-symbol-font");
  F::ImportSelection sel;
  sel.packages = {"t2-a.mse-symbol-font"};
  F::ImportResult r = F::installTemplates(path.string(), sel);
  REQUIRE_OK(r);
  CHECK(r.packages.size() == 1 && r.packages[0].outcome == F::ImportedPackage::Outcome::Installed);
  CHECK(fs::exists(templates_dir / "t2-a.mse-symbol-font" / "red.png"));
}

TEST(template_import_dependency_versions) {
  using O = F::ImportedPackage::Outcome;
  using V = F::ScannedPackage::Verdict;
  // the installed t1-a (2026-01-01) is enough
  ZipBuilder z1;
  z1.package("", "t3-b.mse-symbol-font", symbol_font_data("2026-01-01", depends_on("t1-a.mse-symbol-font", "2025-06-01")));
  F::ImportScan scan = F::scanTemplates(z1.write("t3-1.zip").string());
  CHECK(scan.packages.size() == 1 && scan.packages[0].verdict == V::Install);
  // the installed t1-a is too old, and there is no newer one in the zip
  ZipBuilder z2;
  z2.package("", "t3-c.mse-symbol-font", symbol_font_data("2026-01-01", depends_on("t1-a.mse-symbol-font", "2027-01-01")));
  scan = F::scanTemplates(z2.write("t3-2.zip").string());
  CHECK(scan.packages.size() == 1 && scan.packages[0].verdict == V::Refuse);
  // ... there is: it is selected too, and replaces the installed one
  z2.package("", "t1-a.mse-symbol-font", symbol_font_data("2027-01-01"));
  fs::path zip = z2.write("t3-3.zip");
  scan = F::scanTemplates(zip.string());
  const F::ScannedPackage* a = find_scanned(scan, "t1-a.mse-symbol-font");
  CHECK(a && a->verdict == V::Replace && a->installed_version == "2026-01-01");
  CHECK(find_scanned(scan, "t3-c.mse-symbol-font") && find_scanned(scan, "t3-c.mse-symbol-font")->verdict == V::Install);
  F::ImportSelection sel;
  sel.packages = {"t3-c.mse-symbol-font"};
  F::ImportResult r = F::installTemplates(zip.string(), sel);
  REQUIRE_OK(r);
  CHECK(r.packages.size() == 2 && r.packages[0].name == "t1-a.mse-symbol-font" && r.packages[0].outcome == O::Replaced && r.packages[0].dependency);
  CHECK_EQ(installed_version("t1-a.mse-symbol-font"), "2027-01-01");
  // no downgrade
  ZipBuilder z3;
  z3.package("", "t1-a.mse-symbol-font", symbol_font_data("2026-01-01"));
  r = F::importTemplates(z3.write("t3-4.zip").string());
  CHECK(r.packages.size() == 1 && r.packages[0].outcome == O::SkippedNotNewer);
  CHECK_EQ(installed_version("t1-a.mse-symbol-font"), "2027-01-01");
}

TEST(template_import_failure) {
  using O = F::ImportedPackage::Outcome;
  // t4-a is new, t4-b needs it, t1-a replaces the installed one; the 2nd file written fails
  ZipBuilder z;
  z.package("", "t1-a.mse-symbol-font", symbol_font_data("2028-01-01"));
  z.package("", "t4-a.mse-symbol-font", symbol_font_data("2026-01-01"));
  z.package("", "t4-b.mse-symbol-font", symbol_font_data("2026-01-01", depends_on("t4-a.mse-symbol-font", "2026-01-01")));
  z.add("fonts/Failing.ttf", read_text(fixture_dir / "fonts" / "DejaVuSans.ttf"));
  fs::path zip = z.write("t4.zip");
  std::map<std::string, int> writes;
  F::import_write_hook = [&](const std::string& path) {
    // the second file of t1-a and of t4-a, and every font
    for (const char* failing : {"t1-a.mse-symbol-font", "t4-a.mse-symbol-font"}) {
      if (path.find(failing) != std::string::npos && ++writes[failing] == 2) return false;
    }
    return path.find("Failing") == std::string::npos;
  };
  F::ImportResult r = F::importTemplates(zip.string());
  F::import_write_hook = nullptr;
  REQUIRE_OK(r);
  CHECK_EQ(r.packages.size(), (size_t)3);
  for (const char* name : {"t1-a.mse-symbol-font", "t4-a.mse-symbol-font"}) {
    const F::ImportedPackage* p = find_imported(r, name);
    CHECK(p && p->outcome == O::Failed && p->error.find("could not extract") != std::string::npos);
  }
  const F::ImportedPackage* needs = find_imported(r, "t4-b.mse-symbol-font");
  CHECK(needs && needs->outcome == O::Failed && needs->error.find("t4-a.mse-symbol-font") != std::string::npos);
  CHECK(no_temp_files());
  CHECK(r.fonts.empty() && !r.messages.empty());
  CHECK(!fs::exists(font_store_dir / "Failing.ttf"));
  // the replaced package is still the old version, whole
  CHECK_EQ(installed_version("t1-a.mse-symbol-font"), "2027-01-01");
  CHECK(fs::exists(templates_dir / "t1-a.mse-symbol-font" / "red.png"));
  CHECK(fs::exists(templates_dir / "t1-a.mse-symbol-font" / "blue.png"));
  // the new one is not there at all, nor what needs it
  CHECK(!fs::exists(templates_dir / "t4-a.mse-symbol-font"));
  CHECK(!fs::exists(templates_dir / "t4-b.mse-symbol-font"));
  // a read-only store
  fs::permissions(templates_dir, fs::perms::owner_write, fs::perm_options::remove);
  r = F::importTemplates(zip.string());
  fs::permissions(templates_dir, fs::perms::owner_write, fs::perm_options::add);
  CHECK(!r.ok() || (find_imported(r, "t4-a.mse-symbol-font") && find_imported(r, "t4-a.mse-symbol-font")->outcome == O::Failed));
  CHECK(!fs::exists(templates_dir / "t4-a.mse-symbol-font"));
  CHECK(no_temp_files());
  // and then it works
  r = F::importTemplates(zip.string());
  REQUIRE_OK(r);
  CHECK(find_imported(r, "t4-b.mse-symbol-font") && find_imported(r, "t4-b.mse-symbol-font")->outcome == O::Installed);
  CHECK_EQ(installed_version("t1-a.mse-symbol-font"), "2028-01-01");
  CHECK(fs::exists(font_store_dir / "Failing.ttf"));
  CHECK(no_temp_files());
}

// ----------------------------------------------------------------------------- : Main

/// facade_tests --import <zip> <template store> <font store>
static int import_main(int argc, char** argv) {
  fs::create_directories(argv[3]);
  if (!test_host::start(argc, argv)) {
    std::fprintf(stderr, "could not start the test host (no display for the desktop host? run under xvfb-run)\n");
    return 1;
  }
  F::InitOptions options;
  options.template_dir   = argv[3];
  options.font_store_dir = argv[4];
  F::Result init = F::init(options);
  if (!init.ok()) {
    std::fprintf(stderr, "init failed: %s\n", init.error.c_str());
    return 1;
  }
  // as the app does: scan the picked file, then install everything that can be
  int fd = ::open(argv[2], O_RDONLY);
  if (fd < 0) {
    std::fprintf(stderr, "can't open %s\n", argv[2]);
    return 1;
  }
  F::ImportScan scan;
  F::ImportResult r;
  std::thread([&] {
    scan = F::scanTemplates(fd);
    F::ImportSelection all;
    for (const F::ScannedPackage& p : scan.packages) all.packages.push_back(p.name);
    if (scan.ok()) r = F::installTemplates(fd, all);
  }).join();
  ::close(fd);
  if (!scan.ok()) {
    std::fprintf(stderr, "scan failed: %s\n", scan.error.c_str());
    return 1;
  }
  if (!r.ok()) {
    std::fprintf(stderr, "import failed: %s\n", r.error.c_str());
    return 1;
  }
  const char* names[] = {"installed", "replaced", "skipped", "refused", "failed"};
  int counts[5] = {};
  for (const F::ImportedPackage& p : r.packages) {
    ++counts[(int)p.outcome];
    if (p.outcome == F::ImportedPackage::Outcome::Refused || p.outcome == F::ImportedPackage::Outcome::Failed) {
      std::printf("%s %s: %s\n", names[(int)p.outcome], p.name.c_str(), p.error.c_str());
    }
  }
  for (const F::Message& m : r.messages) std::printf("message: %s\n", m.text.c_str());
  for (int i = 0 ; i < 5 ; ++i) std::printf("%s: %d\n", names[i], counts[i]);
  std::printf("fonts: %zu\n", r.fonts.size());
  std::printf("dictionaries: %zu\n", r.dictionaries.size());
  F::shutdown();
  test_host::stop();
  return counts[4] == 0 && r.messages.empty() ? 0 : 1;
}

int main(int argc, char** argv) {
  if (argc == 5 && std::string(argv[1]) == "--import") return import_main(argc, argv);
  if (argc < 3) {
    std::fprintf(stderr, "usage: %s <fixture dir> <work dir> [<fixture goldens dir>]\n", argv[0]);
    return 2;
  }
  fixture_dir = fs::absolute(argv[1]);
  work_dir    = fs::absolute(argv[2]);
  if (argc > 3) goldens_dir = fs::absolute(argv[3]);
  // a template store of our own, so import can write to it
  fs::remove_all(work_dir);
  templates_dir  = work_dir / "templates";
  font_store_dir = work_dir / "fonts";
  fs::create_directories(work_dir / "sessions");
  fs::copy(fixture_dir / "data", templates_dir, fs::copy_options::recursive);

  if (!test_host::start(argc, argv)) {
    std::fprintf(stderr, "could not start the test host (no display for the desktop host? run under xvfb-run)\n");
    return 1;
  }
  F::InitOptions options;
  options.template_dir   = templates_dir.string();
  options.font_store_dir = font_store_dir.string();
  F::Result init = F::init(options);
  if (!init.ok()) {
    std::fprintf(stderr, "init failed: %s\n", init.error.c_str());
    return 1;
  }
  int failed_tests = 0;
  for (const TestCase& t : tests()) {
    current_test = t.name;
    int before = failures;
    try {
      t.run();
    } catch (...) {
      report_failure(__FILE__, __LINE__, "a C++ exception escaped");
    }
    std::printf("%s %s\n", failures == before ? "ok  " : "FAIL", t.name);
    if (failures != before) ++failed_tests;
  }
  F::shutdown();
  test_host::stop();
  std::printf("%zu tests, %d failed, %d checks, %d failed checks\n", tests().size(), failed_tests, checks, failures);
  return failures == 0 ? 0 : 1;
}
