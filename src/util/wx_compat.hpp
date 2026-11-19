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

#include <wx/stream.h>

// wx 3.3 reads/writes a PNG tEXt "Description" chunk through this image option.
// On 3.2 the PNG handler ignores it, so the metadata still flows between in-memory
// images but is not saved to or loaded from PNG files.
#define wxIMAGE_OPTION_PNG_DESCRIPTION wxS("PngDescription")

// wx 3.3 dark-mode switch (wxApp::SetAppearance). Unqualified calls inside wxApp
// members find this free function because wx 3.2's wxApp has no such member.
enum Appearance { Appearance_System, Appearance_Light, Appearance_Dark };
inline void SetAppearance(Appearance) {}

// wx 3.3 synchronous web requests. Network features (installer list, package
// icons, updater) are out of scope for the test host, so every request fails.
class wxWebRequestSync {
public:
  struct Result {
    explicit operator bool() const { return false; }
  };
  struct Response {
    wxInputStream* GetStream() const { return nullptr; }
  };
  Result Execute() { return {}; }
  Response GetResponse() const { return {}; }
};

class wxWebSessionSync {
public:
  static wxWebSessionSync& GetDefault() {
    static wxWebSessionSync session;
    return session;
  }
  wxWebRequestSync CreateRequest(const wxString&) { return {}; }
};

#endif
