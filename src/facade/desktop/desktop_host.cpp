// Mobile Set Editor - the facade's desktop test host
// SPDX-License-Identifier: GPL-3.0-or-later

#include <util/prec.hpp>
#include <facade/desktop/desktop_host.hpp>
#include <gfx/image_codec.hpp>
#include <gfx/canvas.hpp>
#include <gfx/wx_image_codec.hpp>
#ifdef MSE_SKIA_CODEC
#include <gfx/skia/skia_image_codec.hpp>
#endif
#ifdef MSE_SKIA_CANVAS
#include <gfx/skia/skia_canvas.hpp>
#include <gfx/font_registry.hpp>
#endif
#include <gfx/wx_canvas.hpp>
#include <wx/app.h>
#include <wx/init.h>
#include <cstdio>
#include <cstdlib>
#include <set>

namespace mse::facade::desktop {

/// A wx application that only exists so GTK is initialised; it has no windows and no event loop
class HostApp : public wxApp {
public:
  bool OnInit() override { return true; }
};

/// Headless: a failed wx assertion is printed (once per place), never shown in a dialog nobody can close.
/** wxGTK 3.2 asserts on every Style font (wx_canvas.cpp, SetPixelSize), harmlessly. */
static void print_assert(const wxString& file, int line, const wxString& func, const wxString& cond, const wxString& msg) {
  static std::set<std::pair<wxString, int>> seen;
  if (!seen.insert(std::make_pair(file, line)).second) return;
  std::fprintf(stderr, "wx assertion failed: %s:%d in %s(): %s %s\n", (const char*)file.utf8_str(), line,
               (const char*)func.utf8_str(), (const char*)cond.utf8_str(), (const char*)msg.utf8_str());
  if (std::getenv("MSE_ABORT_ON_ASSERT")) std::abort();
}

bool start_host(int& argc, char** argv) {
  wxSetAssertHandler(print_assert);
  wxApp::SetInstance(new HostApp());
  if (!wxEntryStart(argc, argv)) return false;
  if (!wxTheApp->CallOnInit()) {
    wxEntryCleanup();
    return false;
  }
  wxInitAllImageHandlers();
#ifdef MSE_SKIA_CODEC
  set_image_codec(make_skia_image_codec()); // (CMake option MSE_SKIA_CODEC, #41)
#else
  set_image_codec(make_wx_image_codec());
#endif
#ifdef MSE_SKIA_CANVAS
  // (CMake option MSE_SKIA_CANVAS, #42) The desktop reference renders with the system's fonts as
  // well as the pack's, so the font registry gets them too
  set_canvas_factory(make_unique<SkiaCanvasFactory>());
  add_system_fonts_to_registry();
#else
  set_canvas_factory(make_unique<WxCanvasFactory>());
#endif
  return true;
}

void stop_host() {
  wxEntryCleanup();
}

} // namespace mse::facade::desktop
