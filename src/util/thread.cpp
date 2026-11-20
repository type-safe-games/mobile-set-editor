//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/thread.hpp>

// ----------------------------------------------------------------------------- : Core thread

// Static initialization runs on the main thread
static std::thread::id core_thread_id = std::this_thread::get_id();

bool is_core_thread() {
  return std::this_thread::get_id() == core_thread_id;
}

void set_core_thread() {
  core_thread_id = std::this_thread::get_id();
}
