//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

/** @file util/thread.hpp
 *  The core thread: sets, scripts and packages may only be changed from one thread.
 */

// ----------------------------------------------------------------------------- : Includes

#include <thread>

// ----------------------------------------------------------------------------- : Core thread

/// Is this the thread that owns the core?
/** On desktop that is the main thread. A host that runs the core on another thread
 *  calls set_core_thread() from that thread before using the core.
 */
bool is_core_thread();

/// Make the calling thread the one that owns the core
void set_core_thread();
