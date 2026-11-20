// Mobile Set Editor - the facade over the core
// SPDX-License-Identifier: GPL-3.0-or-later

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <facade/template_import.hpp>
#include <util/file_utils.hpp>
#include <util/version.hpp>
#include <util/io/zip.hpp>
#include <filesystem>
#include <fstream>

// Runs on any thread: only plain file operations and the zip reader, never the package manager,
// the error queue or anything else that belongs to the core thread.

namespace mse::facade {

namespace fs = std::filesystem;

// ----------------------------------------------------------------------------- : Template store

static std::mutex store_mutex;
static std::string store_templates, store_fonts;

void set_template_store(const std::string& template_dir, const std::string& font_store_dir) {
  std::lock_guard<std::mutex> lock(store_mutex);
  store_templates = template_dir;
  store_fonts     = font_store_dir;
}

// ----------------------------------------------------------------------------- : Helpers

/// The name parts of a path in a zip, or empty if it is unsafe (absolute, or has "..")
static vector<String> split_entry(const String& name) {
  vector<String> parts;
  size_t start = 0;
  while (start <= name.size()) {
    size_t end = name.find_first_of(_("/\\"), start);
    if (end == String::npos) end = name.size();
    String part = name.substr(start, end - start);
    if (part == _("..")) return vector<String>();
    if (!part.empty() && part != _(".")) parts.push_back(part);
    start = end + 1;
  }
  return parts;
}

/// "magic-m15.mse-style" -> "style": the name of the package's main data file
static String package_type(const String& name) {
  size_t pos = name.rfind(_(".mse-"));
  if (pos == String::npos || pos == 0 || pos + 5 >= name.size()) return String();
  return name.substr(pos + 5);
}

static bool is_font_file(const String& name) {
  String lower = to_lower(name);
  return ends_with(lower, _(".ttf")) || ends_with(lower, _(".otf"));
}

/// A top level "key: value" from a package's data file
static String header_value(const std::string& data, const char* key) {
  std::string k = std::string(key) + ":";
  size_t pos = 0;
  if (data.compare(0, 3, "\xEF\xBB\xBF") == 0) pos = 3; // byte order mark
  while (pos < data.size()) {
    size_t eol = data.find('\n', pos);
    if (eol == std::string::npos) eol = data.size();
    if (data.compare(pos, k.size(), k) == 0) {
      std::string v = data.substr(pos + k.size(), eol - pos - k.size());
      String value = decode_utf8(v);
      return trim(value);
    }
    pos = eol + 1;
  }
  return String();
}

static std::string read_file(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

static bool write_entry(const ZipReader& zip, const ZipEntry& entry, const fs::path& to) {
  std::error_code ec;
  fs::create_directories(to.parent_path(), ec);
  unique_ptr<InputStream> in = zip.openEntry(entry);
  if (!in) return false;
  std::string data = in->readAll();
  if (!in->ok()) return false;
  std::ofstream out(to, std::ios::binary | std::ios::trunc);
  out.write(data.data(), (std::streamsize)data.size());
  out.close();
  return !out.fail();
}

// ----------------------------------------------------------------------------- : Import

struct FoundPackage {
  String name;                 ///< e.g. "magic-m15.mse-style"
  size_t depth;                ///< number of path parts before the package directory
  String prefix;               ///< the path in the zip up to and including the package directory, '/' separated
  vector<const ZipEntry*> files;
  std::string data_file;       ///< contents of the main data file
};

void import_templates(const std::string& zip_path, std::mutex& loading_mutex, ImportResult& out) {
  std::string templates, fonts;
  {
    std::lock_guard<std::mutex> lock(store_mutex);
    templates = store_templates;
    fonts     = store_fonts;
  }
  if (templates.empty()) {
    out.status = Status::Failed;
    out.error = "the facade is not initialised (no template store)";
    return;
  }
  ZipReader zip(decode_utf8(zip_path));
  if (!zip.ok()) {
    out.status = Status::InvalidArgument;
    out.error = "not a zip file: " + zip_path;
    return;
  }
  // find the package directories (the outermost *.mse-* directory of each entry) and loose fonts
  vector<FoundPackage> packages;
  map<String, size_t> package_index; // name -> index in packages
  vector<pair<String, const ZipEntry*>> loose_fonts;
  for (const ZipEntry& entry : zip.entries()) {
    vector<String> parts = split_entry(entry.name);
    if (parts.empty()) continue;
    bool is_dir = ends_with(entry.name, _("/"));
    size_t pkg = parts.size();
    for (size_t i = 0 ; i + (is_dir ? 0 : 1) < parts.size() ; ++i) {
      if (!package_type(parts[i]).empty()) { pkg = i; break; }
    }
    if (pkg == parts.size()) {
      if (!is_dir && is_font_file(parts.back())) loose_fonts.emplace_back(parts.back(), &entry);
      continue;
    }
    String prefix;
    for (size_t i = 0 ; i <= pkg ; ++i) prefix += parts[i] + _("/");
    auto it = package_index.find(parts[pkg]);
    if (it == package_index.end()) {
      package_index[parts[pkg]] = packages.size();
      packages.push_back(FoundPackage{parts[pkg], pkg, prefix, {}, {}});
      it = package_index.find(parts[pkg]);
    }
    FoundPackage& p = packages[it->second];
    if (p.prefix != prefix) continue; // the same package elsewhere in the zip: the first one wins
    if (!is_dir) p.files.push_back(&entry);
    if (!is_dir && parts.size() == pkg + 2 && parts[pkg + 1] == package_type(p.name)) {
      unique_ptr<InputStream> in = zip.openEntry(entry);
      if (in) p.data_file = in->readAll();
    }
  }
  fs::path store = fs::u8path(templates);
  // install each package
  for (FoundPackage& p : packages) {
    ImportedPackage result;
    result.name = encode_utf8(p.name);
    String version = header_value(p.data_file, "version");
    result.version = encode_utf8(version);
    try {
      if (p.data_file.empty()) throw std::runtime_error("the package has no '" + encode_utf8(package_type(p.name)) + "' file");
      String mse_version = header_value(p.data_file, "mse version");
      if (!mse_version.empty() && Version::fromString(mse_version) > app_version) {
        throw std::runtime_error("the package needs a newer version of the program (mse version " + encode_utf8(mse_version) + ")");
      }
      fs::path target = store / to_fs_path(p.name);
      bool exists = fs::exists(target);
      if (exists) {
        // one version per package: only newer replaces
        String installed = header_value(read_file(target / to_fs_path(package_type(p.name))), "version");
        if (!(Version::fromString(version) > Version::fromString(installed))) {
          result.outcome = ImportedPackage::Outcome::SkippedNotNewer;
          out.packages.push_back(result);
          continue;
        }
      }
      // extract next to the target, then rename into place
      fs::path temp = store / to_fs_path(_(".") + p.name + _(".importing"));
      std::error_code ec;
      fs::remove_all(temp, ec);
      fs::create_directories(temp);
      for (const ZipEntry* e : p.files) {
        String rel = e->name.substr(p.prefix.size());
        vector<String> rel_parts = split_entry(rel);
        if (rel_parts.empty()) continue;
        fs::path to = temp;
        for (const String& part : rel_parts) to /= to_fs_path(part);
        if (!write_entry(zip, *e, to)) throw std::runtime_error("could not extract " + encode_utf8(e->name));
      }
      {
        // not while a session is loading packages
        std::lock_guard<std::mutex> lock(loading_mutex);
        fs::path old = store / to_fs_path(_(".") + p.name + _(".replaced"));
        fs::remove_all(old, ec);
        if (exists) fs::rename(target, old);
        fs::rename(temp, target);
        fs::remove_all(old, ec);
      }
      result.outcome = exists ? ImportedPackage::Outcome::Replaced : ImportedPackage::Outcome::Installed;
    } catch (const std::exception& e) {
      result.outcome = ImportedPackage::Outcome::Failed;
      result.error = e.what();
    }
    out.packages.push_back(result);
  }
  // loose fonts go to the shared font store
  if (!loose_fonts.empty() && !fonts.empty()) {
    fs::path font_store = fs::u8path(fonts);
    std::error_code ec;
    fs::create_directories(font_store, ec);
    for (auto& f : loose_fonts) {
      fs::path to = font_store / to_fs_path(f.first);
      if (fs::exists(to)) continue;
      fs::path temp = font_store / to_fs_path(_(".") + f.first + _(".importing"));
      if (write_entry(zip, *f.second, temp)) {
        fs::rename(temp, to, ec);
        if (!ec) out.fonts.push_back(encode_utf8(f.first));
      } else {
        fs::remove(temp, ec);
      }
    }
  }
}

} // namespace mse::facade
