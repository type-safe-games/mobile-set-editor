// Mobile Set Editor - the facade over the core
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

/** @file facade/template_import.hpp
 *  Template import (ADR 0005), the facade calls that may run off the core thread.
 *  Internal to the facade.
 */

#include <facade/facade.hpp>
#include <functional>
#include <mutex>
#include <string>

namespace mse::facade {

/// Where imports go (set by init on the core thread)
void set_template_store(const std::string& template_dir, const std::string& font_store_dir);

/// The zip to import from: a file descriptor if fd >= 0, otherwise a file
struct ImportInput {
  int fd = -1;
  std::string path;
};

/// Scan a zip for Templates and loose fonts
void scan_templates(const ImportInput& input, ImportScan& out);

/// Install Templates and loose fonts from a zip; renames happen while holding loading_mutex.
/** With install_all, every package the scan would install is selected. */
void install_templates(const ImportInput& input, const ImportSelection& selection, bool install_all,
                       const ImportProgress& progress, std::mutex& loading_mutex, ImportResult& out);

/// For tests: called with the path of every file import is about to write; returning false makes
/// that write fail (as a full disk would).
extern std::function<bool(const std::string& path)> import_write_hook;

} // namespace mse::facade
