// Mobile Set Editor - the facade tests on the desktop host
// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_host.hpp"
#include <facade/desktop/desktop_host.hpp>

namespace test_host {

bool start(int& argc, char** argv) { return mse::facade::desktop::start_host(argc, argv); }
void stop() { mse::facade::desktop::stop_host(); }

bool draws_like_goldens() {
  #ifdef MSE_SKIA_CANVAS
    return false; // the desktop host then installs the Skia canvas
  #else
    return true;
  #endif
}

} // namespace test_host
