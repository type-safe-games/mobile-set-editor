// Mobile Set Editor - the facade over the core
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

/** @file facade/facade.hpp
 *
 *  The wx-free C++ facade over the Magic Set Editor core (ADR 0006, ADR 0009).
 *
 *  It speaks only plain types: UTF-8 strings, integers, pixel buffers and region rectangles.
 *
 *  Threading: the core is single threaded. The host owns one core thread and makes every call on
 *  it, starting with init(). The facade has no locks; the one exception is Template import
 *  (scanTemplates, installTemplates, importTemplates), which may run on any thread (it only writes
 *  the template store and the font store, and waits while a session is loading).
 *
 *  Errors: no C++ exception leaves a facade call. Every call returns a Result with the messages
 *  the core queued while it ran; a failed call has ok() == false and says why in `error`.
 *
 *  Host backends: the core draws through a Canvas and decodes and encodes images through an
 *  ImageCodec. The host installs both before init() (the desktop test host installs the wx ones,
 *  facade/desktop/desktop_host.hpp; Android will install Skia ones).
 *
 *  Text positions: the text of a text field is edited in *visible text*, the text the user sees
 *  (tags removed, atoms such as keyword reminders and separators shown as their contents).
 *  Positions in it are counted in Unicode code points (not bytes, not UTF-16 units).
 */

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace mse::facade {

// ----------------------------------------------------------------------------- : Results

enum class Severity { Info, Warning, Error, Fatal };

/// A message the core queued during a call
struct Message {
  Severity    severity = Severity::Info;
  std::string text;
  std::string card_uid; ///< the card the call was about, if any
  std::string field;    ///< the field the call was about, if any
};

enum class Status {
  Ok,
  Failed,          ///< an error in the core; see `error` and the messages
  MissingTemplate, ///< a Game, Style or other Template the Set needs is not installed; see `missing_package`
  NotFound,        ///< no such session, card or field
  InvalidArgument, ///< the call's arguments don't make sense (wrong value type, bad range, bad buffer, ...)
  WrongThread,     ///< called from a thread other than the core thread
};

struct Result {
  Status      status = Status::Ok;
  std::string error;           ///< why the call failed
  std::string missing_package; ///< for MissingTemplate: the package that is missing (e.g. "magic-m15.mse-style")
  std::vector<Message> messages;
  bool ok() const { return status == Status::Ok; }
};

/// What an action made stale, for the host's render cache and card grid
/** - all: re-render every card (after display, style or set info changes).
 *  - card_uids: otherwise, re-render these cards. They include the cards an action added or put
 *    back (undoing a removal), and every card whose rendering depends on the card list (e.g. a card
 *    number) and changed with it.
 *  - card_list: the list of cards changed (cards added, removed or moved). Call listCards() again
 *    to rebuild the grid, and drop cached renders of uids that are no longer listed. Cards that
 *    only moved keep their renders unless they are also in card_uids. */
struct Stale {
  bool all = false;
  std::vector<std::string> card_uids;
  bool card_list = false;
  bool empty() const { return !all && card_uids.empty() && !card_list; }
};

/// The result of anything that changes the Set
struct ActionResult : Result {
  Stale stale;
  bool  dirty    = false; ///< does the Set differ from what was last saved with saveTo()?
  bool  can_undo = false;
  bool  can_redo = false;
  std::string undo_name; ///< e.g. "Typing", empty if nothing to undo
  std::string redo_name;
};

// ----------------------------------------------------------------------------- : Initialisation

struct InitOptions {
  std::string template_dir;      ///< the template store: installed Games, Styles, symbol fonts, ...
  std::string program_data_dir;  ///< the program's own data (locales), may be empty
  std::string font_store_dir;    ///< the shared font store (loose fonts from Template import), may be empty
  std::string locale;            ///< e.g. "en"; empty for the built-in English strings
};

/// Initialise the core. Call once, on the thread that will be the core thread.
Result init(const InitOptions& options);

/// Release everything the core holds (closes all sessions).
Result shutdown();

// ----------------------------------------------------------------------------- : Sessions

/// An open Set. 0 is never a valid session.
using SessionHandle = std::int64_t;

struct OpenResult : Result {
  SessionHandle session = 0;
};

/// Open a Set (.mse-set file or directory) for editing.
/** If working_copy_dir is not empty, the Set is first copied there (as a directory) and edits go to
 *  that working copy, which flush() saves; the source file is never written. With an empty
 *  working_copy_dir the Set is edited in memory only. */
OpenResult openSession(const std::string& set_path, const std::string& working_copy_dir);

Result closeSession(SessionHandle session);

/// Autosave: write the working copy if the Set changed since the last flush.
/** The host debounces; calling flush after every action is allowed but slow. */
Result flush(SessionHandle session);

/// Write the Set as an .mse-set (zip) to a file descriptor the host opened for writing.
/** The descriptor is not closed. Afterwards the Set is no longer dirty. */
ActionResult saveTo(SessionHandle session, int fd);

struct SessionInfo : Result {
  std::string game;       ///< package name, e.g. "magic.mse-game"
  std::string stylesheet; ///< the Set's Style, e.g. "magic-m15.mse-style"
  bool dirty = false, can_undo = false, can_redo = false;
};
SessionInfo sessionInfo(SessionHandle session);

// ----------------------------------------------------------------------------- : Cards

struct CardLinkInfo {
  std::string uid;      ///< the linked card
  std::string relation; ///< what the linked card is to this one, e.g. "Back Face"
};

struct CardInfo {
  std::string uid;
  std::string name;       ///< the card's identification (its name, usually)
  std::string stylesheet; ///< the Style it renders with
  std::vector<CardLinkInfo> links;
};

struct CardList : Result {
  std::vector<CardInfo> cards; ///< in the Set's order
};
CardList listCards(SessionHandle session);

// ----------------------------------------------------------------------------- : Changing the card list

// Each call is one undo step, and reports `stale.card_list` (see Stale). Positions are indexes in
// the Set's card order, as listCards() gives it.

/// The cards a call made
struct NewCardsResult : ActionResult {
  std::vector<std::string> uids; ///< the new cards, in the Set's order
};

/// Add a new card: the Game's initial values, rendered with the Set's Style.
/** It goes at `position` (0 = first), or at the end when position is -1. uids holds its uid. */
NewCardsResult addCard(SessionHandle session, int position = -1);

/// Duplicate cards. As the desktop's copy and paste does, the cards linked to them are duplicated
/** too, and the copies are linked to each other the way the originals are (the originals' links
 *  don't change). The copies get new uids and go right after the last of the cards copied, in the
 *  same order as those. */
NewCardsResult duplicateCards(SessionHandle session, const std::vector<std::string>& card_uids);

/// Remove cards. A card left in the Set that was linked to a removed card loses that link; undo
/** brings back both the cards and the links. */
ActionResult removeCards(SessionHandle session, const std::vector<std::string>& card_uids);

/// Move cards, in the order they have in the Set, so that the first of them ends up at
/** `position` in the new order (0 <= position <= number of cards - number moved). Moving cards to
 *  where they already are changes nothing and adds no undo step. */
ActionResult moveCards(SessionHandle session, const std::vector<std::string>& card_uids, int position);

// ----------------------------------------------------------------------------- : Fields and values

/// What a field call is about
struct Target {
  enum class Kind {
    Card,      ///< a card's field
    Set,       ///< a set info field
    CardStyle, ///< the style options in effect for a card (the Set's, unless the card has its own)
    SetStyle,  ///< the Set's style options for its default Style
  };
  Kind        kind = Kind::Card;
  std::string card_uid; ///< for Card and CardStyle

  static Target card(const std::string& uid)      { return Target{Kind::Card, uid}; }
  static Target set()                             { return Target{Kind::Set, std::string()}; }
  static Target cardStyle(const std::string& uid) { return Target{Kind::CardStyle, uid}; }
  static Target setStyle()                        { return Target{Kind::SetStyle, std::string()}; }
};

struct FieldInfo {
  std::string name;
  std::string type;   ///< "text", "choice", "multiple choice", "color", "image", "symbol", "package choice", "info", ...
  bool editable = true;
  bool multi_line = false; ///< text fields
  std::vector<std::string> choices; ///< choice fields: the choice names, in order
};

struct FieldList : Result {
  std::vector<FieldInfo> fields;
};
FieldList listFields(SessionHandle session, const Target& target);

/// A value in its plain string form (see setValue)
struct ValueResult : Result {
  std::string type;
  std::string value;
};
ValueResult getValue(SessionHandle session, const Target& target, const std::string& field);

/// Replace a value whole. The string form depends on the field's type:
/** - text: the tagged text, as stored in the .mse-set
 *  - choice, package choice: the choice name / package name
 *  - multiple choice: the choice names separated by ", "
 *  - color: "rgb(r,g,b)", "rgba(r,g,b,a)" or "#rrggbb"
 *  - image, symbol: the path of a file to copy into the Set, or "" to clear the value
 *  One undo step. */
ActionResult setValue(SessionHandle session, const Target& target, const std::string& field, const std::string& value);

// ----------------------------------------------------------------------------- : Text editing

struct TextSpan {
  int start = 0, end = 0; ///< [start, end) in visible text code points
};

/// The state of a text field for an editor
struct TextResult : ActionResult {
  std::string text;          ///< the visible text (UTF-8)
  int selection_start = 0;   ///< the selection after the call, in visible text; the caret is selection_end
  int selection_end   = 0;
  std::vector<TextSpan> locked; ///< spans that can't be edited inside (each is replaced or removed as a whole)
};

/// The visible text of a text field, with the caret at its end
TextResult getText(SessionHandle session, const Target& target, const std::string& field);

struct AutoReplaceRule {
  std::string match;
  std::string replace; ///< tagged text
  bool whole_word = true;
  bool enabled = true;
};

/// The auto-replace settings for typing in a session (the Game's defaults until this is called)
struct AutoReplaceSettings {
  bool enabled = true;
  std::vector<AutoReplaceRule> rules;
};
Result setAutoReplace(SessionHandle session, const AutoReplaceSettings& settings);
/// The rules in effect: the ones set with setAutoReplace, or the Game's defaults
struct AutoReplaceResult : Result {
  AutoReplaceSettings settings;
};
AutoReplaceResult getAutoReplace(SessionHandle session);

/// Replace [start, end) of the visible text with `text`, as typing does.
/** Positions inside a locked span snap to its nearest edge, and a selection never crosses a
 *  separator. With as_symbol the text is inserted in the field's symbol font (e.g. a mana symbol
 *  code). Consecutive calls that each continue at the previous caret merge into one undo step.
 *  Afterwards scripts may have rewritten the value; the returned caret is placed where the text
 *  around it best matches what was typed. Auto-replace runs after inserting text. */
TextResult replaceRange(SessionHandle session, const Target& target, const std::string& field,
                        int start, int end, const std::string& text, bool as_symbol = false);

// ----------------------------------------------------------------------------- : Undo

ActionResult undo(SessionHandle session);
ActionResult redo(SessionHandle session);

// ----------------------------------------------------------------------------- : Linked cards

struct LinkType {
  std::string selected; ///< relation of the card the link is made from, e.g. "Front Face"
  std::string linked;   ///< relation of the other card, e.g. "Back Face"
};
struct LinkTypeList : Result {
  std::vector<LinkType> types; ///< the Game's card link types
};
LinkTypeList listLinkTypes(SessionHandle session);

/// Link two cards, one undo step. card's relation is `relation`, other's is `other_relation`
/** (for a LinkType: relation = selected, other_relation = linked). */
ActionResult link(SessionHandle session, const std::string& card_uid, const std::string& other_uid,
                  const std::string& relation, const std::string& other_relation);
/// Remove the link between two cards (both directions), one undo step
ActionResult unlink(SessionHandle session, const std::string& card_uid, const std::string& other_uid);

// ----------------------------------------------------------------------------- : Rendering

/// A field's region in a rendered card, in buffer pixels
struct FieldRegion {
  std::string field;
  double x = 0, y = 0, width = 0, height = 0; ///< the box the field occupies
  double rotation = 0;   ///< the field's rotation in degrees, counter clockwise
  int    z = 0;          ///< drawing order, higher is on top
  bool   editable = false;
};

struct CardSize : Result {
  double width = 0, height = 0; ///< the card's natural size in pixels (scale 1)
};
CardSize cardSize(SessionHandle session, const std::string& card_uid);

struct RenderResult : Result {
  std::vector<FieldRegion> regions; ///< visible fields, in drawing order
};

/// Draw a card into a caller-allocated buffer of RGBA pixels (8 bits each, row by row, alpha 255).
/** The scale is width / the card's natural width; height should keep the card's aspect ratio
 *  (if it doesn't, the card is drawn at the width's scale and clipped or padded). stride is the
 *  number of bytes per row (>= 4 * width). The regions come from the same layout pass. */
RenderResult renderCard(SessionHandle session, const std::string& card_uid,
                        std::uint8_t* rgba, int width, int height, int stride);

/// The core's Layout dump of a card (ADR 0010): the same JSON as `write_layout_file` in --cli mode
struct TextOutput : Result {
  std::string text;
};
TextOutput layoutDump(SessionHandle session, const std::string& card_uid);

// ----------------------------------------------------------------------------- : Export

struct ExportOptions {
  /// Unset: as the desktop exports with default settings for the card's Style
  std::optional<double> zoom;           ///< 1 = natural size
  std::optional<int>    angle_degrees;  ///< 0, 90, 180 or 270
  std::optional<bool>   bleed;          ///< add the print bleed edge
  std::optional<bool>   both_faces;     ///< put a Linked front/back pair side by side
  bool metadata = true;                 ///< embed <mse-card-data> in the PNG
};

struct BytesResult : Result {
  std::vector<std::uint8_t> data;
};
/// The finished PNG of a card, as the desktop's "export card image" makes it
BytesResult exportCardImage(SessionHandle session, const std::string& card_uid, const ExportOptions& options = ExportOptions());

// ----------------------------------------------------------------------------- : Template import

// Template import (ADR 0005) installs the Templates (*.mse-<type> directories, at any depth) and
// the loose .ttf/.otf fonts in a zip the user picked. `.mse-installer` files, as the zip itself
// or inside it, and zipped packages inside it are opened and scanned too. Spelling dictionaries
// (.dic/.aff files in a `dictionaries` directory, as in MSE's data directory and the Full Magic
// Pack) go into the template store's `dictionaries` directory, where the core looks for them.
//
// The import calls are the facade's exception to the core thread: they may run on any thread
// (they only read the zip and the template store, and write the store). Scan first, let the user
// pick, then install. Each package is extracted into a temporary directory in the store and
// renamed into place (while no session is loading), so a failure never leaves a half-installed
// Template. Sessions opened afterwards see the new Templates once refreshTemplates() has run on
// the core thread.
//
// The rules (ADR 0005): one version per package; a newer version replaces the installed one, the
// same or an older one is skipped (no downgrade). A package is refused if it declares a newer
// `mse version` than this program's, or if a package it depends on is neither installed nor in
// the zip in a recent enough version (or is itself refused). Installing a package also installs
// the packages in the zip it depends on, when the installed ones don't do.

/// A package another one needs
struct ImportDependency {
  std::string package; ///< e.g. "magic.mse-game"; "Magic Set Editor" is the program itself
  std::string version; ///< the oldest version that will do, may be empty
};

/// A Template found in a zip
struct ScannedPackage {
  std::string name;              ///< e.g. "magic-m15.mse-style"
  std::string type;              ///< e.g. "style"
  std::string full_name;         ///< for the picker (the short name if there is no full name)
  std::string version;           ///< as declared by the package, may be empty
  std::string installed_version; ///< of the installed package of that name, empty if there is none
  std::string installer_group;   ///< the picker group, e.g. "magic/frames/m15"; derived from the name as MSE's installer does when the package declares none
  std::vector<ImportDependency> dependencies;
  std::uint64_t size = 0;        ///< bytes when extracted
  enum class Verdict {
    Install,      ///< not installed yet
    Replace,      ///< newer than the installed version
    SkipNotNewer, ///< the same as or older than the installed version
    Refuse,       ///< can't be installed, see `reason`
  } verdict = Verdict::Install;
  std::string reason;            ///< why it is refused
};

struct ImportScan : Result {
  std::vector<ScannedPackage> packages; ///< one per package name, by installer group and name
  std::vector<std::string> fonts;       ///< the loose font files (file names) for the font store
  std::uint64_t fonts_size = 0;         ///< their bytes
  std::vector<std::string> dictionaries; ///< spelling dictionary files (file names)
};

/// List the Templates and loose fonts in a zip, and what installing each would do.
/** The descriptor (opened for reading by the host, e.g. from a picked document) is not closed and
 *  its file position is not used. */
ImportScan scanTemplates(int fd);
/// The same for a zip file
ImportScan scanTemplates(const std::string& zip_path);

/// What to install from a zip
struct ImportSelection {
  std::vector<std::string> packages; ///< package names from the scan; the packages in the zip they depend on are added
  bool fonts = true;                 ///< also copy the loose fonts into the font store
  bool dictionaries = true;          ///< also copy the spelling dictionaries
};

struct ImportedPackage {
  std::string name;    ///< e.g. "magic-m15.mse-style"
  std::string version; ///< as declared by the package, may be empty
  enum class Outcome {
    Installed,
    Replaced,
    SkippedNotNewer,
    Refused, ///< see `error`
    Failed,  ///< could not be installed (not in the zip, the zip is damaged, the disk is full, ...), see `error`
  } outcome = Outcome::Installed;
  bool dependency = false; ///< not selected, but added because a selected package needs it
  std::string error;   ///< for Refused and Failed
};

struct ImportResult : Result {
  std::vector<ImportedPackage> packages; ///< the selected packages and the dependencies added, dependencies first
  std::vector<std::string> fonts;        ///< loose font files copied into the font store
  std::vector<std::string> dictionaries; ///< spelling dictionary files copied (new or changed)
};

/// Install the selected Templates (and their dependencies) and the loose fonts from a zip.
/** The zip is scanned again, so the outcomes follow the rules above even if the store changed
 *  since scanTemplates. The descriptor is not closed. */
ImportResult installTemplates(int fd, const ImportSelection& selection);
/// The same for a zip file
ImportResult installTemplates(const std::string& zip_path, const ImportSelection& selection);

/// Install everything in a zip file that scanTemplates says can be installed, its loose fonts and dictionaries
ImportResult importTemplates(const std::string& zip_path);

/// On the core thread, after installing Templates: forget cached packages so new sessions load the
/// imported versions, and register their fonts and the font store's new files.
Result refreshTemplates();

} // namespace mse::facade
