//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

/** @file data/text_editing.hpp
 *
 *  Typing in a TextValue, without a GUI (ADR 0009): the selection and its constraints, replacing
 *  the selection as one undoable typing action, finding the caret back after scripts rewrote the
 *  value, and auto-replace. The desktop TextValueEditor and the facade both use this.
 */

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/tagged_string.hpp>

DECLARE_POINTER_TYPE(TextValue);
DECLARE_POINTER_TYPE(AutoReplace);
class ValueAction;

// ----------------------------------------------------------------------------- : Selection

enum IndexType
{  TYPE_CURSOR  ///< Positions are cursor positions
,  TYPE_INDEX  ///< Positions are character indices
};

/// A selection in a tagged string, both as cursor positions and as character indices
struct TextSelection {
  size_t start   = 0, end   = 0; ///< cursor positions (see index_to_cursor)
  size_t start_i = 0, end_i = 0; ///< character indices into the tagged string
};

/// Make sure the selection satisfies its constraints
/** - selection start and end are inside the text
 *  - not inside tags
 *  - the selection does not contain a <sep> or </sep> tag
 *
 *  t says which of the two kinds of positions is leading; when correcting the selection,
 *  move in the given direction.
 */
void fix_text_selection(const String& value, TextSelection& sel, IndexType t = TYPE_CURSOR, Movement dir = MOVE_MID);

// ----------------------------------------------------------------------------- : Words

bool is_word_boundary_char(Char c);
/// Front of the previous word (character indices), used with Ctrl+Left
size_t prev_word_boundary(const String& value, size_t pos_i);
/// Front of the next word (character indices), used with Ctrl+Right
size_t next_word_boundary(const String& value, size_t pos_i);
/// Is there a word boundary at the character index pos_i? (tags are skipped)
bool is_word_boundary(const String& value, size_t pos_i);

// ----------------------------------------------------------------------------- : Caret re-matching

/// Compare two cursor positions, determine how much the text matches before and after
size_t match_cursor_position(size_t pos1, const String& text1, size_t pos2, const String& text2);

/// Where the caret belongs after an edit that scripts may have rewritten.
/** expected_value/expected_cursor are the untag_for_cursor value and caret if no script had run,
 *  real_value is the untag_for_cursor value as it is now. Returns a cursor position in real_value.
 */
size_t rematch_cursor(const String& expected_value, size_t expected_cursor, const String& real_value);

// ----------------------------------------------------------------------------- : Typing

enum class ReplaceResult
{  NOTHING    ///< nothing to do (empty replacement for an empty selection), the selection is untouched
,  UNCHANGED  ///< the value would not change; no action was added, the selection was fixed
,  CHANGED    ///< an action was added, and the selection is the caret after the new text
};

/// Adds a value action to the set's action stack (performing it)
typedef function<void(unique_ptr<ValueAction>)> AddValueAction;

/// Replace the selection with a (tagged, already escaped) replacement, as typing does.
/** The edit is a typing_action named action_name, so consecutive edits with the same name merge into
 *  one undo step. After the action is added (and scripts have run), the caret is put where the
 *  text around it best matches what was expected.
 */
ReplaceResult replace_text_selection(const TextValueP& value, TextSelection& sel,
                                     const String& replacement, const String& action_name,
                                     bool select_on_undo, const AddValueAction& add);

/// Apply the enabled auto replacements that end at the start of the selection (after typing)
void auto_replace_text(const TextValueP& value, TextSelection& sel,
                       const vector<AutoReplaceP>& auto_replaces, const AddValueAction& add);

// ----------------------------------------------------------------------------- : Visible text

/// The text of a value as an editor without the card shows it.
/** There is one cursor position per character of untag_for_cursor(value): ordinary characters show
 *  as themselves, an atom (<atom>, <sep>, <bullet>, <line>, <soft-line>) shows as its untagged
 *  contents and can't be edited inside. A leading <prefix> and trailing <suffix> are not shown.
 */
struct VisibleText {
  String text;                           ///< what is shown
  vector<size_t> cursor_pos;             ///< for each cursor position (0 ... #cursor positions), its offset in text
  vector<pair<size_t,size_t>> locked;    ///< [start,end) ranges of text that are one atom
  vector<pair<size_t,size_t>> separators; ///< those of them that are separators (<sep>), between the parts of a combined field

  /// The cursor position for an offset in text, a position inside an atom snaps to its nearest edge
  size_t toCursor(size_t visible_pos) const;
  /// The offset in text of a cursor position
  size_t toVisible(size_t cursor) const;
};

VisibleText visible_text(const String& value);
