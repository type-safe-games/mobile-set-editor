//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/text_editing.hpp>
#include <data/action/value.hpp>
#include <data/word_list.hpp>

// ----------------------------------------------------------------------------- : Selection

// direction of a with respect to b
static Movement direction_of(size_t a, size_t b) {
  if (a < b) return MOVE_LEFT_OPT;
  if (a > b) return MOVE_RIGHT_OPT;
  else       return MOVE_MID;
}

void fix_text_selection(const String& val, TextSelection& sel, IndexType t, Movement dir) {
  // Which type takes precedent?
  if (t == TYPE_INDEX) {
    sel.start = index_to_cursor(val, sel.start_i, dir);
    sel.end   = index_to_cursor(val, sel.end_i,   dir);
  }
  // make sure the selection is at a valid position inside the text
  // prepare to move 'inward' (i.e. from start in the direction of end and vice versa)
  sel.start_i = cursor_to_index(val, sel.start, direction_of(sel.end, sel.start));
  sel.end_i   = cursor_to_index(val, sel.end,   direction_of(sel.start, sel.end));
  // start and end must be on the same side of separators
  size_t seppos = val.find(_("<sep"));
  while (seppos != String::npos) {
    size_t sepend = match_close_tag_end(val, seppos);
    if (sel.start_i <= seppos && sel.end_i > seppos) {
        // not on same side, move selection end before sep
      sel.end   = index_to_cursor(val, seppos, dir);
      sel.end_i = cursor_to_index(val, sel.end, direction_of(sel.start, sel.end));
    } else if (sel.start_i >= sepend && sel.end_i < sepend) {
        // not on same side, move selection end after sep
      sel.end   = index_to_cursor(val, sepend, dir);
      sel.end_i = cursor_to_index(val, sel.end, direction_of(sel.start, sel.end));
    }
    // find next separator
    seppos = val.find(_("<sep"), seppos + 1);
  }
}

// ----------------------------------------------------------------------------- : Words

static const Char word_bound_chars[] = _(" ,.:;()\n");
bool is_word_boundary_char(Char c) {
  return c == _(' ') || c == _(',') || c == _('.') || c == _(':') || c == _(';') ||
         c == _('(') || c == _(')') || c == _('\n') || isPunct(c);
}

size_t prev_word_boundary(const String& val, size_t pos_i) {
  size_t p = val.find_last_not_of(word_bound_chars, max(0, (int)pos_i - 1));
  if (p == String::npos) return 0;
  p = val.find_last_of(word_bound_chars, p);
  if (p == String::npos) return 0;
  return p + 1;
}
size_t next_word_boundary(const String& val, size_t pos_i) {
  size_t p = val.find_first_of(word_bound_chars, pos_i);
  if (p == String::npos) return val.size();
  p = val.find_first_not_of(word_bound_chars, p);
  if (p == String::npos) return val.size();
  return p;
}
bool is_word_boundary(const String& val, size_t pos_i) {
  // boundary after?
  size_t pos = pos_i;
  while (true) {
    if (pos >= val.size()) return true;
    Char c = val[pos];
    if (c == _('<')) pos = skip_tag(val,pos); // skip tags
    else if (is_word_boundary_char(c)) return true;
    else break;
  }
  // boundary before?
  pos = pos_i;
  while (true) {
    if (pos == 0) return true;
    Char c = val[pos - 1];
    if (c == _('>')) {
      // (try to) skip tags in reverse
      while (true) {
        if (pos == 0) return false; // not a tag
        --pos;
        c = val[pos - 1];
        if      (c == _('<')) { --pos; break; } // was a tag
        else if (c == _('>')) return false; // was not a tag
      }
    }
    else return is_word_boundary_char(c);
  }
}

// ----------------------------------------------------------------------------- : Caret re-matching

