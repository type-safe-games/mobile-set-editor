//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : wx 3.3 -> 3.2 shim for the GUI
//
// Moved here from util/wx_compat.hpp, it is only used by the GUI (see there).

#include <wx/version.h>

#if !wxCHECK_VERSION(3,3,0)

#include <wx/stream.h>

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
