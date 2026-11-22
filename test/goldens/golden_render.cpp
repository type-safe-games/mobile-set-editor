// Mobile Set Editor - render Sets to Layout dumps and PNGs through the facade, for the Golden images
// SPDX-License-Identifier: GPL-3.0-or-later
//
//   golden_render --templates <dir> [--font-store <dir>] [--program-data <dir> --locale <name>]
//                 <set> <out dir> [<set> <out dir> ...]
//
// For every Card of every Set (in the Set's order, numbered from 0) it writes <out dir>/<n>.json,
// the Layout dump (ADR 0010), and <out dir>/<n>.png, the exported image: what tools/goldens gets
// from `magicseteditor --cli` with write_layout_file and write_image_file. It runs on the wx-free
// Skia host (test/facade/skia_host.cpp), as on Android (#44); MSE_SYSTEM_FONT_DIRS names the
// system fonts the reference rendered with. Each Set's messages go to <out dir>/render.log, as
// "ERROR: ..." / "WARNING: ..." lines like the CLI's, then "status: <exit code>".

#include <util/prec.hpp>
#include <facade/facade.hpp>
#include "../facade/test_host.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace F = mse::facade;
namespace fs = std::filesystem;

namespace {

/// Write a call's messages to the log; true if one was an error
bool log_messages(std::ofstream& log, const F::Result& r) {
  bool error = !r.ok();
  for (const F::Message& m : r.messages) {
    if (m.severity == F::Severity::Error || m.severity == F::Severity::Fatal) {
      log << "ERROR: " << m.text << "\n";
      error = true;
    } else if (m.severity == F::Severity::Warning) {
      log << "WARNING: " << m.text << "\n";
    }
  }
  if (!r.ok()) log << "ERROR: " << r.error << "\n";
  return error;
}

bool write_file(const fs::path& path, const void* data, size_t size) {
  std::ofstream out(path, std::ios::binary);
  out.write((const char*)data, (std::streamsize)size);
  return (bool)out;
}

/// Render one Set; the exit code the CLI would have had (0 = ok)
int render_set(const std::string& set_path, const fs::path& out_dir) {
  fs::remove_all(out_dir);
  fs::create_directories(out_dir);
  std::ofstream log(out_dir / "render.log");
  bool error = false;
  F::OpenResult open = F::openSession(set_path, "");
  error |= log_messages(log, open);
  if (!open.ok()) {
    log << "status: 1\n";
    return 1;
  }
  F::CardList cards = F::listCards(open.session);
  error |= log_messages(log, cards);
  for (size_t n = 0 ; n < cards.cards.size() ; ++n) {
    const std::string& uid = cards.cards[n].uid;
    F::TextOutput dump = F::layoutDump(open.session, uid);
    error |= log_messages(log, dump);
    if (dump.ok()) write_file(out_dir / (std::to_string(n) + ".json"), dump.text.data(), dump.text.size());
    // as write_image_file(card, file: ...) with its defaults: natural size, upright, no bleed, one face
    F::ExportOptions export_options;
    export_options.zoom = 1.0;
    export_options.angle_degrees = 0;
    export_options.bleed = false;
    export_options.both_faces = false;
    F::BytesResult png = F::exportCardImage(open.session, uid, export_options);
    error |= log_messages(log, png);
    if (png.ok()) write_file(out_dir / (std::to_string(n) + ".png"), png.data.data(), png.data.size());
  }
  F::closeSession(open.session);
  int status = error ? 1 : 0;
  log << "status: " << status << "\n";
  return status;
}

} // namespace

int main(int argc, char** argv) {
  F::InitOptions options;
  int i = 1;
  for (; i + 1 < argc && argv[i][0] == '-' && argv[i][1] == '-' ; i += 2) {
    std::string opt = argv[i];
    if (opt == "--templates") options.template_dir = argv[i + 1];
    else if (opt == "--font-store") options.font_store_dir = argv[i + 1];
    else if (opt == "--program-data") options.program_data_dir = argv[i + 1];
    else if (opt == "--locale") options.locale = argv[i + 1];
    else { std::fprintf(stderr, "unknown option %s\n", argv[i]); return 2; }
  }
  if (options.template_dir.empty() || (argc - i) < 2 || (argc - i) % 2 != 0) {
    std::fprintf(stderr, "usage: %s --templates <dir> [--font-store <dir>] [--program-data <dir> --locale <name>] <set> <out dir> [<set> <out dir> ...]\n", argv[0]);
    return 2;
  }
  if (!test_host::start(argc, argv)) {
    std::fprintf(stderr, "could not start the host\n");
    return 1;
  }
  F::Result init = F::init(options);
  if (!init.ok()) {
    std::fprintf(stderr, "init failed: %s\n", init.error.c_str());
    return 1;
  }
  int failed = 0;
  for (; i + 1 < argc ; i += 2) {
    int status = render_set(argv[i], argv[i + 1]);
    std::printf("%s %s\n", status == 0 ? "ok   " : "error", argv[i]);
    std::fflush(stdout);
    if (status) ++failed;
  }
  F::shutdown();
  test_host::stop();
  return failed ? 1 : 0;
}
