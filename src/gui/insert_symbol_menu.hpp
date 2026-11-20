//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>

class SymbolFont;
class Context;

// ----------------------------------------------------------------------------- : Insert symbol menu

/// The 'insert symbol' menu of a symbol font, or nullptr if the font has none.
/** The font owns the menu; it is built once and then cached.
 *  All ids used are in the range ID_INSERT_SYMBOL_MENU_MIN...ID_INSERT_SYMBOL_MENU_MAX.
 */
wxMenu* insert_symbol_menu(SymbolFont& font, Context& ctx);

/// The code to insert for a choice from the insert symbol menu, asks the user for custom symbols
String insert_symbol_code(const SymbolFont& font, int menu_id);
