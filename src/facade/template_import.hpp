// Mobile Set Editor - the facade over the core
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

/** @file facade/template_import.hpp
 *  Template import (ADR 0005), the one facade call that may run off the core thread.
 *  Internal to the facade.
 */

#include <facade/facade.hpp>
#include <mutex>
#include <string>

namespace mse::facade {

/// Where imports go (set by init on the core thread)
void set_template_store(const std::string& template_dir, const std::string& font_store_dir);

/// Install the packages and loose fonts in a zip; renames happen while holding loading_mutex
void import_templates(const std::string& zip_path, std::mutex& loading_mutex, ImportResult& out);

} // namespace mse::facade
