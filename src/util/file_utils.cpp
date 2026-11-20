//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/file_utils.hpp>
#include <errno.h>
#include <sys/stat.h>
#include <filesystem>
#include <cstdlib>
#include <random>
#if defined(__WXMSW__) || defined(__WXOSX__)
  #include <wx/stdpaths.h>
#else
  #include <unistd.h>
  #include <pwd.h>
#endif

namespace fs = std::filesystem;

// ----------------------------------------------------------------------------- : Path conversion

// Paths are converted the same way wx converted file names (wxConvFileName), so non-ASCII names keep working.
fs::path to_fs_path(const String& s) {
  #ifdef _WIN32
    return fs::path(s.ToStdWstring());
  #else
    wxCharBuffer buf = s.fn_str();
    return fs::path(buf.data() ? std::string(buf.data()) : std::string());
  #endif
}
static String from_fs_path(const fs::path& p) {
  #ifdef _WIN32
    return String(p.wstring());
  #else
    return String(p.c_str(), *wxConvFileName);
  #endif
}

static bool is_path_separator(Char c) {
  #ifdef _WIN32
    return c == _('/') || c == _('\\');
  #else
    return c == _('/');
  #endif
}

// ----------------------------------------------------------------------------- : Path components

Char path_separator() {
  return Char(fs::path::preferred_separator);
}

String path_only(const String& path) {
  return from_fs_path(to_fs_path(path).parent_path());
}

String file_full_name(const String& path) {
  return from_fs_path(to_fs_path(path).filename());
}

String file_name_no_ext(const String& path) {
  return from_fs_path(to_fs_path(path).stem());
}

String file_ext(const String& path) {
  String ext = from_fs_path(to_fs_path(path).extension());
  return ext.empty() ? ext : ext.substr(1); // drop the dot
}

String join_path(const String& dir, const String& name) {
  return from_fs_path(to_fs_path(dir) / to_fs_path(name));
}

String replace_file_name(const String& path, const String& full_name) {
  return from_fs_path(to_fs_path(path).replace_filename(to_fs_path(full_name)));
}

bool is_relative_path(const String& path) {
  return to_fs_path(path).is_relative();
}

// ----------------------------------------------------------------------------- : File names

/// Expand $VAR, ${VAR} and $(VAR) environment variable references, unknown variables are left alone.
/** Matches wxExpandEnvVars, which wxFileName::Normalize used. '\' escapes a '$' (not on Windows). */
static String expand_env_vars(const String& str) {
  String result;
  for (size_t n = 0 ; n < str.size() ; ++n) {
    Char c = str[n];
    if (c == _('$')) {
      Char bracket = 0;
      if (n + 1 < str.size()) {
        if      (str[n+1] == _('(')) { bracket = _(')'); ++n; }
        else if (str[n+1] == _('{')) { bracket = _('}'); ++n; }
      }
      size_t m = n + 1;
      while (m < str.size() && (isAlnum(str[m]) || str[m] == _('_'))) ++m;
      String var_name = str.substr(n + 1, m - n - 1);
      const char* value = var_name.empty() ? nullptr : std::getenv(var_name.mb_str());
      if (value) {
        result += String(value, wxConvLibc);
      } else {
        // variable doesn't exist => don't change anything
        result += _('$');
        if (bracket) result += str[n];
        result += var_name;
      }
      if (bracket) {
        if (m < str.size() && str[m] == bracket) {
          if (!value) result += bracket;
          ++m;
        }
      }
      n = m - 1; // skip variable name
    #ifndef _WIN32
    } else if (c == _('\\') && n + 1 < str.size() && str[n+1] == _('$')) {
      result += str[++n];
    #endif
    } else {
      result += c;
    }
  }
  return result;
}

#if !defined(__WXMSW__) && !defined(__WXOSX__)
/// Home directory of the given user (the current user if empty), "" if unknown
static String user_home_dir(const String& user = String()) {
  struct passwd* who = nullptr;
  if (user.empty()) {
    if (const char* home = std::getenv("HOME")) return String(home, wxConvLibc);
    if (const char* name = std::getenv("USER")) who = getpwnam(name);
    else if (const char* name = std::getenv("LOGNAME")) who = getpwnam(name);
    if (!who) who = getpwuid(getuid());
  } else {
    who = getpwnam(user.mb_str());
  }
  return who ? String(who->pw_dir, wxConvLibc) : String();
}
#endif

