//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/file_utils.hpp>

class VCS;

DECLARE_POINTER_TYPE(VCS);

template <>
void Reader::handle(VCSP& pointer);

// ----------------------------------------------------------------------------- : VCS

/// Interface to a version control system
/** This allows MSE to interact with various revision control systems directly
 *  rather than relying on the user to do so. Each method causes the external
 *  version control system to perform an operation on the specified file name.
 *  The default implementation just calls the normal file-handling functions.
 */
class VCS : public IntrusivePtrVirtualBase
{
public:
  /// Add a file - it's assumed to already have been created
  virtual void addFile (const String& filename) {
  }
  /// Rename a file (currently unused)
  /** Note: this works on the file names only (relative to the working directory), as it always has */
  virtual void moveFile (const String& source, const String& destination) {
    rename_file_or_dir(file_full_name(source), file_full_name(destination));
  }
  /// Delete a file right off the disk
  /** Note: this works on the file name only (relative to the working directory), as it always has */
  virtual void removeFile (const String& filename) {
    remove_file(file_full_name(filename));
  }
  
  DECLARE_REFLECTION_VIRTUAL();
};

