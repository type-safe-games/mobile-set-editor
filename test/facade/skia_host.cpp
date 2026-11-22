// Mobile Set Editor - the facade tests on a wx-free host with the Skia backends
// SPDX-License-Identifier: GPL-3.0-or-later

#include <util/prec.hpp>
#include "test_host.hpp"
#include <gfx/image_codec.hpp>
#include <gfx/canvas.hpp>
#include <gfx/font_registry.hpp>
#include <gfx/skia/skia_image_codec.hpp>
#include <gfx/skia/skia_canvas.hpp>

namespace test_host {

bool start(int&, char**) {
  set_image_codec(make_skia_image_codec());
  set_canvas_factory(make_unique<SkiaCanvasFactory>());
  add_system_fonts_to_registry(); // MSE_SYSTEM_FONT_DIRS: the tests' fonts
  return true;
}

void stop() {}

bool draws_like_goldens() { return false; }

} // namespace test_host