String normalize_filename(const String& name, const String& base_dir) {
  // This reproduces wxFileName::Normalize() with the default flags.
  String path = expand_env_vars(name);
  // split off the last component, it is kept as is, only directory components are normalized
  size_t pos = path.size();
  while (pos > 0 && !is_path_separator(path[pos-1])) --pos;
  String dir = path.substr(0, pos), full_name = path.substr(pos);
  #if !defined(__WXMSW__) && !defined(__WXOSX__)
    // "~/" and "~user/" refer to home directories
    if (!dir.empty() && dir[0] == _('~')) {
      size_t sep = 1;
      while (!is_path_separator(dir[sep])) ++sep; // dir ends in a separator, so this terminates
      String home = user_home_dir(dir.substr(1, sep - 1));
      if (!home.empty()) dir = home + dir.substr(sep);
    }
  #endif
  fs::path dir_path = to_fs_path(dir);
  if (dir_path.is_relative()) {
    std::error_code ec;
    fs::path base = base_dir.empty() ? fs::current_path(ec) : fs::absolute(to_fs_path(base_dir), ec);
    dir_path = base / dir_path;
  }
  String normal_dir = from_fs_path(dir_path.lexically_normal());
  if (normal_dir.empty() || !is_path_separator(normal_dir.Last())) normal_dir += path_separator();
  return normal_dir + full_name;
}

String normalize_internal_filename(const String& name) {
  String ret;
  FOR_EACH_CONST(c, name) {
    if (c==_('\\')) ret += _('/');
    else            ret += toLower(c);
  }
  return ret;
}

bool ignore_file(const String& name) {
  // Files that are never part of a package,
  // i.e. random stuff the OS file manager dumps without being asked
  return name == _("Thumbs.db"); // winXP explorer thumbnails
}

bool match_filename_wildcard(const String& text, const String& pattern) {
  size_t text_index = 0, pattern_index = 0;
  // if a '*' is hit but the rest of the pattern doesn't match, we backtrack here:
  // retry with the '*' consuming one more character of text than last time.
  size_t star_pattern_index = String::npos, star_text_index = 0;
  while (text_index < text.size()) {
    if (pattern_index < pattern.size() && (pattern[pattern_index] == _('?') || pattern[pattern_index] == text[text_index])) {
      // literal character (or '?') matches the current text character
      ++text_index; ++pattern_index;
    } else if (pattern_index < pattern.size() && pattern[pattern_index] == _('*')) {
      // remember this '*' in case we need to backtrack to it
      star_pattern_index = pattern_index++;
      star_text_index = text_index;
    } else if (star_pattern_index != String::npos) {
      // mismatch, but we saw a '*' earlier: let it consume one more character and retry
      pattern_index = star_pattern_index + 1;
      text_index = ++star_text_index;
    } else {
      return false;
    }
  }
  // skip any trailing '*'s, they can match zero characters
  while (pattern_index < pattern.size() && pattern[pattern_index] == _('*')) ++pattern_index;
  return pattern_index == pattern.size();
}

String add_extension(const String& filename, String const& extension) {
  if (extension.size() <= filename.size() && is_substr(filename, filename.size() - extension.size(), extension)) {
    return filename;
  } else {
    return filename + extension;
  }
}

bool is_filename_char(Char c) {
  return isAlnum(c) || c == _(' ') || c == _('_') || c == _('-') || c == _('.');
}
bool is_filename_char_trim(Char c) {
  return c == _(' ') || c == _('.');
}

String clean_filename(const String& name) {
  String clean;
  // allow only valid characters, and remove leading whitespace
  bool start = true;
  FOR_EACH_CONST(c, name) {
    if (is_filename_char(c) && !(start && is_filename_char_trim(c))) {
      start = false;
      clean += c;
    }
  }
  // remove trailing whitespace and dots
  while (!clean.empty() && is_filename_char_trim(clean[clean.size()-1])) {
    clean.resize(clean.size()-1);
  }
  if (clean.empty()) {
    clean = _("no-name") + clean;
  }
  return clean;
}

