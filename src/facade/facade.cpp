// Mobile Set Editor - the facade over the core
// SPDX-License-Identifier: GPL-3.0-or-later

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <facade/facade.hpp>
#include <facade/template_import.hpp>
#include <util/thread.hpp>
#include <util/file_utils.hpp>
#include <util/tagged_string.hpp>
#include <util/io/package_manager.hpp>
#include <util/io/stream.hpp>
#include <data/set.hpp>
#include <data/card.hpp>
#include <data/card_link.hpp>
#include <data/game.hpp>
#include <data/keyword.hpp>
#include <data/stylesheet.hpp>
#include <data/locale.hpp>
#include <data/word_list.hpp>
#include <data/text_editing.hpp>
#include <data/action/value.hpp>
#include <data/action/set.hpp>
#include <data/action/keyword.hpp>
#include <data/field/text.hpp>
#include <data/symbol_font.hpp>
#include <data/field/choice.hpp>
#include <data/field/multiple_choice.hpp>
#include <data/field/color.hpp>
#include <data/field/image.hpp>
#include <data/field/symbol.hpp>
#include <data/field/package_choice.hpp>
#include <data/format/formats.hpp>
#include <gfx/canvas.hpp>
#include <gfx/image_codec.hpp>
#include <gfx/image_load.hpp>
#include <gfx/font_registry.hpp>
#include <script/script.hpp>
#include <script/parser.hpp>
#include <atomic>
#include <filesystem>
#include <mutex>

namespace mse::facade {

// ----------------------------------------------------------------------------- : Strings

static String from_utf8(const std::string& s) { return decode_utf8(s); }
static std::string to_utf8(const String& s)    { return encode_utf8(s); }

// ----------------------------------------------------------------------------- : Errors

/// An error the facade itself raises, with the status to report
struct FacadeError {
  Status status;
  std::string message;
};
[[noreturn]] static void fail(Status status, const std::string& message) {
  throw FacadeError{status, message};
}

static Severity severity_of(MessageType type) {
  switch (type) {
    case MESSAGE_WARNING:     return Severity::Warning;
    case MESSAGE_ERROR:       return Severity::Error;
    case MESSAGE_FATAL_ERROR: return Severity::Fatal;
    default:                  return Severity::Info;
  }
}

// The messages of the call that is running on the core thread
static vector<Message>* current_messages = nullptr;
static std::string current_card, current_field;

/// The ErrorSink while the facade owns the core: messages go to the running call
static bool facade_error_sink(MessageType type, const String& msg) {
  if (!current_messages) return false; // between calls: keep it queued for the next call
  current_messages->push_back(Message{severity_of(type), to_utf8(msg), current_card, current_field});
  return true;
}

/// Collects the messages of one call, also those queued from other threads
class CallScope {
public:
  CallScope(vector<Message>& out, const std::string& card, const std::string& field) : out(out) {
    current_messages = &out;
    current_card = card;
    current_field = field;
  }
  ~CallScope() {
    // messages queued from other threads, or before this call
    MessageType type; String msg;
    while (get_queued_message(type, msg)) {
      out.push_back(Message{severity_of(type), to_utf8(msg), current_card, current_field});
    }
    current_messages = nullptr;
    current_card.clear();
    current_field.clear();
  }
private:
  vector<Message>& out;
};

/// Run the body of a facade call: on the core thread only, catching everything
template <typename R, typename F>
R guarded(const std::string& card, const std::string& field, F&& body) {
  R r;
  if (!is_core_thread()) {
    r.status = Status::WrongThread;
    r.error = "facade call made off the core thread";
    return r;
  }
  CallScope scope(r.messages, card, field);
  try {
    body(r);
  } catch (const FacadeError& e) {
    r.status = e.status;
    r.error = e.message;
  } catch (const PackageNotFoundError& e) {
    r.status = Status::MissingTemplate;
    r.error = to_utf8(e.what());
    r.missing_package = to_utf8(file_full_name(e.package_name));
  } catch (const Error& e) {
    r.status = Status::Failed;
    r.error = to_utf8(e.what());
  } catch (const std::exception& e) {
    r.status = Status::Failed;
    r.error = e.what();
  } catch (...) {
    r.status = Status::Failed;
    r.error = "an unexpected exception occurred";
  }
  return r;
}

// ----------------------------------------------------------------------------- : Sessions

/// Collects the cards that actions made stale
class StaleCollector : public ActionListener {
public:
  bool  collecting = false;
  Stale stale;
  set<const StyleSheet*> changed_styles; ///< stylesheets with styles that scripts invalidated

  void onAction(const Action& action, bool undone) override {
    if (!collecting) return;
    if (const CardListAction* a = dynamic_cast<const CardListAction*>(&action)) {
      stale.card_list = true;
      // added cards, and removed ones that are put back, have no up to date render
      if (const AddCardAction* add = dynamic_cast<const AddCardAction*>(a)) {
        if (add->action.adding != undone) {
          for (const auto& step : add->action.steps) addCard(step.item->uid);
        }
      }
      return;
    }
    if (const ScriptStyleEvent* e = dynamic_cast<const ScriptStyleEvent*>(&action)) {
      changed_styles.insert(e->stylesheet);
      return;
    }
    if (stale.all) return;
    if (const ScriptValueEvent* e = dynamic_cast<const ScriptValueEvent*>(&action)) {
      if (e->card) addCard(e->card->uid); else stale.all = true;
    } else if (const ValueAction* a = dynamic_cast<const ValueAction*>(&action)) {
      if (a->card) {
        addCard(a->card->uid);
      } else if (dynamic_cast<const KeywordTextValue*>(a->valueP.get())) {
        // a keyword's field: the cards whose text changed with it come as ScriptValueEvents
      } else {
        // a value without card is set info or style options: shown on every card
        stale.all = true;
      }
    } else if (dynamic_cast<const KeywordListAction*>(&action) || dynamic_cast<const ChangeKeywordModeAction*>(&action)) {
      // likewise
    } else if (const OneWayLinkCardsAction* a = dynamic_cast<const OneWayLinkCardsAction*>(&action)) {
      addCard(a->card->uid);
    } else if (const ChangeCardStyleAction* a = dynamic_cast<const ChangeCardStyleAction*>(&action)) {
      addCard(a->card->uid);
    } else if (const ChangeCardHasStylingAction* a = dynamic_cast<const ChangeCardHasStylingAction*>(&action)) {
      addCard(a->card->uid);
    } else if (dynamic_cast<const BulkAction*>(&action)) {
      // a BulkAction's parts are reported one by one
    } else {
      // display changes, the Set's Style, ...
      stale.all = true;
    }
  }

  /// After a card list change, scripts in a Style that depend on the card list (e.g. a card
  /// number drawn by the Style) may have changed: every card with that Style is stale
  void addStyleDependentCards(Set& set) {
    if (stale.card_list && !stale.all && !changed_styles.empty()) {
      for (const CardP& c : set.cards) {
        if (changed_styles.count(&set.stylesheetFor(c))) addCard(c->uid);
      }
    }
    changed_styles.clear();
  }

private:
  void addCard(const String& uid) {
    if (stale.all) return;
    std::string u = to_utf8(uid);
    if (std::find(stale.card_uids.begin(), stale.card_uids.end(), u) == stale.card_uids.end()) {
      stale.card_uids.push_back(u);
    }
  }
};

struct Session {
  SetP   set;
  String working_copy;              ///< directory of the working copy, or empty
  size_t generation = 0;            ///< counts actions, for autosave
  size_t flushed_generation = 0;
  bool   custom_auto_replace = false;
  bool   auto_replace_enabled = true;
  vector<AutoReplaceP> auto_replaces; ///< when custom_auto_replace
  StaleCollector stale;
  // keywords: their ids, and the values that edit their fields (one per keyword and field, so that
  // typing merges into one undo step, as in the desktop's keyword panel)
  std::map<const Keyword*, KeywordId> keyword_ids;
  std::map<KeywordId, KeywordP> keywords;
  KeywordId next_keyword_id = 1;
  TextFieldP keyword_fields[4];
  std::map<std::pair<const Keyword*, int>, TextValueP> keyword_values;

