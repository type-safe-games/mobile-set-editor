// Mobile Set Editor - the facade over the core
// SPDX-License-Identifier: GPL-3.0-or-later

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <facade/template_import.hpp>
#include <data/installer.hpp>
#include <util/file_utils.hpp>
#include <util/version.hpp>
#include <util/io/zip.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>

// Runs on any thread: only plain file operations, the zip reader and plain data (the installer's
// PackageDescription, Version), never the package manager, the error queue or anything else that
// belongs to the core thread.

namespace mse::facade {

namespace fs = std::filesystem;

std::function<bool(const std::string& path)> import_write_hook;

// ----------------------------------------------------------------------------- : Template store

static std::mutex store_mutex;
static std::string store_templates, store_fonts;

void set_template_store(const std::string& template_dir, const std::string& font_store_dir) {
  std::lock_guard<std::mutex> lock(store_mutex);
  store_templates = template_dir;
  store_fonts     = font_store_dir;
}

/// One import at a time, so an import can clean up what a crashed one left behind
static std::mutex import_mutex;

/// Temporary directories in the store start with this; they are never Templates (no .mse-*)
static const char* const TEMP_PREFIX = ".importing-";

/// A refused or failed package
struct ImportError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

/// The progress callback asked to stop
struct ImportCancelled {};

/// Counts extracted bytes for the progress callback
struct ProgressCounter {
  const ImportProgress& progress;
  uint64_t done = 0, total = 0;
  /// Some bytes were extracted; throws ImportCancelled if the callback says to stop
  void add(uint64_t bytes) {
    done += bytes;
    if (progress && !progress(done, total)) throw ImportCancelled();
  }
};

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

/// Is this a Template type? (Sets and symbols are not, installers are containers)
static bool is_template_type(const String& type) {
  return !type.empty() && type != _("set") && type != _("symbol") && type != _("installer");
}

static bool is_font_file(const String& name) {
  String lower = to_lower(name);
  return ends_with(lower, _(".ttf")) || ends_with(lower, _(".otf"));
}

/// A spelling dictionary, in a "dictionaries" directory as in MSE's data directory
static bool is_dictionary_file(const vector<String>& parts) {
  String lower = to_lower(parts.back());
  return parts.size() >= 2 && to_lower(parts[parts.size() - 2]) == _("dictionaries")
      && (ends_with(lower, _(".dic")) || ends_with(lower, _(".aff")));
}

static std::string read_file(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

static std::string read_entry(const ZipReader& zip, const ZipEntry& entry, bool& ok) {
  unique_ptr<InputStream> in = zip.openEntry(entry);
  ok = false;
  if (!in) return std::string();
  std::string data = in->readAll();
  ok = in->ok();
  return data;
}

static bool write_file(const fs::path& to, const std::string& data) {
  if (import_write_hook && !import_write_hook(to.u8string())) return false;
  std::ofstream out(to, std::ios::binary | std::ios::trunc);
  out.write(data.data(), (std::streamsize)data.size());
  out.close();
  return !out.fail();
}

static bool write_entry(const ZipReader& zip, const ZipEntry& entry, const fs::path& to) {
  std::error_code ec;
  fs::create_directories(to.parent_path(), ec);
  bool ok;
  std::string data = read_entry(zip, entry, ok);
  return ok && write_file(to, data);
}

/// A new, empty temporary directory in dir
static fs::path make_temp_dir(const fs::path& dir) {
  std::random_device rd;
  for (int i = 0 ; i < 100 ; ++i) {
    fs::path p = dir / (TEMP_PREFIX + std::to_string(rd()));
    std::error_code ec;
    if (fs::create_directory(p, ec)) return p;
    if (ec) throw ImportError("could not create a temporary directory in " + dir.u8string() + ": " + ec.message());
  }
  throw ImportError("could not create a temporary directory in " + dir.u8string());
}

/// Remove what an interrupted import left in dir
static void remove_stale_temp_dirs(const fs::path& dir) {
  std::error_code ec;
  for (fs::directory_iterator it(dir, ec), end ; !ec && it != end ; it.increment(ec)) {
    if (it->path().filename().u8string().rfind(TEMP_PREFIX, 0) == 0) {
      std::error_code ec2;
      fs::remove_all(it->path(), ec2);
    }
  }
}

/// Removes a directory when it goes out of scope
struct TempDir {
  fs::path path;
  ~TempDir() {
    std::error_code ec;
    if (!path.empty()) fs::remove_all(path, ec);
  }
};

// ----------------------------------------------------------------------------- : Package headers

/// The parts of a package's data file the installer looks at, in MSE's installer data model
struct PackageHeader {
  PackageDescription description;
  String version_string;     ///< as written
  String mse_version;
};

/// The top level "key: value" lines of a data file, and its "depends on" blocks
static PackageHeader read_header(const String& package_name, const std::string& raw) {
  PackageHeader h;
  PackageDescription& d = h.description;
  d.name = package_name;
  String data = decode_utf8(raw);
  if (!data.empty() && data[0] == 0xFEFF) data.erase(0, 1); // byte order mark
  vector<String> lines;
  size_t pos = 0;
  while (pos <= data.size()) {
    size_t eol = data.find(_('\n'), pos);
    if (eol == String::npos) eol = data.size();
    lines.push_back(data.substr(pos, eol - pos));
    pos = eol + 1;
  }
  auto key_value = [](const String& line, String& key, String& value) {
    size_t colon = line.find(_(':'));
    if (colon == String::npos) return false;
    key   = trim(line.substr(0, colon));
    value = trim(line.substr(colon + 1));
    return true;
  };
  for (size_t i = 0 ; i < lines.size() ; ++i) {
    const String& line = lines[i];
    if (line.empty() || line[0] == _('\t') || line[0] == _(' ') || line[0] == _('#')) continue;
    String key, value;
    if (!key_value(line, key, value)) continue;
    if      (key == _("version"))         { h.version_string = value; d.version = Version::fromString(value); }
    else if (key == _("mse version"))     h.mse_version = value;
    else if (key == _("short name"))      d.short_name = value;
    else if (key == _("full name"))       d.full_name = value;
    else if (key == _("installer group")) d.installer_group = value;
    else if (key == _("depends on")) {
      auto dep = make_intrusive<PackageDependency>();
      if (!value.empty()) {
        // "package version", as Reader::handle(PackageDependency&) reads it
        size_t space = value.find(_(' '));
        dep->package = value.substr(0, space);
        if (space != String::npos) dep->version = Version::fromString(trim(value.substr(space + 1)));
      } else {
        for (size_t j = i + 1 ; j < lines.size() ; ++j) {
          const String& sub = lines[j];
          if (!sub.empty() && sub[0] != _('\t') && sub[0] != _(' ') && sub[0] != _('#')) break;
          String k, v;
          if (!key_value(sub, k, v)) continue;
          if      (k == _("package")) dep->package = v;
          else if (k == _("version")) dep->version = Version::fromString(v);
        }
      }
      if (!dep->package.empty()) d.dependencies.push_back(dep);
    }
  }
  // as PackageDescription(const Packaged&) derives them
  String type = package_type(package_name);
  String base = package_name.substr(0, package_name.size() - type.size() - 5); // without ".mse-type"
  if (d.short_name.empty()) d.short_name = base;
  if (d.full_name.empty())  d.full_name = d.short_name;
  if (d.installer_group.empty()) {
    String group = base;
    size_t dash = group.find_last_of(_('-'));
    if (dash != String::npos) group.resize(dash);
    if (!group.empty()) {
      d.installer_group = _("unclassified/") + replace_all(group, _("-"), _("/")) + _("/");
    } else {
      d.installer_group = _("unclassified/");
    }
    d.installer_group += type == _("game") ? String(_("Game files")) : d.short_name;
  }
  return h;
}

// ----------------------------------------------------------------------------- : Scanning

/// A package found in a zip
struct FoundPackage {
  String name;                 ///< e.g. "magic-m15.mse-style"
  String type;                 ///< e.g. "style"
  const ZipReader* zip;        ///< the zip it is in (the input, or one inside it)
  String prefix;               ///< its path in that zip, '/' separated, ending in '/' (empty for a zipped package)
  vector<const ZipEntry*> files;
  std::string data_file;       ///< contents of the main data file
  bool has_data_file = false;
  PackageHeader header;
  uint64_t size = 0;
  // the scan's verdict
  bool   installed = false;
  String installed_version;
  ScannedPackage::Verdict verdict = ScannedPackage::Verdict::Install;
  String reason;

  bool installable() const {
    return verdict == ScannedPackage::Verdict::Install || verdict == ScannedPackage::Verdict::Replace;
  }
};

struct LooseFile {
  String name;
  const ZipReader* zip;
  const ZipEntry* entry;
};

/// Everything in a zip (and the zips inside it)
class ImportScanner {
public:
  vector<FoundPackage> packages; ///< one per name, sorted by name
  vector<LooseFile> fonts;
  vector<LooseFile> dictionaries;

  /// temp: a directory for zips inside the zip
  ImportScanner(const fs::path& temp) : temp(temp) {}

  void scan(unique_ptr<ZipReader> zip, const fs::path& store) {
    const ZipReader* z = zip.get();
    zips.push_back(std::move(zip));
    scanZip(*z, 0);
    choose();
    judge(store);
  }

  FoundPackage* find(const String& name) {
    auto it = std::lower_bound(packages.begin(), packages.end(), name, [](const FoundPackage& p, const String& n) { return p.name < n; });
    return it != packages.end() && it->name == name ? &*it : nullptr;
  }

private:
  fs::path temp;
  vector<unique_ptr<ZipReader>> zips;
  vector<FoundPackage> candidates;
  map<pair<const ZipReader*, String>, size_t> candidate_index; // (zip, prefix) -> candidates

  static const int MAX_NESTING = 4;

  FoundPackage& candidate(const ZipReader& zip, const String& name, const String& prefix) {
    auto key = make_pair(&zip, prefix);
    auto it = candidate_index.find(key);
    if (it != candidate_index.end()) return candidates[it->second];
    candidate_index[key] = candidates.size();
    FoundPackage p;
    p.name = name;
    p.type = package_type(name);
    p.zip = &zip;
    p.prefix = prefix;
    candidates.push_back(std::move(p));
    return candidates.back();
  }

  /// Extract a zip inside a zip to the temp directory and open it
  const ZipReader* openNested(const ZipReader& zip, const ZipEntry& entry) {
    fs::path to = temp / ("nested-" + std::to_string(zips.size()) + ".zip");
    if (!write_entry(zip, entry, to)) return nullptr;
    auto nested = make_unique<ZipReader>(decode_utf8(to.u8string()));
    if (!nested->ok()) return nullptr;
    zips.push_back(std::move(nested));
    return zips.back().get();
  }

  void scanZip(const ZipReader& zip, int nesting) {
    for (const ZipEntry& entry : zip.entries()) {
      vector<String> parts = split_entry(entry.name);
      if (parts.empty()) continue;
      bool is_dir = ends_with(entry.name, _("/"));
      size_t last = parts.size() - 1;
      // the outermost Template directory on the path
      size_t pkg = parts.size();
      bool in_other_package = false;
      for (size_t i = 0 ; i < (is_dir ? parts.size() : last) ; ++i) {
        String type = package_type(parts[i]);
        if (is_template_type(type)) { pkg = i; break; }
        if (type == _("set") || type == _("symbol")) { in_other_package = true; break; }
        // an unzipped installer directory: look inside it
      }
      if (in_other_package) continue;
      if (pkg < parts.size()) {
        String prefix;
        for (size_t i = 0 ; i <= pkg ; ++i) prefix += parts[i] + _("/");
        FoundPackage& p = candidate(zip, parts[pkg], prefix);
        if (is_dir) continue;
        p.files.push_back(&entry);
        p.size += (uint64_t)std::max<int64_t>(0, entry.size);
        if (parts.size() == pkg + 2 && parts[pkg + 1] == p.type) {
          p.data_file = read_entry(zip, entry, p.has_data_file);
        }
        continue;
      }
      if (is_dir) continue;
      String type = package_type(parts[last]);
      if (type == _("installer") || is_template_type(type)) {
        // an installer or a zipped package
        if (nesting >= MAX_NESTING) continue;
        const ZipReader* nested = openNested(zip, entry);
        if (!nested) continue;
        if (type == _("installer")) {
          scanZip(*nested, nesting + 1);
        } else {
          FoundPackage& p = candidate(*nested, parts[last], String());
          for (const ZipEntry& e : nested->entries()) {
            if (ends_with(e.name, _("/"))) continue;
            p.files.push_back(&e);
            p.size += (uint64_t)std::max<int64_t>(0, e.size);
            if (e.name == p.type) p.data_file = read_entry(*nested, e, p.has_data_file);
          }
        }
      } else if (is_font_file(parts[last])) {
        fonts.push_back(LooseFile{parts[last], &zip, &entry});
      } else if (is_dictionary_file(parts)) {
        dictionaries.push_back(LooseFile{parts[last], &zip, &entry});
      }
    }
  }

  /// One version per package: the newest one in the zip (the first of equal ones)
  void choose() {
    for (FoundPackage& c : candidates) {
      c.header = read_header(c.name, c.data_file);
    }
    map<String, size_t> best;
    for (size_t i = 0 ; i < candidates.size() ; ++i) {
      auto it = best.find(candidates[i].name);
      if (it == best.end()) {
        best[candidates[i].name] = i;
      } else {
        const FoundPackage& b = candidates[it->second];
        const FoundPackage& c = candidates[i];
        if ((c.has_data_file && !b.has_data_file) || (c.has_data_file && c.header.description.version > b.header.description.version)) {
          it->second = i;
        }
      }
    }
    for (auto& b : best) packages.push_back(std::move(candidates[b.second]));
    candidates.clear();
    candidate_index.clear();
  }

  /// The verdict on each package
  void judge(const fs::path& store) {
    using V = ScannedPackage::Verdict;
    for (FoundPackage& p : packages) {
      fs::path target = store / to_fs_path(p.name);
      std::error_code ec;
      p.installed = fs::is_directory(target, ec) || fs::is_regular_file(target, ec);
      if (p.installed) {
        p.installed_version = read_header(p.name, read_file(target / to_fs_path(p.type))).version_string;
      }
      if (!p.has_data_file) {
        p.verdict = V::Refuse;
        p.reason = _("the package has no '") + p.type + _("' file");
      } else if (!p.header.mse_version.empty() && Version::fromString(p.header.mse_version) > app_version) {
        p.verdict = V::Refuse;
        p.reason = _("the package needs a newer version of the program (mse version ") + p.header.mse_version + _(")");
      } else if (p.installed && !(p.header.description.version > Version::fromString(p.installed_version))) {
        p.verdict = V::SkipNotNewer;
      } else {
        p.verdict = p.installed ? V::Replace : V::Install;
      }
    }
    // dependencies, until nothing changes (a refusal can refuse what depends on it)
    bool changed = true;
    while (changed) {
      changed = false;
      for (FoundPackage& p : packages) {
        if (!p.installable()) continue;
        for (const PackageDependencyP& dep : p.header.description.dependencies) {
          String why = unmet(*dep, store);
          if (!why.empty()) {
            p.verdict = V::Refuse;
            p.reason = why;
            changed = true;
            break;
          }
        }
      }
    }
  }

public:
  /// Is the installed package recent enough for a dependency?
  static bool installed_satisfies(const PackageDependency& dep, const fs::path& store, String& installed_version) {
    fs::path target = store / to_fs_path(dep.package);
    std::error_code ec;
    if (!fs::exists(target, ec)) return false;
    installed_version = read_header(dep.package, read_file(target / to_fs_path(package_type(dep.package)))).version_string;
    return Version::fromString(installed_version) >= dep.version;
  }

  /// Why a dependency is not met, empty if it is
  String unmet(const PackageDependency& dep, const fs::path& store) {
    String need = dep.package + (dep.version.isZero() ? String() : _(" version ") + dep.version.toString());
    if (dep.package == mse_package) {
      return app_version >= dep.version ? String() : _("the package needs ") + need;
    }
    String installed_version;
    if (installed_satisfies(dep, store, installed_version)) return String();
    const FoundPackage* in_zip = find(dep.package);
    if (in_zip && in_zip->header.description.version >= dep.version) {
      if (in_zip->installable()) return String();
      if (in_zip->verdict == ScannedPackage::Verdict::Refuse) {
        return _("the package needs ") + need + _(", which is refused: ") + in_zip->reason;
      }
    }
    return _("the package needs ") + need + _(", which is neither installed nor in the zip");
  }
};

// ----------------------------------------------------------------------------- : Scan

static unique_ptr<ZipReader> open_input(const ImportInput& input) {
  if (input.fd >= 0) return make_unique<ZipReader>(input.fd);
  return make_unique<ZipReader>(decode_utf8(input.path));
}

/// Open the store and the zip, and scan it; returns false (and sets out) on failure
template <typename R>
static bool start_import(const ImportInput& input, R& out, fs::path& store, fs::path& fonts, TempDir& temp, unique_ptr<ImportScanner>& scanner) {
  std::string templates, font_dir;
  {
    std::lock_guard<std::mutex> lock(store_mutex);
    templates = store_templates;
    font_dir  = store_fonts;
  }
  if (templates.empty()) {
    out.status = Status::Failed;
    out.error = "the facade is not initialised (no template store)";
    return false;
  }
  store = fs::u8path(templates);
  if (!font_dir.empty()) fonts = fs::u8path(font_dir);
  unique_ptr<ZipReader> zip = open_input(input);
  if (!zip->ok()) {
    out.status = Status::InvalidArgument;
    out.error = input.fd >= 0 ? "not a zip file (descriptor " + std::to_string(input.fd) + ")" : "not a zip file: " + input.path;
    return false;
  }
  std::error_code ec;
  fs::create_directories(store, ec);
  remove_stale_temp_dirs(store);
  temp.path = make_temp_dir(store);
  scanner = make_unique<ImportScanner>(temp.path);
  scanner->scan(std::move(zip), store);
  return true;
}

/// Loose files, each file name once (the first of the files with that name)
static vector<const LooseFile*> unique_names(const vector<LooseFile>& files) {
  vector<const LooseFile*> out;
  set<String> seen;
  for (const LooseFile& f : files) {
    if (seen.insert(f.name).second) out.push_back(&f);
  }
  return out;
}

static bool loose_file_installed(const LooseFile& f, const fs::path& dir, bool keep_both);

void scan_templates(const ImportInput& input, ImportScan& out) {
  std::lock_guard<std::mutex> lock(import_mutex);
  fs::path store, fonts;
  TempDir temp;
  unique_ptr<ImportScanner> scanner;
  if (!start_import(input, out, store, fonts, temp, scanner)) return;
  for (const FoundPackage& p : scanner->packages) {
    const PackageDescription& d = p.header.description;
    ScannedPackage s;
    s.name              = encode_utf8(p.name);
    s.type              = encode_utf8(p.type);
    s.full_name         = encode_utf8(d.full_name);
    s.version           = encode_utf8(p.header.version_string);
    s.installed_version = encode_utf8(p.installed_version);
    s.installer_group   = encode_utf8(d.installer_group);
    for (const PackageDependencyP& dep : d.dependencies) {
      s.dependencies.push_back(ImportDependency{encode_utf8(dep->package), dep->version.isZero() ? std::string() : encode_utf8(dep->version.toString())});
    }
    s.size    = p.size;
    s.verdict = p.verdict;
    s.reason  = encode_utf8(p.reason);
    out.packages.push_back(std::move(s));
  }
  std::stable_sort(out.packages.begin(), out.packages.end(), [](const ScannedPackage& a, const ScannedPackage& b) {
    return a.installer_group != b.installer_group ? a.installer_group < b.installer_group : a.name < b.name;
  });
  for (const LooseFile* f : unique_names(scanner->fonts)) {
    uint64_t size = (uint64_t)std::max<int64_t>(0, f->entry->size);
    out.fonts.push_back(encode_utf8(f->name));
    out.fonts_size += size;
    if (fonts.empty() || !loose_file_installed(*f, fonts, true)) {
      out.new_fonts.push_back(encode_utf8(f->name));
      out.new_fonts_size += size;
    }
  }
  for (const LooseFile* f : unique_names(scanner->dictionaries)) {
    out.dictionaries.push_back(encode_utf8(f->name));
    if (!loose_file_installed(*f, store / "dictionaries", false)) out.new_dictionaries.push_back(encode_utf8(f->name));
  }
}

// ----------------------------------------------------------------------------- : Install

/// Extract a package into the temporary directory and rename it into place
static void install_package(const FoundPackage& p, const fs::path& store, const fs::path& temp, std::mutex& loading_mutex,
                            ProgressCounter& counter) {
  fs::path extracted = temp / to_fs_path(p.name);
  std::error_code ec;
  fs::create_directories(extracted, ec);
  if (ec) throw ImportError("could not create " + extracted.u8string() + ": " + ec.message());
  for (const ZipEntry* e : p.files) {
    vector<String> rel_parts = split_entry(e->name.substr(p.prefix.size()));
    if (rel_parts.empty()) continue;
    fs::path to = extracted;
    for (const String& part : rel_parts) to /= to_fs_path(part);
    if (!write_entry(*p.zip, *e, to)) throw ImportError("could not extract " + encode_utf8(e->name));
    counter.add((uint64_t)e->size);
  }
  fs::path target = store / to_fs_path(p.name);
  fs::path old = temp / to_fs_path(p.name + _(".replaced"));
  {
    // not while a session is loading packages
    std::lock_guard<std::mutex> lock(loading_mutex);
    bool exists = fs::exists(target, ec);
    if (exists) {
      fs::rename(target, old, ec);
      if (ec) throw ImportError("could not replace " + target.u8string() + ": " + ec.message());
    }
    fs::rename(extracted, target, ec);
    if (ec) {
      std::error_code ec2;
      if (exists) fs::rename(old, target, ec2); // put the old version back
      throw ImportError("could not install " + target.u8string() + ": " + ec.message());
    }
  }
  fs::remove_all(old, ec);
}

/// Copy a loose file into a directory, unless it is already there; returns the name it got.
/** A different file with the same name is replaced, or with keep_both (for fonts, whose file
 *  names say little) gets a new name: "name (2).ttf". */
/// Is a loose file already in a directory, with the same contents (under its name, or with
/// keep_both under a numbered name as install_loose_file gives it)?
static bool loose_file_installed(const LooseFile& f, const fs::path& dir, bool keep_both) {
  String stem = f.name, ext;
  size_t dot = f.name.find_last_of(_('.'));
  if (dot != String::npos) { stem = f.name.substr(0, dot); ext = f.name.substr(dot); }
  std::string data;
  bool have_data = false;
  for (int n = 1 ; n < 100 ; ++n) {
    String name = n == 1 ? f.name : stem + _(" (") + int_to_string(n) + _(")") + ext;
    fs::path to = dir / to_fs_path(name);
    std::error_code ec;
    if (!fs::exists(to, ec)) return false;
    if (fs::file_size(to, ec) == (uintmax_t)f.entry->size) {
      if (!have_data) {
        bool ok;
        data = read_entry(*f.zip, *f.entry, ok);
        if (!ok) return false;
        have_data = true;
      }
      if (read_file(to) == data) return true;
    }
    if (!keep_both) return false;
  }
  return false;
}

static String install_loose_file(const LooseFile& f, const fs::path& dir, bool keep_both) {
  bool ok;
  std::string data = read_entry(*f.zip, *f.entry, ok);
  if (!ok) throw ImportError("could not read " + encode_utf8(f.entry->name));
  String stem = f.name, ext;
  size_t dot = f.name.find_last_of(_('.'));
  if (dot != String::npos) { stem = f.name.substr(0, dot); ext = f.name.substr(dot); }
  for (int n = 1 ; n < 100 ; ++n) {
    String name = n == 1 ? f.name : stem + _(" (") + int_to_string(n) + _(")") + ext;
    fs::path to = dir / to_fs_path(name);
    std::error_code ec;
    if (fs::exists(to, ec)) {
      if (fs::file_size(to, ec) == data.size() && read_file(to) == data) return String(); // already there
      if (keep_both) continue;
    }
    fs::path temp = dir / to_fs_path(_(".") + name + _(".importing"));
    if (!write_file(temp, data)) {
      fs::remove(temp, ec);
      throw ImportError("could not write " + to.u8string());
    }
    fs::rename(temp, to, ec);
    if (ec) {
      std::error_code ec2;
      fs::remove(temp, ec2);
      throw ImportError("could not write " + to.u8string() + ": " + ec.message());
    }
    return name;
  }
  return String();
}

void install_templates(const ImportInput& input, const ImportSelection& selection, bool install_all,
                       const ImportProgress& progress, std::mutex& loading_mutex, ImportResult& out) {
  using V = ScannedPackage::Verdict;
  using O = ImportedPackage::Outcome;
  std::lock_guard<std::mutex> lock(import_mutex);
  fs::path store, fonts;
  TempDir temp;
  unique_ptr<ImportScanner> scanner;
  if (!start_import(input, out, store, fonts, temp, scanner)) return;

  // what was asked for
  vector<String> wanted;
  if (install_all) {
    for (const FoundPackage& p : scanner->packages) wanted.push_back(p.name);
  } else {
    for (const std::string& n : selection.packages) wanted.push_back(decode_utf8(n));
  }
  // the order to install in: dependencies first; and what each needs from the zip
  vector<const FoundPackage*> order;
  set<String> visited;
  map<String, vector<String>> needs;
  set<String> chosen;
  std::function<void(const FoundPackage&)> visit = [&](const FoundPackage& p) {
    if (!visited.insert(p.name).second) return;
    for (const PackageDependencyP& dep : p.header.description.dependencies) {
      if (dep->package == mse_package) continue;
      String installed_version;
      if (ImportScanner::installed_satisfies(*dep, store, installed_version)) continue;
      const FoundPackage* d = scanner->find(dep->package);
      if (d && d->installable()) { // (the scan refused p otherwise)
        needs[p.name].push_back(d->name);
        visit(*d);
      }
    }
    order.push_back(&p);
  };
  for (const String& name : wanted) {
    if (!chosen.insert(name).second) continue;
    const FoundPackage* p = scanner->find(name);
    ImportedPackage r;
    r.name = encode_utf8(name);
    if (!p) {
      r.outcome = O::Failed;
      r.error = "the package is not in the zip";
      out.packages.push_back(r);
    } else if (p->verdict == V::SkipNotNewer) {
      r.version = encode_utf8(p->header.version_string);
      r.outcome = O::SkippedNotNewer;
      out.packages.push_back(r);
    } else if (p->verdict == V::Refuse) {
      r.version = encode_utf8(p->header.version_string);
      r.outcome = O::Refused;
      r.error = encode_utf8(p->reason);
      out.packages.push_back(r);
    } else {
      visit(*p);
    }
  }
  // what there is to extract, for the progress callback
  ProgressCounter counter{progress};
  for (const FoundPackage* p : order) counter.total += p->size;
  if (selection.dictionaries) for (const LooseFile* f : unique_names(scanner->dictionaries)) counter.total += (uint64_t)f->entry->size;
  if (selection.fonts && !fonts.empty()) for (const LooseFile& f : scanner->fonts) counter.total += (uint64_t)f.entry->size;
  if (progress && !progress(0, counter.total)) out.cancelled = true;
  // install
  set<String> failed;
  for (const FoundPackage* p : order) {
    ImportedPackage r;
    r.name       = encode_utf8(p->name);
    r.version    = encode_utf8(p->header.version_string);
    r.dependency = !chosen.count(p->name);
    if (out.cancelled) {
      r.outcome = O::Cancelled;
      out.packages.push_back(r);
      continue;
    }
    try {
      for (const String& d : needs[p->name]) {
        if (failed.count(d)) throw ImportError("a package it needs could not be installed: " + encode_utf8(d));
      }
      install_package(*p, store, temp.path, loading_mutex, counter);
      r.outcome = p->verdict == V::Replace ? O::Replaced : O::Installed;
    } catch (const ImportCancelled&) {
      // (its partly extracted files are in the temporary directory, which goes away)
      out.cancelled = true;
      r.outcome = O::Cancelled;
    } catch (const std::exception& e) {
      failed.insert(p->name);
      r.outcome = O::Failed;
      r.error = e.what();
    }
    out.packages.push_back(r);
  }
  if (out.cancelled) return;
  try {
    // spelling dictionaries go where MSE looks for them
    if (selection.dictionaries && !scanner->dictionaries.empty()) {
      fs::path dir = store / "dictionaries";
      std::error_code ec;
      fs::create_directories(dir, ec);
      for (const LooseFile* f : unique_names(scanner->dictionaries)) {
        try {
          String name = install_loose_file(*f, dir, false);
          if (!name.empty()) out.dictionaries.push_back(encode_utf8(name));
          counter.add((uint64_t)f->entry->size);
        } catch (const std::exception& e) {
          out.messages.push_back(Message{Severity::Error, e.what(), std::string(), std::string()});
        }
      }
    }
    // loose fonts go to the shared font store (files with the same name and contents only once)
    if (selection.fonts && !fonts.empty()) {
      std::error_code ec;
      if (!scanner->fonts.empty()) fs::create_directories(fonts, ec);
      for (const LooseFile& f : scanner->fonts) {
        try {
          String name = install_loose_file(f, fonts, true);
          if (!name.empty()) out.fonts.push_back(encode_utf8(name));
          counter.add((uint64_t)f.entry->size);
        } catch (const std::exception& e) {
          out.messages.push_back(Message{Severity::Error, e.what(), std::string(), std::string()});
        }
      }
    }
  } catch (const ImportCancelled&) {
    out.cancelled = true;
  }
}

} // namespace mse::facade
