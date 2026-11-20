//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gui/insert_symbol_menu.hpp>
#include <data/symbol_font.hpp>
#include <gui/window_id.hpp>

// ----------------------------------------------------------------------------- : Building the menu

static wxMenu* make_menu(const InsertSymbolMenu& menu, int id, SymbolFont& font);

static wxMenuItem* make_menu_item(const InsertSymbolMenu& item, wxMenu* parent, int first_id, SymbolFont& font) {
  String label = item.label.get();
  // ensure that we are not defining an accelerator...
  // everything after a tab is considered to be an accelerator by wxMenuItem
  int accel_pos = label.find_last_of('\t');
  if (accel_pos != label.npos && accel_pos > 0) {
    String accel = label.substr(accel_pos+1);
#ifdef __WXMSW__
    // if there is a + or - in the accelerator, replace the tab with spaces (simply adding a space does not work)
    if (accel.Contains("+") || accel.Contains("-")) {
      label = label.substr(0, accel_pos) + _("      ") + accel;
    }
    // otherwise simply add a space after the tab if there isn't one
    else {
      label.Replace(_("\t "), _("\t"));
      label.Replace(_("\t"), _("\t "));
    }
#else
    label = label.substr(0, accel_pos) + _("      ") + accel; // replace the tab with spaces
#endif
  }
  if (item.type == InsertSymbolMenu::Type::SUBMENU) {
    wxMenuItem* menu_item = new wxMenuItem(parent, wxID_ANY, label,
                                           _(""), wxITEM_NORMAL,
                                           make_menu(item, first_id, font));
    menu_item->SetBitmap(wxNullBitmap);
    return menu_item;
  } else if (item.type == InsertSymbolMenu::Type::LINE) {
    return new wxMenuItem(parent, wxID_SEPARATOR);
  } else {
    wxMenuItem* menu_item = new wxMenuItem(parent, first_id, label);
    // Generate bitmap for use on this item
    menu_item->SetBitmap(font.insertSymbolPreview(item, wxSize(16,16)));
    return menu_item;
  }
}

static wxMenu* make_menu(const InsertSymbolMenu& menu, int id, SymbolFont& font) {
  if (menu.type == InsertSymbolMenu::Type::SUBMENU) {
    wxMenu* result = new wxMenu();
    FOR_EACH_CONST(i, menu.items) {
      result->Append(make_menu_item(*i, result, id, font));
      id += i->size();
    }
    return result;
  }
  return nullptr;
}

wxMenu* insert_symbol_menu(SymbolFont& font, Context& ctx) {
  const InsertSymbolMenu* menu = font.insertSymbolMenu();
  if (!font.insert_symbol_menu_ui && menu) {
    font.update(ctx);
    wxMenu* built = make_menu(*menu, ID_INSERT_SYMBOL_MENU_MIN, font);
    if (built) {
      font.insert_symbol_menu_ui = shared_ptr<void>(built, [](void* m) { delete static_cast<wxMenu*>(m); });
    }
  }
  return static_cast<wxMenu*>(font.insert_symbol_menu_ui.get());
}

// ----------------------------------------------------------------------------- : Handling a choice

String insert_symbol_code(const SymbolFont& font, int menu_id) {
  const InsertSymbolMenu* menu = font.insertSymbolMenu();
  if (!menu) return _("");
  const InsertSymbolMenu* item = menu->itemAt(menu_id - ID_INSERT_SYMBOL_MENU_MIN);
  if (!item) return String();
  if (item->type == InsertSymbolMenu::Type::CUSTOM) {
    String title = item->label.get();
    title = replace_all(title, _("&"), _("")); // remove underlines
    return wxGetTextFromUser(item->prompt.get(), title).ToStdWstring();
  }
  return item->name;
}
