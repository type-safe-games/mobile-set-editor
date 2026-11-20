//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/vcs.hpp>

// ----------------------------------------------------------------------------- : SubversionVCS

class SubversionVCS : public VCS {
public:
  void addFile (const String& filename) final;
  void moveFile (const String& source, const String& destination) final;
  void removeFile (const String& filename) final;
  
  DECLARE_REFLECTION();
};

