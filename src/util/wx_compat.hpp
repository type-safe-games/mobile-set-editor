//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : wx 3.3 -> 3.2 shim
//
// MSE 2.6 targets wxWidgets >= 3.3.3. This fork only keeps a desktop build as a
// test host (ADR 0007), so instead of reverting 2.6 code it builds against the
// system wx 3.2 with these stand-ins. They are deliberately inert: nothing here
// tries to reach desktop parity.

#include <wx/version.h>

#if !wxCHECK_VERSION(3,3,0)

// wx 3.3 dark-mode switch (wxApp::SetAppearance). Unqualified calls inside wxApp
// members find this free function because wx 3.2's wxApp has no such member.
enum Appearance { Appearance_System, Appearance_Light, Appearance_Dark };
inline void SetAppearance(Appearance) {}

#endif
