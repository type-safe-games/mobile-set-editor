//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gui/value/editor.hpp>
#include <data/action/value.hpp>
#include <data/field/text.hpp>
#include <data/field/choice.hpp>
#include <data/field/multiple_choice.hpp>
#include <data/field/color.hpp>
#include <data/field/image.hpp>
#include <data/field/symbol.hpp>
#include <data/field/package_choice.hpp>
#include <data/field/information.hpp>

// ----------------------------------------------------------------------------- : Making editors

// defined by IMPLEMENT_VALUE_EDITOR in gui/value/*.cpp
#define VALUE_EDITOR_MAKER(Type) ValueViewerP make_##Type##_value_editor(DataEditor& parent, const StyleP& style);
VALUE_EDITOR_MAKER(Text)
VALUE_EDITOR_MAKER(Choice)
VALUE_EDITOR_MAKER(MultipleChoice)
VALUE_EDITOR_MAKER(Color)
VALUE_EDITOR_MAKER(Image)
VALUE_EDITOR_MAKER(Symbol)
VALUE_EDITOR_MAKER(PackageChoice)
VALUE_EDITOR_MAKER(Info)

ValueViewerP make_value_editor(DataEditor& parent, const StyleP& style) {
  // derived style types before their base (slider and boolean styles use the choice editor)
  Style* s = style.get();
  if (dynamic_cast<TextStyle*>(s))           return make_Text_value_editor(parent, style);
  if (dynamic_cast<MultipleChoiceStyle*>(s)) return make_MultipleChoice_value_editor(parent, style);
  if (dynamic_cast<ChoiceStyle*>(s))         return make_Choice_value_editor(parent, style);
  if (dynamic_cast<ColorStyle*>(s))          return make_Color_value_editor(parent, style);
  if (dynamic_cast<ImageStyle*>(s))          return make_Image_value_editor(parent, style);
  if (dynamic_cast<SymbolStyle*>(s))         return make_Symbol_value_editor(parent, style);
  if (dynamic_cast<PackageChoiceStyle*>(s))  return make_PackageChoice_value_editor(parent, style);
  if (dynamic_cast<InfoStyle*>(s))           return make_Info_value_editor(parent, style);
  throw InternalError(_("No editor for this kind of field"));
}

// ----------------------------------------------------------------------------- : ValueEditor

void ValueEditor::addAction(unique_ptr<ValueAction> a) {
  if (a) {
    a->setCard(editor().getCard());
    editor().addAction(move(a));
  }
}