bool resolve_filename_conflicts(String& fn, FilenameConflicts conflicts, set<String>& used) {
  // the filename with its extension replaced by a number + the old extension
  String ext = file_ext(fn);
  String without_ext = replace_file_name(fn, file_name_no_ext(fn));
  auto numbered = [&](int i) { return String() << without_ext << _(".") << i << _(".") << ext; };
  switch (conflicts) {
    case CONFLICT_KEEP_OLD:
      return !file_exists(fn);
    case CONFLICT_OVERWRITE:
      return true;
    case CONFLICT_NUMBER: {
      int i = 0;
      while(file_exists(fn)) {
        fn = numbered(++i);
      }
      return true;
    }
    case CONFLICT_NUMBER_OVERWRITE: {
      int i = 0;
      while(used.find(fn) != used.end()) {
        fn = numbered(++i);
      }
      return true;
    }
    default: {
      throw InternalError(_("resolve_filename_conflicts: default case"));
    }
  }
}

String make_temp_file_name(const String& prefix) {
  std::error_code ec;
  fs::path dir = fs::temp_directory_path(ec);
  if (ec) dir = fs::path("/tmp");
  #ifdef _WIN32
    std::mt19937 rng(std::random_device{}());
    for (int attempt = 0 ; attempt < 100 ; ++attempt) {
      String name = prefix;
      for (int i = 0 ; i < 6 ; ++i) name += Char(_('a') + rng() % 26);
      fs::path path = dir / to_fs_path(name);
      if (FILE* f = _wfopen(path.c_str(), L"wx")) {
        fclose(f);
        return from_fs_path(path);
      }
    }
    return String();
  #else
    std::string templ = (dir / to_fs_path(prefix)).string() + "XXXXXX";
    int fd = mkstemp(&templ[0]);
    if (fd == -1) return String();
    close(fd);
    return from_fs_path(fs::path(templ));
  #endif
}

// ----------------------------------------------------------------------------- : File info

bool file_exists(const String& path) {
  std::error_code ec;
  return fs::is_regular_file(to_fs_path(path), ec);
}

bool dir_exists(const String& path) {
  std::error_code ec;
  return fs::is_directory(to_fs_path(path), ec);
}

time_t file_modified_time(const String& path) {
  // Note: wxFileName also provides a function for this, but that is very slow.
  struct stat statbuf;
  if (stat(path.mb_str(), &statbuf) != 0) {
    if (errno == ENOENT) {
      return 0;
    } else {
      throw InternalError(_("could not stat ") + path);
    }
  }
  return statbuf.st_mtime;
}

// ----------------------------------------------------------------------------- : Directory listing

bool list_directory(const String& dir, vector<String>& names_out, int flags, const String& pattern) {
  std::error_code ec;
  fs::directory_iterator it(to_fs_path(dir), ec);
  if (ec) return false;
  for (; it != fs::directory_iterator() ; it.increment(ec)) {
    if (ec) break;
    String name = from_fs_path(it->path().filename());
    if (!(flags & LIST_HIDDEN) && !name.empty() && name[0] == _('.')) continue;
    if (!pattern.empty() && !match_filename_wildcard(name, pattern)) continue;
    std::error_code ec2;
    bool is_dir = fs::is_directory(it->path(), ec2); // follows symlinks
    if (!(flags & (is_dir ? LIST_DIRS : LIST_FILES))) continue;
    names_out.push_back(name);
  }
  return true;
}

bool is_empty_dir(const String& path) {
  std::error_code ec;
  return fs::is_directory(to_fs_path(path), ec) && fs::is_empty(to_fs_path(path), ec) && !ec;
}

/// The directory name with exactly one trailing separator, used to build paths of its entries
static String dir_with_sep(const String& dir) {
  String d = dir;
  while (!d.empty() && is_path_separator(d.Last())) d.RemoveLast();
  return d + path_separator();
}

// ----------------------------------------------------------------------------- : Standard locations

