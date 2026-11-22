// Mobile Set Editor - the host the facade tests run on
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

/** @file test/facade/test_host.hpp
 *
 *  What the facade tests need from their host: something that installs an image codec and a
 *  canvas factory before mse::facade::init (ADR 0009). Two implementations:
 *   - wx_host.cpp: the desktop's wx backends (or the Skia ones with the CMake options), under
 *     xvfb-run, through facade/desktop/desktop_host.hpp;
 *   - skia_host.cpp: the wx-free Skia backends, as on Android (#43). It adds the font files in
 *     MSE_SYSTEM_FONT_DIRS to the font registry (the tests' fonts, like fontconfig's on desktop).
 */

namespace test_host {

/// Start the host; false if it could not start
bool start(int& argc, char** argv);
/// Shut the host down again
void stop();
/// Does the host draw with the canvas the Golden images were made with (wx), so that renders
/// must match them pixel for pixel? Otherwise only their Layout dump must match (ADR 0010).
bool draws_like_goldens();

} // namespace test_host