size_t match_cursor_position(size_t pos1, const String& text1, size_t pos2, const String& text2) {
  pos1 = min(pos1, text1.size());
  pos2 = min(pos2, text2.size());
  size_t score = 0; // total score
  // Match part before cursor
  size_t before1, before2;
  for (before1 = before2 = 0 ; before1 < pos1 && before2 < pos2 ; ++before1, ++before2) {
    Char c1 = text1[pos1-before1-1], c2 = text2[pos2-before2-1];
    if      (c1 == c2)                   score += 10000;
    else if (toLower(c1) == toLower(c2)) score +=  9999;
    else if (isPunct(c1) && isPunct(c2)) score +=     1;
    else if (c1 == UNTAG_ATOM_KWPPH)    {score +=     1; --before2;}
    else                                 break;
  }
  // bonus points for matching start of string
  if (pos1 == before1 && pos2 == before2) score += 10000;
  // Match part after cursor
  size_t after1, after2;
  for (after1 = after2 = 0 ; pos1 + after1 < text1.size() && pos2 + after2 < text2.size() ; ++after1, ++after2) {
    Char c1 = text1[pos1+after1], c2 = text2[pos2+after2];
    if      (c1 == c2)                   score += 10;
    else if (toLower(c1) == toLower(c2)) score +=  9;
    else if (isPunct(c1) && isPunct(c2)) score +=  1;
    else if (c2 == UNTAG_ATOM_KWPPH)    {score +=  1; --after1;}
    else                                 break;
  }
  // bonus points for matching end of string
  if (pos1+after1 == text1.size() && pos2+after2 == text2.size()) score += 2;
  // final score: matching 'before' is more important
  return score;
}

size_t rematch_cursor(const String& expected_value, size_t expected_cursor, const String& real_value) {
  // where real and expected value are the same, nothing has happend, so don't look there
  size_t start, end_min;
  for (start = 0 ; start < min(real_value.size(), expected_value.size()) ; ++start) {
    if (real_value[start] != expected_value[start]) break;
  }
  for (end_min = 0 ; end_min < min(real_value.size(), expected_value.size()) ; ++end_min) {
    if (real_value[real_value.size() - end_min - 1] !=
      expected_value[expected_value.size() - end_min - 1]) break;
  }
  // what is the best cursor position?
  size_t best_cursor = expected_cursor;
  if (real_value.size() < expected_value.size()
    && expected_cursor < expected_value.size()
    && start < real_value.size()
    && expected_value[expected_cursor] == UNTAG_SEP
    && real_value[start]               == UNTAG_SEP
    && real_value.size() - end_min == start) {
    // exception for type-over separators
    best_cursor = start + 1;
  } else {
    // try to find the best match to what text we expected to be around the cursor
    size_t best_match  = 0;
    size_t begin = min(start, expected_cursor);
    size_t end   = min(real_value.size() + 1, max(real_value.size() - end_min, expected_cursor) + 1);
    for (size_t i = begin ; i < end ; ++i) {
      size_t match = match_cursor_position(expected_cursor, expected_value, i, real_value);
      if (match > best_match || (match == best_match && abs((int)expected_cursor - (int)i) < abs((int)expected_cursor - (int)best_cursor))) {
        best_match = match;
        best_cursor = i;
      }
    }
  }
  return best_cursor;
}

// ----------------------------------------------------------------------------- : Typing

ReplaceResult replace_text_selection(const TextValueP& value, TextSelection& sel,
                                     const String& replacement, const String& name,
                                     bool select_on_undo, const AddValueAction& add) {
  if (replacement.empty() && sel.start == sel.end) {
    // no text selected, nothing to delete
    return ReplaceResult::NOTHING;
  }
  // fix the selection, it may be changed by undo/redo
  if (sel.end < sel.start) swap(sel.end, sel.start);
  fix_text_selection(value->value(), sel);
  // execute the action before adding it to the stack,
  // because we want to run scripts before action listeners see the action
  auto action = typing_action(value, sel.start_i, sel.end_i, select_on_undo ? sel.start : sel.end, sel.end, replacement, name);
  if (!action) {
    // nothing changes (the caller may want to move the selection anyway)
    return ReplaceResult::UNCHANGED;
  }
  // what we would expect if no scripts take place
  String expected_value  = untag_for_cursor(action->newValue());
  size_t expected_cursor = min(sel.start, sel.end) + untag_for_cursor(replacement).size();
  // perform the action
  add(std::move(action));
  // move cursor
  sel.end = sel.start = rematch_cursor(expected_value, expected_cursor, untag_for_cursor(value->value()));
  fix_text_selection(value->value(), sel, TYPE_CURSOR, MOVE_RIGHT);
  return ReplaceResult::CHANGED;
}