#if defined(__WXMSW__) || defined(__WXOSX__)
// The Windows and macOS locations come from the system, keep using wx for those.
static String wx_standard_path(int which) {
  wxStandardPaths& sp = wxStandardPaths::Get();
  return which == 0 ? sp.GetExecutablePath() : which == 1 ? sp.GetDataDir() : sp.GetUserDataDir();
}
String executable_path() { return wx_standard_path(0); }
String app_data_dir()    { return wx_standard_path(1); }
String user_data_dir()   { return wx_standard_path(2); }
#else
// Same layout as wxStandardPaths on Unix (classic file layout)
static const Char* app_name = _("magicseteditor");

String executable_path() {
  std::error_code ec;
  fs::path exe = fs::read_symlink("/proc/self/exe", ec);
  if (ec) return String();
  String path = from_fs_path(exe);
  // the kernel marks the link like this if the executable was deleted
  if (path.EndsWith(_(" (deleted)"))) path.resize(path.size() - 10);
  return path;
}

String app_data_dir() {
  // consider the executable to be in the last "bin" directory of the install prefix
  String exe = executable_path();
  size_t pos = exe.rfind(_("/bin/"));
  String prefix = pos == String::npos ? String(_("/usr/local")) : exe.substr(0, pos);
  return prefix + _("/share/") + app_name;
}

String user_data_dir() {
  String home = user_home_dir();
  if (home.empty()) home = _("/");
  return home + _("/.") + app_name;
}
#endif

// ----------------------------------------------------------------------------- : Directories

bool create_directory(const String& path) {
  std::error_code ec;
  return fs::create_directory(to_fs_path(path), ec);
}

bool create_directories(const String& path) {
  String p = path;
  while (p.size() > 1 && is_path_separator(p.Last())) p.RemoveLast();
  std::error_code ec;
  fs::create_directories(to_fs_path(p), ec);
  return dir_exists(p);
}

bool remove_empty_dir(const String& path) {
  std::error_code ec;
  fs::path p = to_fs_path(path);
  if (!fs::is_directory(fs::symlink_status(p, ec))) return false; // like rmdir, only remove directories
  return fs::remove(p, ec);
}

bool create_parent_dirs(const String& file) {
  for (size_t pos = file.find_first_of(_("\\/"), 1) ;
       pos != String::npos ;
       pos = file.find_first_of(_("\\/"),pos+1)) {
    String part = file.substr(0,pos);
    if (!dir_exists(part)) {
      if (!create_directory(part)) return false;
    }
  }
  return true;
}

// ----------------------------------------------------------------------------- : Removing

/// Remove a file, or an empty directory, return true if it was removed
static bool remove_once(const String& filename) {
  std::error_code ec;
  return fs::remove(to_fs_path(filename), ec);
}

class RecursiveDeleter {
public:
  RecursiveDeleter(const String& start) {
    to_delete.push_back(start);
    ok = true;
  }
  
  bool ok;
  
  /// Delete all files in the directory tree, and collect the directories
  void traverse(const String& dir) {
    String prefix = dir_with_sep(dir);
    vector<String> names;
    list_directory(dir, names, LIST_DIRS | LIST_HIDDEN);
    for (const String& name : names) {
      String path = prefix + name;
      std::error_code ec;
      if (fs::is_symlink(to_fs_path(path), ec)) {
        // don't follow links to directories, just remove the link
        OnFile(path);
      } else {
        to_delete.push_back(path);
        traverse(path);
      }
    }
    names.clear();
    list_directory(dir, names, LIST_FILES | LIST_HIDDEN);
    for (const String& name : names) {
      OnFile(prefix + name);
    }
  }
  
  void remove() {
    FOR_EACH_REVERSE(dir, to_delete) {
      if (!remove_empty_dir(dir)) {
        ok = false;
        handle_error(_("Cannot delete ") + dir + _("\n")
          _("The remainder of the package has still been removed, if possible.\n")
          _("Other packages may have been removed, including packages that this on is dependent on. Please remove manually."));
      }
    }
  }
  
  void OnFile(const String& filename) {
    if (!remove_once(filename)) {
      ok = false;
      handle_error(_("Cannot delete ") + filename + _("\n")
        _("The remainder of the package has still been removed, if possible.\n")
        _("Other packages may have been removed, including packages that this on is dependent on. Please remove manually."));
    }
  }
private:
  vector<String> to_delete;
};

