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
#include <data/stylesheet.hpp>
#include <data/locale.hpp>
#include <data/word_list.hpp>
#include <data/text_editing.hpp>
#include <data/action/value.hpp>
#include <data/action/set.hpp>
#include <data/field/text.hpp>
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
#include <atomic>
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

  void onAction(const Action& action, bool undone) override {
    if (!collecting || stale.all) return;
    if (const ScriptValueEvent* e = dynamic_cast<const ScriptValueEvent*>(&action)) {
      if (e->card) addCard(e->card->uid); else stale.all = true;
    } else if (const ValueAction* a = dynamic_cast<const ValueAction*>(&action)) {
      // a value without card is set info or style options: shown on every card
      if (a->card) addCard(a->card->uid); else stale.all = true;
    } else if (const OneWayLinkCardsAction* a = dynamic_cast<const OneWayLinkCardsAction*>(&action)) {
      addCard(a->card->uid);
    } else if (dynamic_cast<const BulkAction*>(&action) || dynamic_cast<const ScriptStyleEvent*>(&action)) {
      // a BulkAction's parts are reported one by one
      // a ScriptStyleEvent comes from drawing, not from an action
    } else {
      // display changes, card list changes, keywords, ...
      stale.all = true;
    }
  }

private:
  void addCard(const String& uid) {
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
  } catch (...) {
    s.stale.collecting = false;
    throw;
  }
  ++s.generation;
  end_action(s, r);
}

// ----------------------------------------------------------------------------- : Initialisation

static std::string template_dir_, font_store_dir_; // read by importTemplates, set before it may run

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
    if (!options.font_store_dir.empty()) {
      font_registry().addDirectory(from_utf8(options.font_store_dir), FontSource::FONT_STORE);
    }
    the_locale = options.locale.empty() ? LocaleP() : Locale::byName(from_utf8(options.locale));
    template_dir_   = options.template_dir;
    font_store_dir_ = options.font_store_dir;
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
    if (!template_dir_.empty())   font_registry().addTemplateDirectory(from_utf8(template_dir_));
    if (!font_store_dir_.empty()) font_registry().addDirectory(from_utf8(font_store_dir_), FontSource::FONT_STORE);
  });
}

// ----------------------------------------------------------------------------- : Sessions

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
      if (!working_copy_dir.empty()) {
        // the working copy is a directory, so autosaving never rewrites a whole zip
        String work = from_utf8(working_copy_dir);
        if (file_exists(work) || dir_exists(work)) fail(Status::InvalidArgument, "the working copy '" + working_copy_dir + "' already exists");
        set->saveAs(work, false, true);
        s->working_copy = work;
      }
      s->set = set;
    }
    s->set->actions.addListener(&s->stale);
    r.session = next_session++;
    sessions[r.session] = std::move(s);
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
      for (int i = 0 ; i < Card::MAX_LINKS ; ++i) {
        const String& uid = c->getLinkedUID(i);
        if (!uid.empty()) info.links.push_back(CardLinkInfo{to_utf8(uid), to_utf8(c->getLinkedRelation(i))});
      }
      r.cards.push_back(std::move(info));
    }
  });
}

// ----------------------------------------------------------------------------- : Fields and values

/// A value a field call is about
struct FieldRef {
  ValueP value;
  CardP  card;  ///< for card values, so the action knows its card
  StyleP style; ///< the field's style, if there is one
};

static FieldRef find_field(Session& s, const Target& t, const std::string& name) {
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
  }
  String n = from_utf8(name);
  for (const ValueP& v : *values) {
    if (v->fieldP->name == n) {
      ref.value = v;
      if (styles && v->fieldP->index < styles->size()) ref.style = styles->at(v->fieldP->index);
      return ref;
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
  return info;
}

FieldList listFields(SessionHandle h, const Target& t) {
  return guarded<FieldList>(t.card_uid, "", [&](FieldList& r) {
    Session& s = get_session(h);
    Set& set = *s.set;
    IndexMap<FieldP, ValueP>* values = nullptr;
    switch (t.kind) {
      case Target::Kind::Card:      values = &find_card(set, t.card_uid)->data; break;
      case Target::Kind::Set:       values = &set.data; break;
      case Target::Kind::CardStyle: values = &set.stylingDataFor(find_card(set, t.card_uid)); break;
      case Target::Kind::SetStyle:  values = &set.stylingDataFor(*set.stylesheet); break;
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
    FieldRef ref = find_field(s, t, field);
    if (!ref.value->fieldP->editable) fail(Status::InvalidArgument, "field '" + field + "' can't be edited");
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
    if (!tf.editable) fail(Status::InvalidArgument, "field '" + field + "' can't be edited");
    if (start < 0 || end < 0) fail(Status::InvalidArgument, "negative text position");
    // the replacement as tagged text
    String plain = from_utf8(text);
    if (!tf.multi_line) plain.erase(std::remove(plain.begin(), plain.end(), _('\n')), plain.end());
    String replacement = escape(plain);
    if (as_symbol && !plain.empty()) {
      const TextStyle* style = dynamic_cast<const TextStyle*>(ref.style.get());
      if (!(style && style->always_symbol)) replacement = _("<sym>") + replacement + _("</sym>");
    }
    // visible text positions -> cursor positions
    VisibleText vt = visible_text(tv->value());
    TextSelection sel;
    sel.start = vt.toCursor((size_t)start);
    sel.end   = vt.toCursor((size_t)end);
    fix_text_selection(tv->value(), sel);
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
    for (const CardLinkP& link : s.set->game->card_links) {
      r.types.push_back(LinkType{to_utf8(link->selected.default_), to_utf8(link->linked.default_)});
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

// ----------------------------------------------------------------------------- : Template import

ImportResult importTemplates(const std::string& zip_path) {
  ImportResult r;
  try {
    // wait while a session is loading packages
    import_templates(zip_path, loading_mutex, r);
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

} // namespace mse::facade