void auto_replace_text(const TextValueP& value, TextSelection& sel,
                       const vector<AutoReplaceP>& auto_replaces, const AddValueAction& add) {
  size_t end = sel.start_i;
  FOR_EACH_CONST(ar, auto_replaces) {
    if (ar->enabled && ar->match.size() <= end) {
      size_t start = end - ar->match.size();
      if (is_substr(value->value(), start, ar->match) &&
          (!ar->whole_word || (is_word_boundary(value->value(), start) && is_word_boundary(value->value(), end)))
         ) {
        // replace
        sel.start_i = start;
        sel.end_i   = end;
        fix_text_selection(value->value(), sel, TYPE_INDEX);
        replace_text_selection(value, sel, ar->replace, _ACTION_("auto replace"), false, add);
        end = sel.start_i;
      }
    }
  }
}

// ----------------------------------------------------------------------------- : Visible text

VisibleText visible_text(const String& str) {
  VisibleText out;
  out.text.reserve(str.size());
  // one atom: its untagged contents, locked
  auto add_atom = [&](size_t i) -> size_t {
    size_t close = match_close_tag(str, i);
    size_t open_end = min(skip_tag(str, i), close);
    size_t end = match_close_tag_end(str, i);
    String shown = close == String::npos ? String() : untag(str.substr(open_end, close - open_end));
    out.cursor_pos.push_back(out.text.size());
    if (!shown.empty()) out.locked.emplace_back(out.text.size(), out.text.size() + shown.size());
    out.text += shown;
    return end;
  };
  // the same walk as untag_for_cursor, so there is one entry per cursor position
  for (size_t i = 0 ; i < str.size() ; ) {
    Char c = str[i];
    if (c == _('<')) {
      if (is_substr(str, i, _("<atom")) || is_substr(str, i, _("<sep")) || is_substr(str, i, _("<bullet")) ||
          is_substr(str, i, _("<soft-line")) || is_substr(str, i, _("<line"))) {
        i = add_atom(i);
      } else if (i == 0 && is_substr(str, i, _("<prefix"))) {
        i = match_close_tag_end(str,i);
      } else if (is_substr(str, i, _("<suffix")) && match_close_tag_end(str,i) >= str.size()) {
        i = str.size();
      } else {
        i = skip_tag(str, i);
      }
    } else {
      out.cursor_pos.push_back(out.text.size());
      out.text += untag_char(c);
      ++i;
    }
  }
  out.cursor_pos.push_back(out.text.size());
  return out;
}

size_t VisibleText::toCursor(size_t visible_pos) const {
  visible_pos = min(visible_pos, text.size());
  // the last cursor position at or before visible_pos
  size_t c = upper_bound(cursor_pos.begin(), cursor_pos.end(), visible_pos) - cursor_pos.begin();
  if (c == 0) return 0;
  --c;
  if (cursor_pos[c] == visible_pos || c + 1 >= cursor_pos.size()) return c;
  // inside an atom: the nearest edge
  return (visible_pos - cursor_pos[c]) * 2 > (cursor_pos[c+1] - cursor_pos[c]) ? c + 1 : c;
}

size_t VisibleText::toVisible(size_t cursor) const {
  if (cursor_pos.empty()) return 0;
  return cursor_pos[min(cursor, cursor_pos.size() - 1)];
}
