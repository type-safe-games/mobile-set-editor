// Mobile Set Editor - the facade's desktop test host
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

/** @file facade/desktop/desktop_host.hpp
 *
 *  The desktop host for the facade: what the Android host will do with Skia (ADR 0004), done here
 *  with the wx backends of the desktop build, so the facade can be tested headless on Linux.
 *
 *  It starts wxWidgets without a main window (GTK needs a display: run under xvfb-run) and
 *  installs the wx image codec and canvas factory. Call it before mse::facade::init.
 *  Not part of the facade itself; Android never compiles it.
 */

namespace mse::facade::desktop {

/// Start wx and install the wx image codec and canvas factory. False if wx could not start.
bool start_host(int& argc, char** argv);

/// Shut wx down again
void stop_host();

} // namespace mse::facade::desktop
