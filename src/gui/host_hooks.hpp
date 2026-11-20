//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>

// ----------------------------------------------------------------------------- : Host hooks

/// Install the desktop program's implementations of the core's host hooks
/** These are the places where the core needs the user interface or the platform:
 *  error popups, dialogs, downloads, running programs and system colors.
 */
void install_desktop_hooks();

/// Load the fonts (.ttf or .otf) in all directories in the app directory that contain "fonts" in their names,
/// and optionally their subdirectories, so wx can use them; returns true if there were errors.
/** These directories, and the ones in the MSE_FONT_STORE environment variable (separated like PATH),
 *  are the desktop's shared font store for the font registry.
 */
bool preload_resource_fonts(bool recursive);
