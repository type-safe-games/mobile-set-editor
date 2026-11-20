//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/io/stream.hpp>
#include <wx/stream.h>

// ----------------------------------------------------------------------------- : Adapters

/// Reads a wxInputStream (from wx's web requests) as an MSE InputStream, for the core's Reader
class WxInputStreamReader : public InputStream {
public:
  explicit WxInputStreamReader(wxInputStream& in) : in(in) {
    if (!in.IsOk()) setError();
  }
protected:
  size_t doRead(void* buffer, size_t size) override {
    in.Read(buffer, size);
    size_t n = in.LastRead();
    if (n == 0 && !in.Eof() && in.GetLastError() != wxSTREAM_NO_ERROR) setError();
    return n;
  }
private:
  wxInputStream& in;
};