bool remove_file(const String& filename) {
  if (!file_exists(filename)) {
    // nothing there to remove, just call this once as cleanup
    return remove_once(filename);
  }
  return retry_io([&]{ return remove_once(filename); });
}

static bool remove_file_or_dir_once(const String& name) {
  if (file_exists(name)) {
    return remove_once(name);
  } else if (dir_exists(name)) {
    RecursiveDeleter rd(name);
    rd.traverse(name);
    rd.remove();
    return rd.ok;
  } else {
    return true;
  }
}

bool remove_file_or_dir(const String& name) {
  return retry_io([&]{ return remove_file_or_dir_once(name); });
}

// ----------------------------------------------------------------------------- : Renaming

/// Copy a file once, overwriting the destination
static bool copy_file_once(const String& from, const String& to) {
  std::error_code ec;
  return fs::copy_file(to_fs_path(from), to_fs_path(to), fs::copy_options::overwrite_existing, ec);
}

bool copy_file(const String& from, const String& to) {
  return retry_io([&]{ return copy_file_once(from, to); });
}

static bool rename_file_or_dir_once(const String& from, const String& to) {
  create_parent_dirs(to);
  std::error_code ec;
  fs::rename(to_fs_path(from), to_fs_path(to), ec);
  if (!ec) return true;
  // rename can't move across volumes or if something inside a directory is locked, fall back to copy + delete
  if (dir_exists(from)) {
    if (!create_directories(to)) return false;
    vector<String> names;
    if (!list_directory(from, names)) return false;
    bool ok = true;
    for (const String& filename : names) {
      ok &= rename_file_or_dir_once(from + path_separator() + filename, to + path_separator() + filename);
    }
    return ok && remove_file_or_dir_once(from);
  } else if (file_exists(from) && copy_file_once(from, to)) {
    return remove_once(from);
  }
  return false;
}

bool rename_file_or_dir(const String& from, const String& to) {
  return retry_io([&]{ return rename_file_or_dir_once(from, to); });
}

// ----------------------------------------------------------------------------- : Moving

class IgnoredMover {
public:
  IgnoredMover(const String& from, const String& to, const vector<String>& ignore_patterns)
    : from(from), to(to), ignore_patterns(ignore_patterns)
  {}
  /// Visit the directories (recursing into those not moved), then the files, of a directory tree
  void traverse(const String& dir) {
    String prefix = dir_with_sep(dir);
    vector<String> names;
    list_directory(dir, names, LIST_DIRS | LIST_HIDDEN);
    for (const String& name : names) {
      if (!tryMove(prefix + name)) traverse(prefix + name);
    }
    names.clear();
    list_directory(dir, names, LIST_FILES | LIST_HIDDEN);
    for (const String& name : names) {
      tryMove(prefix + name);
    }
  }
private:
  String from, to;
  const vector<String>& ignore_patterns;
  bool isIgnored(const String& from_path) const {
    // path relative to 'from', in normalized (package-internal) form
    String rel = normalize_internal_filename(from_path.substr(from.size()));
    while (!rel.empty() && rel[0] == _('/')) rel = rel.substr(1);
    for (const String& pattern : ignore_patterns) {
      if (match_filename_wildcard(rel, normalize_internal_filename(pattern))) return true;
    }
    return false;
  }
  bool tryMove(const String& from_path) {
    if (!is_substr(normalize_internal_filename(from_path),0,normalize_internal_filename(from))) {
      // This shouldn't happen
      return false;
    }
    if (!isIgnored(normalize_internal_filename(from_path))) {
      // Not one of the package's read_only_files files/patterns: this is content
      // managed by the package itself, so leave it for the fresh install to provide.
      return false;
    }
    String to_path = to + from_path.substr(from.size());
    remove_file_or_dir(to_path);
    return rename_file_or_dir(from_path, to_path);
  }
};

void move_ignored_files(const String& from_dir, const String& to_dir, const vector<String>& ignore_patterns) {
  if (ignore_patterns.empty()) return;
  if (dir_exists(from_dir) && dir_exists(to_dir)) {
    IgnoredMover im(from_dir, to_dir, ignore_patterns);
    im.traverse(from_dir);
  }
}