  ~Session() {
    if (set) set->actions.removeListener(&stale);
  }
};

static std::map<SessionHandle, unique_ptr<Session>> sessions;
static SessionHandle next_session = 1;
static bool initialized = false;

// Template import may run on another thread; it waits while a session is loading
static std::mutex loading_mutex;

static Session& get_session(SessionHandle h) {
  auto it = sessions.find(h);
  if (it == sessions.end()) fail(Status::NotFound, "no such session");
  return *it->second;
}

static CardP find_card(Set& set, const std::string& uid) {
  String u = from_utf8(uid);
  for (const CardP& c : set.cards) {
    if (c->uid == u) return c;
  }
  fail(Status::NotFound, "no card with uid '" + uid + "'");
}

/// Start collecting stale cards for an action
static void begin_action(Session& s) {
  s.stale.stale = Stale();
  s.stale.changed_styles.clear();
  s.stale.collecting = true;
}

/// The undo state and dirty flag of a session
static void fill_undo_state(Session& s, ActionResult& r) {
  ActionStack& actions = s.set->actions;
  r.dirty     = !actions.atSavePoint();
  r.can_undo  = actions.canUndo();
  r.can_redo  = actions.canRedo();
  r.undo_name = to_utf8(trim(actions.undoName()));
  r.redo_name = to_utf8(trim(actions.redoName()));
}

/// Finish an action call: stale cards and undo state
static void end_action(Session& s, ActionResult& r) {
  s.stale.collecting = false;
  s.stale.addStyleDependentCards(*s.set);
  r.stale = s.stale.stale;
  s.stale.stale = Stale();
  fill_undo_state(s, r);
}

/// Run an action on a session, with stale cards collected; stops collecting even when it throws
template <typename F>
static void with_action(Session& s, ActionResult& r, F&& body) {
  begin_action(s);
  try {
    body();
    // edits of keyword fields postpone updating the cards that use keywords; do it now
    s.set->updateDelayed();
  } catch (...) {
    s.stale.collecting = false;
    throw;
  }
  ++s.generation;
  end_action(s, r);
}

// ----------------------------------------------------------------------------- : Initialisation

static std::string template_dir_, font_store_dir_;

/// Add the shared font store to the font registry, and give the Canvas backend its new files
static void add_font_store() {
  if (font_store_dir_.empty()) return;
  String dir = from_utf8(font_store_dir_);
  font_registry().addDirectory(dir, FontSource::FONT_STORE);
  static set<String> given;
  vector<String> paths;
  for (const String& path : list_font_files(dir)) {
    if (given.insert(path).second) paths.push_back(path);
  }
  if (!canvas_factory().addFontFiles(paths)) {
    queue_message(MESSAGE_WARNING, _("Could not load all fonts in the font store ") + dir);
  }
}

Result init(const InitOptions& options) {
  // the calling thread becomes the core thread
  set_core_thread();
  return guarded<Result>("", "", [&](Result&) {
    error_sink = facade_error_sink;
    if (!initialized) {
      init_script_variables();
      init_file_formats();
    }
    if (options.template_dir.empty()) fail(Status::InvalidArgument, "no template directory");
    package_manager.init(from_utf8(options.template_dir), from_utf8(options.program_data_dir));
    the_locale = options.locale.empty() ? LocaleP() : Locale::byName(from_utf8(options.locale));
    template_dir_   = options.template_dir;
    font_store_dir_ = options.font_store_dir;
    add_font_store();
    set_template_store(options.template_dir, options.font_store_dir);
    initialized = true;
  });
}

Result shutdown() {
  return guarded<Result>("", "", [&](Result&) {
    sessions.clear();
    package_manager.destroy();
    the_locale = LocaleP();
  });
}

Result refreshTemplates() {
  return guarded<Result>("", "", [&](Result&) {
    package_manager.reset();
    if (!template_dir_.empty()) font_registry().addTemplateDirectory(from_utf8(template_dir_));
    add_font_store();
  });
}

// ----------------------------------------------------------------------------- : Sessions

/// Write a new session's working copy, if it has one (while holding loading_mutex)
static void make_working_copy(Session& s, const std::string& working_copy_dir) {
  if (working_copy_dir.empty()) return;
  // the working copy is a directory, so autosaving never rewrites a whole zip
  String work = from_utf8(working_copy_dir);
  if (file_exists(work) || dir_exists(work)) fail(Status::InvalidArgument, "the working copy '" + working_copy_dir + "' already exists");
  s.set->saveAs(work, false, true);
  s.working_copy = work;
}

/// Register a new session
static void add_session(unique_ptr<Session> s, OpenResult& r) {
  s->set->actions.addListener(&s->stale);
  r.session = next_session++;
  sessions[r.session] = std::move(s);
}

OpenResult openSession(const std::string& set_path, const std::string& working_copy_dir) {
  return guarded<OpenResult>("", "", [&](OpenResult& r) {
    if (!initialized) fail(Status::Failed, "the facade is not initialised");
    auto s = make_unique<Session>();
    {
      std::lock_guard<std::mutex> lock(loading_mutex);
      String path = from_utf8(set_path);
      if (!file_exists(path) && !dir_exists(path)) fail(Status::NotFound, "no set at '" + set_path + "'");
      SetP set = make_intrusive<Set>();
      set->open(path);
      s->set = set;
      make_working_copy(*s, working_copy_dir);
    }
    add_session(std::move(s), r);
  });
}

/// Open an installed package of type T by its package name, with the given extension
template <typename T>
static intrusive_ptr<T> open_package(const std::string& name, const Char* ext, const char* what) {
  String n = from_utf8(name);
  if (!ends_with(n, ext)) fail(Status::InvalidArgument, "'" + name + "' is not " + what);
  return package_manager.open<T>(n);
}

/// An installed Style of a Game
static StyleSheetP open_stylesheet(const Game& game, const std::string& name) {
  StyleSheetP stylesheet = open_package<StyleSheet>(name, _(".mse-style"), "a Style");
  if (stylesheet->game.get() != &game) {
    fail(Status::InvalidArgument, "'" + name + "' is not a Style of '" + to_utf8(game.relativeFilename()) + "'");
  }
  return stylesheet;
}

OpenResult newSession(const std::string& game, const std::string& stylesheet, const std::string& working_copy_dir) {
  return guarded<OpenResult>("", "", [&](OpenResult& r) {
    if (!initialized) fail(Status::Failed, "the facade is not initialised");
    auto s = make_unique<Session>();
    {
      std::lock_guard<std::mutex> lock(loading_mutex);
      GameP g = open_package<Game>(game, _(".mse-game"), "a Game");
      // as the desktop's new set window: the Game's default set info, and one card
      SetP set = make_intrusive<Set>(open_stylesheet(*g, stylesheet));
      set->validate();
      s->set = set;
      make_working_copy(*s, working_copy_dir);
    }
    add_session(std::move(s), r);
  });
}

OpenResult openWorkingCopy(const std::string& working_copy_dir, bool unsaved) {
  return guarded<OpenResult>("", "", [&](OpenResult& r) {
    if (!initialized) fail(Status::Failed, "the facade is not initialised");
    auto s = make_unique<Session>();
    {
      std::lock_guard<std::mutex> lock(loading_mutex);
      String work = from_utf8(working_copy_dir);
      if (!dir_exists(work)) fail(Status::NotFound, "no working copy at '" + working_copy_dir + "'");
      SetP set = make_intrusive<Set>();
      set->open(work);
      if (unsaved) set->actions.setUnsaved();
      s->set = set;
      s->working_copy = work;
    }
    add_session(std::move(s), r);
  });
}

Result closeSession(SessionHandle h) {
  return guarded<Result>("", "", [&](Result&) {
    get_session(h);
    sessions.erase(h);
  });
}

Result flush(SessionHandle h) {
  return guarded<Result>("", "", [&](Result&) {
    Session& s = get_session(h);
    if (s.working_copy.empty() || s.generation == s.flushed_generation) return;
    // keep files that undo may bring back
    s.set->save(false);
    s.flushed_generation = s.generation;
  });
}

ActionResult saveTo(SessionHandle h, int fd) {
  return guarded<ActionResult>("", "", [&](ActionResult& r) {
    Session& s = get_session(h);
    if (fd < 0) fail(Status::InvalidArgument, "invalid file descriptor");
    FdOutputStream out(fd, false);
    s.set->saveCopy(out);
    if (!out.close()) fail(Status::Failed, "could not write the set to the file descriptor");
    s.set->actions.setSavePoint();
    end_action(s, r);
  });
}

SessionInfo sessionInfo(SessionHandle h) {
  return guarded<SessionInfo>("", "", [&](SessionInfo& r) {
    Session& s = get_session(h);
    r.game       = to_utf8(s.set->game->relativeFilename());
    r.stylesheet = to_utf8(s.set->stylesheet->relativeFilename());
    r.dirty      = !s.set->actions.atSavePoint();
    r.can_undo   = s.set->actions.canUndo();
    r.can_redo   = s.set->actions.canRedo();
  });
}

// ----------------------------------------------------------------------------- : Cards

CardList listCards(SessionHandle h) {
  return guarded<CardList>("", "", [&](CardList& r) {
    Session& s = get_session(h);
    for (const CardP& c : s.set->cards) {
      CardInfo info;
      info.uid        = to_utf8(c->uid);
      info.name       = to_utf8(untag(c->identification()));
      info.stylesheet = to_utf8(s.set->stylesheetFor(c).relativeFilename());
      info.own_stylesheet = (bool)c->stylesheet;
      info.own_styling    = c->has_styling;
      for (int i = 0 ; i < Card::MAX_LINKS ; ++i) {
        const String& uid = c->getLinkedUID(i);
        if (!uid.empty()) info.links.push_back(CardLinkInfo{to_utf8(uid), to_utf8(c->getLinkedRelation(i))});
      }
      r.cards.push_back(std::move(info));
    }
  });
}

// ----------------------------------------------------------------------------- : Changing the card list

/// The cards with the given uids, in the Set's order
static vector<CardP> find_cards(Set& set, const std::vector<std::string>& uids) {
  if (uids.empty()) fail(Status::InvalidArgument, "no cards given");
  unordered_set<String> wanted;
  for (const std::string& uid : uids) {
    find_card(set, uid);
    if (!wanted.insert(from_utf8(uid)).second) fail(Status::InvalidArgument, "card '" + uid + "' is given twice");
  }
  vector<CardP> cards;
  for (const CardP& c : set.cards) {
    if (wanted.count(c->uid)) cards.push_back(c);
  }
  return cards;
}

/// Add cards to the end of the card list, then move them to `position` (if they don't go at the end)
/** One undo step, named after the add. */
static void add_cards_at(Session& s, const vector<CardP>& cards, size_t position) {
  Set& set = *s.set;
  vector<ActionP> actions;
  actions.push_back(make_intrusive<AddCardAction>(ADD, set, cards)); // gives new uids to copies
  if (position < set.cards.size()) {
    vector<CardP> order = set.cards;
    order.insert(order.begin() + position, cards.begin(), cards.end());
    actions.push_back(make_intrusive<MoveCardsAction>(set, order));
  }
  set.actions.addAction(make_unique<BulkAction>(actions, s.set, false), false);
}

NewCardsResult addCard(SessionHandle h, int position) {
  return guarded<NewCardsResult>("", "", [&](NewCardsResult& r) {
    Session& s = get_session(h);
    if (position < -1 || position > (int)s.set->cards.size()) fail(Status::InvalidArgument, "position out of range");
    // as the desktop's "add card": the Game's initial values, no Style of its own
    CardP card = make_intrusive<Card>(*s.set->game);
    with_action(s, r, [&] { add_cards_at(s, {card}, position < 0 ? s.set->cards.size() : (size_t)position); });
    r.uids.push_back(to_utf8(card->uid));
  });
}

NewCardsResult duplicateCards(SessionHandle h, const std::vector<std::string>& card_uids) {
  return guarded<NewCardsResult>("", "", [&](NewCardsResult& r) {
    Session& s = get_session(h);
    Set& set = *s.set;
    vector<CardP> selected = find_cards(set, card_uids);
    // with the cards linked to them, as the desktop copies cards
    unordered_set<String> to_copy;
    for (const CardP& c : selected) {
      to_copy.insert(c->uid);
      for (const auto& linked : c->getLinkedCards(set)) to_copy.insert(linked.first->uid);
    }
    vector<CardP> copies;
    size_t after = 0;
    for (size_t i = 0 ; i < set.cards.size() ; ++i) {
      const CardP& c = set.cards[i];
      if (!to_copy.count(c->uid)) continue;
      // a deep copy with the same uid and links; adding it gives it a new uid and relinks the copies
      CardP copy = make_intrusive<Card>(&set, c);
      copy->stylesheet = c->stylesheet; // keep following the Set's Style if the card does
      copies.push_back(copy);
      after = i + 1;
    }
    with_action(s, r, [&] { add_cards_at(s, copies, after); });
    for (const CardP& c : copies) r.uids.push_back(to_utf8(c->uid));
  });
}

ActionResult removeCards(SessionHandle h, const std::vector<std::string>& card_uids) {
  return guarded<ActionResult>("", "", [&](ActionResult& r) {
    Session& s = get_session(h);
    Set& set = *s.set;
    vector<CardP> cards = find_cards(set, card_uids);
    unordered_set<String> removed;
    for (const CardP& c : cards) removed.insert(c->uid);
    // first unlink the cards that stay from the removed ones, so that undo puts the cards back
    // before their links (and scripts that follow links see the linked card)
    vector<ActionP> actions;
    for (CardP c : set.cards) {
      if (removed.count(c->uid)) continue;
      for (int i = 0 ; i < Card::MAX_LINKS ; ++i) {
        if (removed.count(c->getLinkedUID(i))) {
          actions.push_back(make_intrusive<OneWayLinkCardsAction>(c, _(""), _(""), i));
        }
      }
    }
    ActionP removal = make_intrusive<AddCardAction>(REMOVE, set, cards);
    actions.push_back(removal);
    with_action(s, r, [&] { set.actions.addAction(make_unique<BulkAction>(actions, s.set, *removal), false); });
  });
}

ActionResult moveCards(SessionHandle h, const std::vector<std::string>& card_uids, int position) {
  return guarded<ActionResult>("", "", [&](ActionResult& r) {
    Session& s = get_session(h);
    Set& set = *s.set;
    vector<CardP> moved = find_cards(set, card_uids);
    if (position < 0 || position > (int)(set.cards.size() - moved.size())) fail(Status::InvalidArgument, "position out of range");
    unordered_set<String> moving;
    for (const CardP& c : moved) moving.insert(c->uid);
    vector<CardP> order;
    for (const CardP& c : set.cards) {
      if (!moving.count(c->uid)) order.push_back(c);
    }
    order.insert(order.begin() + position, moved.begin(), moved.end());
    if (order == set.cards) {
      fill_undo_state(s, r); // already there: no undo step
      return;
    }
    with_action(s, r, [&] { set.actions.addAction(make_unique<MoveCardsAction>(set, order), false); });
  });
}

// ----------------------------------------------------------------------------- : Styles

ActionResult setCardStyle(SessionHandle h, const std::string& card_uid, const std::string& stylesheet) {
  return guarded<ActionResult>(card_uid, "", [&](ActionResult& r) {
    Session& s = get_session(h);
    Set& set = *s.set;
    CardP card = find_card(set, card_uid);
    StyleSheetP style = stylesheet.empty() ? StyleSheetP() : open_stylesheet(*set.game, stylesheet);
    // as the desktop's style panel: the Set's Style means no Style of its own
    if (style == set.stylesheet) style = StyleSheetP();
    if (style == card->stylesheet) {
      fill_undo_state(s, r); // no change: no undo step
      return;
    }
    with_action(s, r, [&] { set.actions.addAction(make_unique<ChangeCardStyleAction>(card, style)); });
  });
}

ActionResult setSetStyle(SessionHandle h, const std::string& stylesheet) {
  return guarded<ActionResult>("", "", [&](ActionResult& r) {
    Session& s = get_session(h);
    Set& set = *s.set;
    StyleSheetP style = open_stylesheet(*set.game, stylesheet);
    if (style == set.stylesheet) {
      fill_undo_state(s, r);
      return;
    }
    with_action(s, r, [&] { set.actions.addAction(make_unique<ChangeSetStyleAction>(set, style)); });
  });
}

ActionResult setCardOwnStyling(SessionHandle h, const std::string& card_uid, bool own_styling) {
  return guarded<ActionResult>(card_uid, "", [&](ActionResult& r) {
    Session& s = get_session(h);
    Set& set = *s.set;
    CardP card = find_card(set, card_uid);
    if (card->has_styling == own_styling) {
      fill_undo_state(s, r);
      return;
    }
    with_action(s, r, [&] { set.actions.addAction(make_unique<ChangeCardHasStylingAction>(set, card)); });
  });
}

// ----------------------------------------------------------------------------- : Fields and values

/// A value a field call is about
struct FieldRef {
  ValueP value;
  CardP  card;  ///< for card values, so the action knows its card
  StyleP style; ///< the field's style, if there is one
  bool   read_only_keyword = false; ///< a field of a Game keyword
};

static FieldRef find_keyword_field(Session& s, const Target& t, const std::string& name);

static FieldRef find_field(Session& s, const Target& t, const std::string& name) {
  if (t.kind == Target::Kind::Keyword) return find_keyword_field(s, t, name);
  Set& set = *s.set;
  FieldRef ref;
  IndexMap<FieldP, ValueP>* values = nullptr;
  IndexMap<FieldP, StyleP>* styles = nullptr;
  switch (t.kind) {
    case Target::Kind::Card: {
      ref.card = find_card(set, t.card_uid);
      values = &ref.card->data;
      styles = &const_cast<StyleSheet&>(set.stylesheetFor(ref.card)).card_style;
      break;
    }
    case Target::Kind::Set:
      values = &set.data;
      styles = &set.stylesheet->set_info_style;
      break;
    case Target::Kind::CardStyle: {
      CardP card = find_card(set, t.card_uid);
      values = &set.stylingDataFor(card);
      styles = &const_cast<StyleSheet&>(set.stylesheetFor(card)).styling_style;
      break;
    }
    case Target::Kind::SetStyle:
      values = &set.stylingDataFor(*set.stylesheet);
      styles = &set.stylesheet->styling_style;
      break;
    case Target::Kind::Keyword:
      break; // above
  }
  String n = from_utf8(name);
  for (const ValueP& v : *values) {
    if (v->fieldP->name == n) {
      ref.value = v;
      if (styles && v->fieldP->index < styles->size()) ref.style = styles->at(v->fieldP->index);
      return ref;
    }
  }
  if (t.kind == Target::Kind::Card) {
    // the extra card fields of the card's Style
    StyleSheet& stylesheet = const_cast<StyleSheet&>(set.stylesheetFor(ref.card));
    for (const ValueP& v : ref.card->extraDataFor(stylesheet)) {
      if (v->fieldP->name == n) {
        ref.value = v;
        if (v->fieldP->index < stylesheet.extra_card_style.size()) ref.style = stylesheet.extra_card_style.at(v->fieldP->index);
        return ref;
      }
    }
  }
  fail(Status::NotFound, "no field '" + name + "'");
}

/// Add a value action on the session's set, for the given card (if any)
static void add_value_action(Session& s, const CardP& card, unique_ptr<ValueAction> action) {
  if (!action) return;
  if (card) action->setCard(card);
  s.set->actions.addAction(std::move(action));
}

static void check_editable(const FieldRef& ref, const std::string& field) {
  if (ref.read_only_keyword) fail(Status::InvalidArgument, "the Game's keywords can't be edited");
  if (!ref.value->fieldP->editable) fail(Status::InvalidArgument, "field '" + field + "' can't be edited");
}

// ----------------------------------------------------------------------------- : Keywords : fields

/// The text fields of a keyword, as in the desktop's keyword panel
static const char* const keyword_text_fields[] = {"keyword", "match", "reminder", "rules"};
enum { KW_KEYWORD, KW_MATCH, KW_REMINDER, KW_RULES, KW_TEXT_FIELDS };

static KeywordId keyword_id(Session& s, const KeywordP& kw) {
  auto it = s.keyword_ids.find(kw.get());
  if (it != s.keyword_ids.end()) return it->second;
  KeywordId id = s.next_keyword_id++;
  s.keyword_ids[kw.get()] = id;
  s.keywords[id] = kw;
  return id;
}

struct KeywordRef {
  KeywordP keyword;
  bool from_game = false;
};

static bool has_keyword(const vector<KeywordP>& list, const Keyword* kw) {
  for (const KeywordP& k : list) if (k.get() == kw) return true;
  return false;
}

/// A keyword of the Set or its Game (not one that was removed)
static KeywordRef find_keyword(Session& s, KeywordId id) {
  auto it = s.keywords.find(id);
  if (it != s.keywords.end()) {
    if (has_keyword(s.set->keywords, it->second.get()))       return KeywordRef{it->second, false};
    if (has_keyword(s.set->game->keywords, it->second.get())) return KeywordRef{it->second, true};
  }
  fail(Status::NotFound, "no keyword with id " + std::to_string(id));
}

/// The value that edits a text field of a keyword
static TextValueP keyword_value(Session& s, const KeywordRef& k, int field) {
  auto key = std::make_pair((const Keyword*)k.keyword.get(), field);
  auto it = s.keyword_values.find(key);
  if (it != s.keyword_values.end()) return it->second;
  TextFieldP& f = s.keyword_fields[field];
  if (!f) {
    f = make_intrusive<TextField>();
    f->name = from_utf8(keyword_text_fields[field]);
    f->multi_line = field == KW_REMINDER || field == KW_RULES;
    if (field == KW_MATCH) f->script = s.set->game->keyword_match_script;
  }
  Keyword* kw = k.keyword.get();
  bool editable = !k.from_game;
  intrusive_ptr<FakeTextValue> v;
  switch (field) {
    case KW_KEYWORD:  v = make_intrusive<KeywordTextValue>(f, kw, &kw->keyword, editable, true); break;
    case KW_MATCH:    v = make_intrusive<KeywordTextValue>(f, kw, &kw->match, editable); break;
    case KW_REMINDER: v = make_intrusive<KeywordReminderTextValue>(*s.set, f, kw, editable); break;
    default:          v = make_intrusive<KeywordTextValue>(f, kw, &kw->rules, editable); break;
  }
  v->retrieve();
  s.keyword_values[key] = v;
  return v;
}

static int keyword_text_field(const std::string& name) {
  for (int i = 0 ; i < KW_TEXT_FIELDS ; ++i) {
    if (name == keyword_text_fields[i]) return i;
  }
  return -1;
}

static FieldRef find_keyword_field(Session& s, const Target& t, const std::string& name) {
  KeywordRef k = find_keyword(s, t.keyword_id);
  int field = keyword_text_field(name);
  if (field < 0) {
    if (name == "mode") fail(Status::InvalidArgument, "field 'mode' is not a text field");
    fail(Status::NotFound, "no field '" + name + "'");
  }
  FieldRef ref;
  ref.value = keyword_value(s, k, field);
  ref.read_only_keyword = k.from_game;
  return ref;
}

static void check_keyword_mode(const Game& game, const String& mode) {
  if (game.keyword_modes.empty()) return; // any mode will do
  for (const KeywordModeP& m : game.keyword_modes) {
    if (m->name == mode) return;
  }
  fail(Status::InvalidArgument, "no keyword mode '" + to_utf8(mode) + "'");
}

/// Change a keyword's mode, one undo step (none if it doesn't change)
static void set_keyword_mode(Session& s, const Target& t, const std::string& mode, ActionResult& r) {
  KeywordRef k = find_keyword(s, t.keyword_id);
  if (k.from_game) fail(Status::InvalidArgument, "the Game's keywords can't be edited");
  String m = from_utf8(mode);
  check_keyword_mode(*s.set->game, m);
  if (m == k.keyword->mode) {
    fill_undo_state(s, r);
    return;
  }
  with_action(s, r, [&] { s.set->actions.addAction(make_unique<ChangeKeywordModeAction>(*k.keyword, m)); });
}

static FieldInfo field_info(const Field& f) {
  FieldInfo info;
  info.name     = to_utf8(f.name);
  info.type     = to_utf8(f.typeName());
  info.editable = f.editable;
  if (const TextField* tf = dynamic_cast<const TextField*>(&f)) {
    info.multi_line = tf->multi_line;
  }
  if (const ChoiceField* cf = dynamic_cast<const ChoiceField*>(&f)) {
    if (cf->choices) {
      for (int i = 0 ; i < cf->choices->lastId() ; ++i) {
        info.choices.push_back(to_utf8(cf->choices->choiceName(i)));
      }
    }
  }
  if (const ColorField* cf = dynamic_cast<const ColorField*>(&f)) {
    for (const ColorField::ChoiceP& c : cf->choices) {
      info.colors.push_back(ColorChoice{to_utf8(c->name), to_utf8(format_color(c->color))});
    }
    info.allow_custom = cf->allow_custom;
  }
  if (const PackageChoiceField* pf = dynamic_cast<const PackageChoiceField*>(&f)) {
    if (!pf->required) info.choices.push_back(std::string());
    vector<PackagedP> packages;
    package_manager.findMatching(pf->match, packages);
    for (const PackagedP& p : packages) info.choices.push_back(to_utf8(p->relativeFilename()));
  }
  return info;
}

FieldList listFields(SessionHandle h, const Target& t) {
  return guarded<FieldList>(t.card_uid, "", [&](FieldList& r) {
    Session& s = get_session(h);
    Set& set = *s.set;
    IndexMap<FieldP, ValueP>* values = nullptr;
    switch (t.kind) {
      case Target::Kind::Card: {
        CardP card = find_card(set, t.card_uid);
        for (const ValueP& v : card->data) r.fields.push_back(field_info(*v->fieldP));
        // and the extra card fields of the card's Style (e.g. a saga's chapter count)
        values = &card->extraDataFor(set.stylesheetFor(card));
        break;
      }
      case Target::Kind::Set:       values = &set.data; break;
      case Target::Kind::CardStyle: values = &set.stylingDataFor(find_card(set, t.card_uid)); break;
      case Target::Kind::SetStyle:  values = &set.stylingDataFor(*set.stylesheet); break;
      case Target::Kind::Keyword: {
        KeywordRef k = find_keyword(s, t.keyword_id);
        for (int i = 0 ; i < KW_TEXT_FIELDS ; ++i) {
          r.fields.push_back(field_info(*keyword_value(s, k, i)->fieldP));
          r.fields.back().editable = !k.from_game;
        }
        FieldInfo mode;
        mode.name     = "mode";
        mode.type     = "choice";
        mode.editable = !k.from_game;
        for (const KeywordModeP& m : set.game->keyword_modes) mode.choices.push_back(to_utf8(m->name));
        r.fields.push_back(mode);
        return;
      }
    }
    for (const ValueP& v : *values) r.fields.push_back(field_info(*v->fieldP));
  });
}

static std::string value_string(const Value& v) {
  if (const TextValue*           x = dynamic_cast<const TextValue*>(&v))           return to_utf8(x->value());
  if (const MultipleChoiceValue* x = dynamic_cast<const MultipleChoiceValue*>(&v)) return to_utf8(x->value());
  if (const ChoiceValue*         x = dynamic_cast<const ChoiceValue*>(&v))         return to_utf8(x->value());
  if (const ColorValue*          x = dynamic_cast<const ColorValue*>(&v))          return to_utf8(format_color(x->value()));
  if (const ImageValue*          x = dynamic_cast<const ImageValue*>(&v))          return to_utf8(x->filename.toStringForKey());
  if (const SymbolValue*         x = dynamic_cast<const SymbolValue*>(&v))         return to_utf8(x->filename.toStringForKey());
  if (const PackageChoiceValue*  x = dynamic_cast<const PackageChoiceValue*>(&v))  return to_utf8(x->package_name);
  return to_utf8(v.toString());
}

ValueResult getValue(SessionHandle h, const Target& t, const std::string& field) {
  return guarded<ValueResult>(t.card_uid, field, [&](ValueResult& r) {
    Session& s = get_session(h);
    if (t.kind == Target::Kind::Keyword && field == "mode") {
      r.type  = "choice";
      r.value = to_utf8(find_keyword(s, t.keyword_id).keyword->mode);
      return;
    }
    FieldRef ref = find_field(s, t, field);
    r.type  = to_utf8(ref.value->fieldP->typeName());
    r.value = value_string(*ref.value);
  });
}

/// Store a file in the set's package under a new name; images are stored as PNG like the desktop does
static LocalFileName store_file(Set& set, const String& field_name, const String& path, bool as_image) {
  if (as_image) {
    Image img;
    if (!image_load_file(img, path)) fail(Status::InvalidArgument, "can't read the image '" + to_utf8(path) + "'");
    LocalFileName name = set.newFileName(field_name, _(".png"));
    if (!image_save_file(img, set.nameOut(name), _("png"))) fail(Status::Failed, "can't write the image into the set");
    return name;
  } else {
    LocalFileName name = set.newFileName(field_name, _(".mse-symbol"));
    if (!copy_file(path, set.nameOut(name))) fail(Status::InvalidArgument, "can't copy '" + to_utf8(path) + "' into the set");
    return name;
  }
}

ActionResult setValue(SessionHandle h, const Target& t, const std::string& field, const std::string& value) {
  return guarded<ActionResult>(t.card_uid, field, [&](ActionResult& r) {
    Session& s = get_session(h);
    if (t.kind == Target::Kind::Keyword && field == "mode") {
      set_keyword_mode(s, t, value, r);
      return;
    }
    FieldRef ref = find_field(s, t, field);
    check_editable(ref, field);
    String v = from_utf8(value);
    unique_ptr<ValueAction> action;
    const ValueP& val = ref.value;
    if (TextValueP x = dynamic_pointer_cast<TextValue>(val)) {
      action = value_action(x, Defaultable<String>(v));
    } else if (MultipleChoiceValueP x = dynamic_pointer_cast<MultipleChoiceValue>(val)) {
      action = value_action(x, Defaultable<String>(v), String());
    } else if (ChoiceValueP x = dynamic_pointer_cast<ChoiceValue>(val)) {
      action = value_action(x, Defaultable<String>(v));
    } else if (ColorValueP x = dynamic_pointer_cast<ColorValue>(val)) {
      optional<Color> c = parse_color(v);
      if (!c) fail(Status::InvalidArgument, "not a color: '" + value + "'");
      action = value_action(x, Defaultable<Color>(*c));
    } else if (PackageChoiceValueP x = dynamic_pointer_cast<PackageChoiceValue>(val)) {
      action = value_action(x, v);
    } else if (ImageValueP x = dynamic_pointer_cast<ImageValue>(val)) {
      action = value_action(x, v.empty() ? LocalFileName() : store_file(*s.set, x->fieldP->name, v, true));
    } else if (SymbolValueP x = dynamic_pointer_cast<SymbolValue>(val)) {
      action = value_action(x, v.empty() ? LocalFileName() : store_file(*s.set, x->fieldP->name, v, false));
    } else {
      fail(Status::InvalidArgument, "values of type '" + to_utf8(val->fieldP->typeName()) + "' can't be set");
    }
    with_action(s, r, [&] { add_value_action(s, ref.card, std::move(action)); });
  });
}

// ----------------------------------------------------------------------------- : Text editing

static void text_state(const TextValue& value, size_t sel_start, size_t sel_end, TextResult& r) {
  VisibleText vt = visible_text(value.value());
  r.text = to_utf8(vt.text);
  r.selection_start = (int)vt.toVisible(sel_start);
  r.selection_end   = (int)vt.toVisible(sel_end);
  for (auto& span : vt.locked) r.locked.push_back(TextSpan{(int)span.first, (int)span.second});
}

static TextValueP text_value(const FieldRef& ref, const std::string& field) {
  TextValueP tv = dynamic_pointer_cast<TextValue>(ref.value);
  if (!tv) fail(Status::InvalidArgument, "field '" + field + "' is not a text field");
  return tv;
}

/// A selection given in visible text positions (which snap out of locked spans), as cursor positions
static TextSelection visible_selection(const TextValue& value, int start, int end) {
  VisibleText vt = visible_text(value.value());
  TextSelection sel;
  sel.start = vt.toCursor((size_t)start);
  sel.end   = vt.toCursor((size_t)end);
  fix_text_selection(value.value(), sel);
  return sel;
}

TextResult getText(SessionHandle h, const Target& t, const std::string& field) {
  return guarded<TextResult>(t.card_uid, field, [&](TextResult& r) {
    Session& s = get_session(h);
    TextValueP tv = text_value(find_field(s, t, field), field);
    size_t end = untag_for_cursor(tv->value()).size();
    text_state(*tv, end, end, r);
    fill_undo_state(s, r);
  });
}

static vector<AutoReplaceP> game_auto_replaces(const Game& game) {
  vector<AutoReplaceP> out;
  for (const AutoReplaceP& ar : game.auto_replaces) out.push_back(ar->clone());
  return out;
}

Result setAutoReplace(SessionHandle h, const AutoReplaceSettings& settings) {
  return guarded<Result>("", "", [&](Result&) {
    Session& s = get_session(h);
    s.custom_auto_replace  = true;
    s.auto_replace_enabled = settings.enabled;
    s.auto_replaces.clear();
    for (const AutoReplaceRule& rule : settings.rules) {
      AutoReplaceP ar = make_intrusive<AutoReplace>();
      ar->match      = from_utf8(rule.match);
      ar->replace    = from_utf8(rule.replace);
      ar->whole_word = rule.whole_word;
      ar->enabled    = rule.enabled;
      ar->custom     = true;
      s.auto_replaces.push_back(ar);
    }
  });
}

AutoReplaceResult getAutoReplace(SessionHandle h) {
  return guarded<AutoReplaceResult>("", "", [&](AutoReplaceResult& r) {
    Session& s = get_session(h);
    r.settings.enabled = s.auto_replace_enabled;
    for (const AutoReplaceP& ar : s.custom_auto_replace ? s.auto_replaces : s.set->game->auto_replaces) {
      r.settings.rules.push_back(AutoReplaceRule{to_utf8(ar->match), to_utf8(ar->replace), ar->whole_word, ar->enabled});
    }
  });
}

TextResult replaceRange(SessionHandle h, const Target& t, const std::string& field,
                        int start, int end, const std::string& text, bool as_symbol) {
  return guarded<TextResult>(t.card_uid, field, [&](TextResult& r) {
    Session& s = get_session(h);
    FieldRef ref = find_field(s, t, field);
    TextValueP tv = text_value(ref, field);
    const TextField& tf = static_cast<const TextField&>(*tv->fieldP);
    check_editable(ref, field);
    if (start < 0 || end < 0) fail(Status::InvalidArgument, "negative text position");
    // the replacement as tagged text
    String plain = from_utf8(text);
    if (!tf.multi_line) plain.erase(std::remove(plain.begin(), plain.end(), _('\n')), plain.end());
    String replacement = escape(plain);
    if (as_symbol && !plain.empty()) {
      const TextStyle* style = dynamic_cast<const TextStyle*>(ref.style.get());
      if (!(style && style->always_symbol)) replacement = _("<sym>") + replacement + _("</sym>");
    }
    TextSelection sel = visible_selection(*tv, start, end);
    AddValueAction add = [&](unique_ptr<ValueAction> action) { add_value_action(s, ref.card, std::move(action)); };
    String name = replacement.empty() ? _ACTION_("backspace") : _ACTION_("typing");
    with_action(s, r, [&] {
      ReplaceResult result = replace_text_selection(tv, sel, replacement, name, true, add);
      if (result == ReplaceResult::CHANGED && !replacement.empty()) {
        // auto replace after typing
        if (s.auto_replace_enabled) {
          auto_replace_text(tv, sel, s.custom_auto_replace ? s.auto_replaces : game_auto_replaces(*s.set->game), add);
        }
      }
    });
    text_state(*tv, sel.start, sel.end, r);
  });
}

// ----------------------------------------------------------------------------- : Inserting symbols

/// A text field's style with its scripts up to date, so its symbol font is loaded
static const TextStyle& updated_text_style(Session& s, const FieldRef& ref, const std::string& field) {
  const TextStyle* style = dynamic_cast<const TextStyle*>(ref.style.get());
  if (!style) fail(Status::InvalidArgument, "'" + field + "' is not a text field with a style");
  Context& ctx = ref.card ? s.set->getContext(ref.card) : s.set->getContext();
  ref.style->update(ctx);
  return *style;
}

/// The field's symbol font, if it can hold symbols; its scripts (e.g. which symbols are enabled) up to date
static SymbolFont* field_symbol_font(Session& s, const FieldRef& ref, const TextStyle& style) {
  if (!(style.always_symbol || style.allow_formating) || !style.symbol_font.valid()) return nullptr;
  SymbolFont& font = *style.symbol_font.font;
  Context& ctx = ref.card ? s.set->getContext(ref.card) : s.set->getContext();
  ctx.setVariable(_("active_color"), to_script(Color(0, 0, 0)));
  font.update(ctx);
  return &font;
}

static SymbolMenuItem symbol_menu_item(const InsertSymbolMenu& m, int& id) {
  SymbolMenuItem item;
  // as the menu shows it: without '&' accelerators, and without the shortcut after a tab
  String label = replace_all(m.label.get(), _("&"), _(""));
  item.label = to_utf8(label.substr(0, label.find(_('\t'))));
  switch (m.type) {
    case InsertSymbolMenu::Type::CODE:
      item.type = SymbolMenuItem::Type::Code;
      item.code = to_utf8(m.name);
      item.id   = id++;
      break;
    case InsertSymbolMenu::Type::CUSTOM:
      item.type   = SymbolMenuItem::Type::Custom;
      item.prompt = to_utf8(m.prompt.get());
      item.id     = id++;
      break;
    case InsertSymbolMenu::Type::LINE:
      item.type = SymbolMenuItem::Type::Line;
      break;
    case InsertSymbolMenu::Type::SUBMENU:
      item.type = SymbolMenuItem::Type::Submenu;
      for (const InsertSymbolMenuP& i : m.items) item.items.push_back(symbol_menu_item(*i, id));
      break;
  }
  return item;
}

SymbolMenu symbolMenu(SessionHandle h, const Target& t, const std::string& field) {
  return guarded<SymbolMenu>(t.card_uid, field, [&](SymbolMenu& r) {
    Session& s = get_session(h);
    FieldRef ref = find_field(s, t, field);
    text_value(ref, field);
    const TextStyle& style = updated_text_style(s, ref, field);
    SymbolFont* font = field_symbol_font(s, ref, style);
    if (!font) return;
    r.has_symbols   = true;
    r.always_symbol = style.always_symbol;
    if (const InsertSymbolMenu* menu = font->insertSymbolMenu()) {
      // ids count the code and custom items in order, as InsertSymbolMenu::itemAt does
      int id = 0;
      SymbolMenuItem top = symbol_menu_item(*menu, id);
      if (top.type == SymbolMenuItem::Type::Submenu) r.items = std::move(top.items);
      else r.items.push_back(std::move(top));
    }
  });
}

ImageData symbolPreview(SessionHandle h, const Target& t, const std::string& field, int item_id, int size) {
  return guarded<ImageData>(t.card_uid, field, [&](ImageData& r) {
    Session& s = get_session(h);
    if (size <= 0 || size > 1024) fail(Status::InvalidArgument, "invalid preview size");
    FieldRef ref = find_field(s, t, field);
    text_value(ref, field);
    const TextStyle& style = updated_text_style(s, ref, field);
    SymbolFont* font = field_symbol_font(s, ref, style);
    if (!font) fail(Status::InvalidArgument, "'" + field + "' has no symbols");
    const InsertSymbolMenu* menu = font->insertSymbolMenu();
    const InsertSymbolMenu* item = menu ? menu->itemAt(item_id) : nullptr;
    if (!item) fail(Status::NotFound, "no symbol menu item " + std::to_string(item_id));
    Image img = font->insertSymbolPreview(*item, IntSize(size, size));
    if (!img.Ok()) return;
    r.width  = img.GetWidth();
    r.height = img.GetHeight();
    const Byte* rgb = img.GetData();
    const Byte* alpha = img.GetAlpha();
    r.rgba.resize((size_t)r.width * r.height * 4);
    for (size_t i = 0 ; i < (size_t)r.width * r.height ; ++i) {
      r.rgba[4*i+0] = rgb[3*i+0];
      r.rgba[4*i+1] = rgb[3*i+1];
      r.rgba[4*i+2] = rgb[3*i+2];
      r.rgba[4*i+3] = alpha ? alpha[i] : 255;
    }
  });
}

// ----------------------------------------------------------------------------- : Keywords

/// A keyword's match as the desktop lists it: "Charge <number>"
static String match_display(const String& match) {
  String out;
  for (size_t pos = 0 ; pos < match.size() ; ) {
    size_t open = match.find(_("<atom-param>"), pos);
    if (open == String::npos) {
      out += untag(match.substr(pos));
      break;
    }
    out += untag(match.substr(pos, open - pos));
    size_t close = match.find(_("</atom-param>"), open);
    if (close == String::npos) close = match.size();
    out += _("<") + untag(match.substr(open + 12, close - open - 12)) + _(">");
    pos = close + 13;
  }
  return out;
}

static KeywordInfo keyword_info(Session& s, const KeywordP& kw, bool from_game) {
  Set& set = *s.set;
  kw->prepare(set.game->keyword_parameter_types); // as expanding keywords does: finds the parameters, checks the match
  KeywordInfo info;
  info.id        = keyword_id(s, kw);
  info.keyword   = to_utf8(kw->keyword);
  info.match     = to_utf8(match_display(kw->match));
  info.reminder  = to_utf8(kw->reminder.getUnparsed());
  info.rules     = to_utf8(kw->rules);
  info.mode      = to_utf8(kw->mode);
  for (const KeywordParamP& p : kw->parameters) info.parameters.push_back(to_utf8(p->name));
  info.from_game = from_game;
  auto it = s.keyword_values.find(std::make_pair((const Keyword*)kw.get(), (int)KW_REMINDER));
  if (it != s.keyword_values.end()) {
    info.reminder_error = to_utf8(static_cast<const KeywordReminderTextValue&>(*it->second).errors);
  }
  info.valid = kw->valid && info.reminder_error.empty();
  for (const CardP& c : set.cards) {
    for (const auto& use : c->keyword_usage) {
      if (use.second == kw.get()) { ++info.usage; break; }
    }
  }
  return info;
}

KeywordList listKeywords(SessionHandle h) {
  return guarded<KeywordList>("", "", [&](KeywordList& r) {
    Session& s = get_session(h);
    for (const KeywordP& kw : s.set->keywords)       r.keywords.push_back(keyword_info(s, kw, false));
    for (const KeywordP& kw : s.set->game->keywords) r.keywords.push_back(keyword_info(s, kw, true));
  });
}

KeywordResult getKeyword(SessionHandle h, KeywordId id) {
  return guarded<KeywordResult>("", "", [&](KeywordResult& r) {
    Session& s = get_session(h);
    KeywordRef k = find_keyword(s, id);
    r.keyword = keyword_info(s, k.keyword, k.from_game);
  });
}

KeywordModeList listKeywordModes(SessionHandle h) {
  return guarded<KeywordModeList>("", "", [&](KeywordModeList& r) {
    Session& s = get_session(h);
    for (const KeywordModeP& m : s.set->game->keyword_modes) {
      r.modes.push_back(KeywordModeInfo{to_utf8(m->name), to_utf8(m->description), m->is_default});
    }
  });
}

KeywordParamTypeList listKeywordParamTypes(SessionHandle h) {
  return guarded<KeywordParamTypeList>("", "", [&](KeywordParamTypeList& r) {
    Session& s = get_session(h);
    for (const KeywordParamP& p : s.set->game->keyword_parameter_types) {
      KeywordParamTypeInfo info;
      info.name        = to_utf8(p->name);
      info.description = to_utf8(p->description);
      info.placeholder = to_utf8(p->placeholder);
      info.example     = to_utf8(p->example);
      info.optional    = p->optional;
      r.types.push_back(info);
    }
  });
}

NewKeywordResult addKeyword(SessionHandle h, const NewKeyword& nk) {
  return guarded<NewKeywordResult>("", "", [&](NewKeywordResult& r) {
    Session& s = get_session(h);
    Set& set = *s.set;
    // as the desktop's "add keyword": a new keyword in the Game's default mode
    unique_ptr<AddKeywordAction> action = make_unique<AddKeywordAction>(set);
    KeywordP kw = action->action.steps.front().item;
    kw->keyword = from_utf8(nk.keyword);
    kw->match   = from_utf8(nk.match);
    kw->rules   = from_utf8(nk.rules);
    if (!nk.mode.empty()) {
      kw->mode = from_utf8(nk.mode);
      check_keyword_mode(*set.game, kw->mode);
    }
    if (!nk.reminder.empty()) {
      String reminder = from_utf8(nk.reminder);
      vector<ScriptParseError> errors;
      ScriptP script = parse(reminder, nullptr, true, errors);
      if (!errors.empty()) fail(Status::InvalidArgument, "error in the reminder text: " + to_utf8(ScriptParseErrors(errors).what()));
      kw->reminder.setScriptP(script);
      kw->reminder.setUnparsed(reminder);
    }
    with_action(s, r, [&] { set.actions.addAction(std::move(action)); });
    r.id = keyword_id(s, kw);
  });
}

ActionResult removeKeyword(SessionHandle h, KeywordId id) {
  return guarded<ActionResult>("", "", [&](ActionResult& r) {
    Session& s = get_session(h);
    KeywordRef k = find_keyword(s, id);
    if (k.from_game) fail(Status::InvalidArgument, "the Game's keywords can't be removed");
    with_action(s, r, [&] { s.set->actions.addAction(make_unique<AddKeywordAction>(REMOVE, *s.set, k.keyword)); });
  });
}

TextResult insertKeywordParameter(SessionHandle h, KeywordId id, int start, int end, const std::string& param_type) {
  return guarded<TextResult>("", "match", [&](TextResult& r) {
    Session& s = get_session(h);
    KeywordRef k = find_keyword(s, id);
    if (k.from_game) fail(Status::InvalidArgument, "the Game's keywords can't be edited");
    if (start < 0 || end < 0) fail(Status::InvalidArgument, "negative text position");
    String type = from_utf8(param_type);
    bool known = false;
    for (const KeywordParamP& p : s.set->game->keyword_parameter_types) known |= p->name == type;
    if (!known) fail(Status::InvalidArgument, "no keyword parameter type '" + param_type + "'");
    TextValueP tv = keyword_value(s, k, KW_MATCH);
    TextSelection sel = visible_selection(*tv, start, end);
    AddValueAction add = [&](unique_ptr<ValueAction> action) { add_value_action(s, CardP(), std::move(action)); };
    // as the desktop's "insert parameter" button
    with_action(s, r, [&] {
      replace_text_selection(tv, sel, _("<atom-param>") + escape(type) + _("</atom-param>"), _("Insert parameter"), true, add);
    });
    text_state(*tv, sel.start, sel.end, r);
  });
}

// ----------------------------------------------------------------------------- : Undo

ActionResult undo(SessionHandle h) {
  return guarded<ActionResult>("", "", [&](ActionResult& r) {
    Session& s = get_session(h);
    if (!s.set->actions.canUndo()) fail(Status::InvalidArgument, "nothing to undo");
    with_action(s, r, [&] { s.set->actions.undo(); });
  });
}

ActionResult redo(SessionHandle h) {
  return guarded<ActionResult>("", "", [&](ActionResult& r) {
    Session& s = get_session(h);
    if (!s.set->actions.canRedo()) fail(Status::InvalidArgument, "nothing to redo");
    with_action(s, r, [&] { s.set->actions.redo(); });
  });
}

// ----------------------------------------------------------------------------- : Linked cards

LinkTypeList listLinkTypes(SessionHandle h) {
  return guarded<LinkTypeList>("", "", [&](LinkTypeList& r) {
    Session& s = get_session(h);
    // The core always puts the built-in "Front Face"/"Back Face" first (Game::after_reading), so a
    // Game that declares that pair itself has it twice; the desktop's link dialog lists both.
    // Same pairs are one choice here.
    for (const CardLinkP& link : s.set->game->card_links) {
      LinkType t{to_utf8(link->selected.default_), to_utf8(link->linked.default_)};
      bool seen = false;
      for (const LinkType& u : r.types) seen |= u.selected == t.selected && u.linked == t.linked;
      if (!seen) r.types.push_back(t);
    }
  });
}

ActionResult link(SessionHandle h, const std::string& card_uid, const std::string& other_uid,
                  const std::string& relation, const std::string& other_relation) {
  return guarded<ActionResult>(card_uid, "", [&](ActionResult& r) {
    Session& s = get_session(h);
    CardP card  = find_card(*s.set, card_uid);
    CardP other = find_card(*s.set, other_uid);
    if (card == other) fail(Status::InvalidArgument, "can't link a card to itself");
    int card_slot  = card->findFreeLink(other->uid, s.set->card_uids);
    int other_slot = other->findFreeLink(card->uid, s.set->card_uids);
    if (card_slot < 0 || other_slot < 0) fail(Status::InvalidArgument, "no free link slot (or the cards are already linked)");
    // each card's slot names the other card, and what the other card is to it
    vector<ActionP> actions;
    actions.push_back(make_intrusive<OneWayLinkCardsAction>(card,  other->uid, from_utf8(other_relation), card_slot));
    actions.push_back(make_intrusive<OneWayLinkCardsAction>(other, card->uid,  from_utf8(relation),       other_slot));
    with_action(s, r, [&] { s.set->actions.addAction(make_unique<BulkAction>(actions, s.set, false), false); });
  });
}

ActionResult unlink(SessionHandle h, const std::string& card_uid, const std::string& other_uid) {
  return guarded<ActionResult>(card_uid, "", [&](ActionResult& r) {
    Session& s = get_session(h);
    CardP card  = find_card(*s.set, card_uid);
    CardP other = find_card(*s.set, other_uid);
    int card_slot  = card->findUIDLink(other->uid);
    int other_slot = other->findUIDLink(card->uid);
    if (card_slot < 0 && other_slot < 0) fail(Status::InvalidArgument, "the cards are not linked");
    vector<ActionP> actions;
    if (card_slot  >= 0) actions.push_back(make_intrusive<OneWayLinkCardsAction>(card,  _(""), _(""), card_slot));
    if (other_slot >= 0) actions.push_back(make_intrusive<OneWayLinkCardsAction>(other, _(""), _(""), other_slot));
    with_action(s, r, [&] { s.set->actions.addAction(make_unique<BulkAction>(actions, s.set, false), false); });
  });
}

// ----------------------------------------------------------------------------- : Rendering

CardSize cardSize(SessionHandle h, const std::string& card_uid) {
  return guarded<CardSize>(card_uid, "", [&](CardSize& r) {
    Session& s = get_session(h);
    RealSize size = card_render_size(s.set, find_card(*s.set, card_uid));
    r.width  = size.width;
    r.height = size.height;
  });
}

RenderResult renderCard(SessionHandle h, const std::string& card_uid, std::uint8_t* rgba, int width, int height, int stride) {
  return guarded<RenderResult>(card_uid, "", [&](RenderResult& r) {
    Session& s = get_session(h);
    if (!rgba || width <= 0 || height <= 0 || stride < 4 * width) fail(Status::InvalidArgument, "invalid pixel buffer");
    CardP card = find_card(*s.set, card_uid);
    RealSize size = card_render_size(s.set, card);
    if (size.width <= 0 || size.height <= 0) fail(Status::Failed, "the card has no size");
    double zoom = width / size.width;
    OffscreenCanvasP canvas = new_canvas(width, height, CanvasFormat::RGB);
    vector<CardFieldRegion> regions;
    render_card(s.set, card, *canvas, zoom, &regions);
    Image img = canvas->getImage();
    if (img.GetWidth() != width || img.GetHeight() != height) fail(Status::Failed, "the canvas has the wrong size");
    const Byte* rgb = img.GetData();
    for (int y = 0 ; y < height ; ++y) {
      std::uint8_t* row = rgba + (size_t)y * stride;
      const Byte* src = rgb + (size_t)y * width * 3;
      for (int x = 0 ; x < width ; ++x) {
        row[4*x+0] = src[3*x+0];
        row[4*x+1] = src[3*x+1];
        row[4*x+2] = src[3*x+2];
        row[4*x+3] = 255;
      }
    }
    for (const CardFieldRegion& cr : regions) {
      FieldRegion fr;
      fr.field    = to_utf8(cr.field);
      fr.x        = cr.rect.x;
      fr.y        = cr.rect.y;
      fr.width    = cr.rect.width;
      fr.height   = cr.rect.height;
      fr.rotation = cr.angle;
      fr.z        = cr.z_index;
      fr.editable = cr.editable;
      r.regions.push_back(fr);
    }
  });
}

FieldAt fieldAt(SessionHandle h, const std::string& card_uid, double x, double y, int width) {
  return guarded<FieldAt>(card_uid, "", [&](FieldAt& r) {
    Session& s = get_session(h);
    if (width <= 0) fail(Status::InvalidArgument, "invalid width");
    CardP card = find_card(*s.set, card_uid);
    RealSize size = card_render_size(s.set, card);
    if (size.width <= 0) fail(Status::Failed, "the card has no size");
    double to_card = size.width / width;
    r.field = to_utf8(card_field_at(s.set, card, RealPoint(x * to_card, y * to_card)));
  });
}

TextOutput layoutDump(SessionHandle h, const std::string& card_uid) {
  return guarded<TextOutput>(card_uid, "", [&](TextOutput& r) {
    Session& s = get_session(h);
    r.text = to_utf8(card_layout_dump(s.set, find_card(*s.set, card_uid)));
  });
}

// ----------------------------------------------------------------------------- : Export

BytesResult exportCardImage(SessionHandle h, const std::string& card_uid, const ExportOptions& options) {
  return guarded<BytesResult>(card_uid, "", [&](BytesResult& r) {
    Session& s = get_session(h);
    CardP card = find_card(*s.set, card_uid);
    const StyleSheet& stylesheet = s.set->stylesheetFor(card);
    // the desktop's export settings for this Style, with the host's overrides
    Settings::ExportSettings es = settings.exportSettingsFor(stylesheet);
    if (options.zoom) {
      if (*options.zoom <= 0) fail(Status::InvalidArgument, "invalid zoom");
      // the bleed edge scales with the zoom
      es.bleed_pixels = es.bleed_pixels / es.zoom * *options.zoom;
      es.zoom = *options.zoom;
    }
    if (options.angle_degrees) {
      int a = ((*options.angle_degrees % 360) + 360) % 360;
      if (a % 90 != 0) fail(Status::InvalidArgument, "the angle must be a multiple of 90 degrees");
      es.angle_radians = deg_to_rad(a);
    }
    if (options.bleed) {
      es.bleed_pixels = *options.bleed ? (stylesheet.card_dpi / 300.0) * 36.0 * es.zoom : 0.0; // as Settings::exportSettingsFor
    }
    if (options.both_faces) es.dfc_export = *options.both_faces;
    // with overrides, the other face of a pair is exported the same way
    bool overridden = options.zoom || options.angle_degrees || options.bleed || options.both_faces;
    Image img = export_image(s.set, card, options.metadata, es, overridden);
    StringOutputStream out;
    if (!image_codec().encode(img, _("png"), out) || !out.close()) fail(Status::Failed, "could not encode the image");
    r.data.assign(out.str().begin(), out.str().end());
  });
}

// ----------------------------------------------------------------------------- : Installed Templates

/// What listTemplates shows of a Style that only its data file's header says: its Game and card size
/** The top level "key: value" lines, read the way read_header in template_import.cpp reads them,
 *  so the Style (and its Game) need not be loaded. */
static void read_style_header(Packaged& style, TemplateInfo& info) {
  // StyleSheet's defaults
  info.card_width  = 100;
  info.card_height = 100;
  info.card_dpi    = 96;
  String data = decode_utf8(style.openIn(_("style"))->readAll());
  if (!data.empty() && data[0] == 0xFEFF) data.erase(0, 1); // byte order mark
  size_t pos = 0;
  while (pos < data.size()) {
    size_t eol = data.find(_('\n'), pos);
    if (eol == String::npos) eol = data.size();
    String line = data.substr(pos, eol - pos);
    pos = eol + 1;
    if (line.empty() || line[0] == _('\t') || line[0] == _(' ') || line[0] == _('#')) continue;
    size_t colon = line.find(_(':'));
    if (colon == String::npos) continue;
    String key   = replace_all(trim(line.substr(0, colon)), _("_"), _(" "));
    String value = trim(line.substr(colon + 1));
    auto number = [&](double& out) {
      try { out = std::stod(to_utf8(value)); } catch (...) {}
    };
    if (key == _("game")) {
      info.game = to_utf8(ends_with(value, _(".mse-game")) ? value : value + _(".mse-game"));
    }
    else if (key == _("card width"))  number(info.card_width);
    else if (key == _("card height")) number(info.card_height);
    else if (key == _("card dpi"))    number(info.card_dpi);
  }
}

TemplateList listTemplates(const std::string& type) {
  return guarded<TemplateList>("", "", [&](TemplateList& r) {
    if (!initialized) fail(Status::Failed, "the facade is not initialised");
    namespace fs = std::filesystem;
    std::vector<std::string> names;
    std::error_code ec;
    for (fs::directory_iterator it(fs::u8path(template_dir_), ec), end ; !ec && it != end ; it.increment(ec)) {
      std::string name = it->path().filename().u8string();
      size_t ext = name.rfind(".mse-");
      // hidden ones are left out (import's temporary directories among them)
      if (name.empty() || name[0] == '.' || ext == std::string::npos || ext == 0) continue;
      std::string t = name.substr(ext + 5);
      if (t == "locale" || t == "installer") continue; // not Templates
      if (!type.empty() && t != type) continue;
      names.push_back(name);
    }
    if (ec) fail(Status::Failed, "can't read the template store '" + template_dir_ + "': " + ec.message());
    std::sort(names.begin(), names.end());
    for (const std::string& name : names) {
      try {
        PackagedP p = package_manager.openAny(from_utf8(name), true);
        TemplateInfo info;
        info.name = name;
        info.type = name.substr(name.rfind(".mse-") + 5);
        String base = from_utf8(name.substr(0, name.rfind(".mse-")));
        info.short_name = to_utf8(p->short_name.empty() ? base : p->short_name);
        info.full_name  = p->full_name.empty() ? info.short_name : to_utf8(p->full_name);
        if (p->version != Version()) info.version = to_utf8(p->version.toString());
        if (!p->icon_filename.empty()) {
          String icon = p->absoluteFilename() + _("/") + p->icon_filename;
          if (file_exists(icon)) info.icon_path = to_utf8(icon);
        }
        if (info.type == "style") read_style_header(*p, info);
        r.templates.push_back(std::move(info));
      } catch (const Error& e) {
        queue_message(MESSAGE_WARNING, _("Can't read the installed Template ") + from_utf8(name) + _(": ") + e.what());
      }
    }
  });
}

// ----------------------------------------------------------------------------- : Template import

/// Run an import call, which may be on any thread: no exception leaves it
template <typename R, typename F>
static R import_call(F&& body) {
  R r;
  try {
    body(r);
  } catch (const FacadeError& e) {
    r.status = e.status;
    r.error = e.message;
  } catch (const Error& e) {
    r.status = Status::Failed;
    r.error = to_utf8(e.what());
  } catch (const std::exception& e) {
    r.status = Status::Failed;
    r.error = e.what();
  } catch (...) {
    r.status = Status::Failed;
    r.error = "an unexpected exception occurred";
  }
  return r;
}

ImportScan scanTemplates(int fd) {
  return import_call<ImportScan>([&](ImportScan& r) {
    if (fd < 0) fail(Status::InvalidArgument, "not a file descriptor");
    scan_templates(ImportInput{fd, std::string()}, r);
  });
}

ImportScan scanTemplates(const std::string& zip_path) {
  return import_call<ImportScan>([&](ImportScan& r) { scan_templates(ImportInput{-1, zip_path}, r); });
}

ImportResult installTemplates(int fd, const ImportSelection& selection, const ImportProgress& progress) {
  return import_call<ImportResult>([&](ImportResult& r) {
    if (fd < 0) fail(Status::InvalidArgument, "not a file descriptor");
    install_templates(ImportInput{fd, std::string()}, selection, false, progress, loading_mutex, r);
  });
}

ImportResult installTemplates(const std::string& zip_path, const ImportSelection& selection, const ImportProgress& progress) {
  return import_call<ImportResult>([&](ImportResult& r) {
    install_templates(ImportInput{-1, zip_path}, selection, false, progress, loading_mutex, r);
  });
}

ImportResult importTemplates(const std::string& zip_path, const ImportProgress& progress) {
  return import_call<ImportResult>([&](ImportResult& r) {
    install_templates(ImportInput{-1, zip_path}, ImportSelection(), true, progress, loading_mutex, r);
  });
}

} // namespace mse::facade
