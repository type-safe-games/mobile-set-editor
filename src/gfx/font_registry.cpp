//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gfx/font_registry.hpp>
#include <util/file_utils.hpp>
#include <fstream>
#include <algorithm>

// ----------------------------------------------------------------------------- : Reading font files

namespace {

/// Random access to a font file
class FontFile {
public:
  FontFile(const String& path) : in(to_fs_path(path), std::ios::binary) {
    if (in) {
      in.seekg(0, std::ios::end);
      size = (size_t)in.tellg();
    }
  }
  bool ok() const { return size > 0; }
  /// Read count bytes at offset; false if they are not all in the file
  bool read(size_t offset, size_t count, vector<Byte>& out) {
    if (offset > size || count > size - offset) return false;
    out.resize(count);
    in.seekg((std::streamoff)offset);
    in.read((char*)out.data(), (std::streamsize)count);
    return (bool)in;
  }
private:
  std::ifstream in;
  size_t size = 0;
};

inline UInt u16(const vector<Byte>& b, size_t i) {
  return i + 2 <= b.size() ? (UInt(b[i]) << 8) | b[i+1] : 0;
}
inline UInt u32(const vector<Byte>& b, size_t i) {
  return i + 4 <= b.size() ? (u16(b, i) << 16) | u16(b, i+2) : 0;
}

struct TableRecord {
  UInt offset = 0, length = 0;
};

/// A name record decoded to a string, with how much we like its platform and language
struct NameCandidate {
  int    preference = -1;
  String value;
};

String decode_utf16be(const vector<Byte>& b, size_t start, size_t length) {
  String out;
  for (size_t i = start ; i + 1 < start + length && i + 1 < b.size() ; i += 2) {
    UInt c = u16(b, i);
    if (c >= 0xD800 && c < 0xDC00 && i + 3 < start + length) {
      UInt d = u16(b, i + 2);
      if (d >= 0xDC00 && d < 0xE000) {
        c = 0x10000 + ((c - 0xD800) << 10) + (d - 0xDC00);
        i += 2;
      }
    }
    out += (Char)c;
  }
  return out;
}

String decode_latin1(const vector<Byte>& b, size_t start, size_t length) {
  String out;
  for (size_t i = start ; i < start + length && i < b.size() ; ++i) {
    out += (Char)b[i];
  }
  return out;
}

/// Read one face starting at the offset table at the given offset
bool read_face(FontFile& file, size_t offset, FontFace& face) {
  vector<Byte> header;
  if (!file.read(offset, 12, header)) return false;
  UInt version = u32(header, 0);
  if (version != 0x00010000 && version != 0x4F54544F /*OTTO*/ && version != 0x74727565 /*true*/) return false;
  UInt num_tables = u16(header, 4);
  vector<Byte> dir;
  if (!file.read(offset + 12, num_tables * 16, dir)) return false;
  TableRecord name_table, os2_table, head_table;
  for (UInt i = 0 ; i < num_tables ; ++i) {
    UInt tag = u32(dir, i * 16);
    TableRecord rec;
    rec.offset = u32(dir, i * 16 + 8);
    rec.length = u32(dir, i * 16 + 12);
    if      (tag == 0x6E616D65 /*name*/) name_table = rec;
    else if (tag == 0x4F532F32 /*OS/2*/) os2_table  = rec;
    else if (tag == 0x68656164 /*head*/) head_table = rec;
  }
  // names
  vector<Byte> names;
  if (!name_table.length || !file.read(name_table.offset, name_table.length, names)) return false;
  UInt count = u16(names, 2), string_offset = u16(names, 4);
  NameCandidate found[17];
  auto add_name = [](vector<String>& names, const String& value) {
    if (find(names.begin(), names.end(), value) == names.end()) names.push_back(value);
  };
  for (UInt i = 0 ; i < count ; ++i) {
    size_t r = 6 + i * 12;
    if (r + 12 > names.size()) break;
    UInt platform = u16(names, r), encoding = u16(names, r+2), language = u16(names, r+4);
    UInt name_id  = u16(names, r+6), length = u16(names, r+8), str = u16(names, r+10);
    if (name_id != 1 && name_id != 2 && name_id != 4 && name_id != 16) continue;
    int preference;
    bool utf16;
    if (platform == 3 && (encoding == 1 || encoding == 10)) {
      preference = language == 0x409 ? 4 : 3;
      utf16 = true;
    } else if (platform == 0) {
      preference = 2;
      utf16 = true;
    } else if (platform == 1 && encoding == 0) {
      preference = language == 0 ? 1 : 0;
      utf16 = false;
    } else {
      continue;
    }
    size_t start = string_offset + str;
    if (start + length > names.size()) continue;
    String value = utf16 ? decode_utf16be(names, start, length) : decode_latin1(names, start, length);
    if (value.empty()) continue;
    if (name_id == 1 || name_id == 16) add_name(face.family_names, value);
    if (name_id == 4)                   add_name(face.full_names, value);
    if (preference <= found[name_id].preference) continue;
    found[name_id].preference = preference;
    found[name_id].value = value;
  }
  face.family     = found[1].value;
  face.style_name = found[2].value;
  face.full_name  = found[4].value;
  if (face.family.empty()) return false;
  // weight and slant
  vector<Byte> os2;
  if (os2_table.length >= 64 && file.read(os2_table.offset, 64, os2)) {
    UInt weight = u16(os2, 4);
    if (weight >= 1 && weight <= 1000) face.weight = (int)weight;
    UInt selection = u16(os2, 62);
    face.italic = (selection & 0x0001) || (selection & 0x0200); // italic or oblique
    if (weight == 0 && (selection & 0x0020)) face.weight = 700;
  } else {
    vector<Byte> head;
    if (head_table.length >= 46 && file.read(head_table.offset, 46, head)) {
      UInt mac_style = u16(head, 44);
      if (mac_style & 1) face.weight = 700;
      face.italic = (mac_style & 2) != 0;
    }
  }
  return true;
}

} // namespace

vector<FontFace> read_font_faces(const String& path) {
  vector<FontFace> faces;
  FontFile file(path);
  if (!file.ok()) return faces;
  vector<Byte> header;
  if (!file.read(0, 12, header)) return faces;
  if (u32(header, 0) == 0x74746366 /*ttcf*/) {
    UInt num_fonts = u32(header, 8);
    vector<Byte> offsets;
    if (num_fonts > 1000 || !file.read(12, num_fonts * 4, offsets)) return faces;
    for (UInt i = 0 ; i < num_fonts ; ++i) {
      FontFace face;
      face.path  = path;
      face.index = (int)i;
      if (read_face(file, u32(offsets, i * 4), face)) faces.push_back(face);
    }
  } else {
    FontFace face;
    face.path = path;
    if (read_face(file, 0, face)) faces.push_back(face);
  }
  return faces;
}

bool is_font_file_name(const String& name) {
  String n = to_lower(name);
  return ends_with(n, _(".ttf")) || ends_with(n, _(".otf")) || ends_with(n, _(".ttc"));
}

// ----------------------------------------------------------------------------- : FontRegistry : sources

void FontRegistry::addFile(const String& path, FontSource source) {
  std::lock_guard<std::mutex> lock(mutex);
  pending.push_back(PendingSource{path, source, PendingSource::FILE});
}

void FontRegistry::addDirectory(const String& dir, FontSource source) {
  std::lock_guard<std::mutex> lock(mutex);
  pending.push_back(PendingSource{dir, source, PendingSource::DIRECTORY});
}

void FontRegistry::addTemplateDirectory(const String& dir) {
  std::lock_guard<std::mutex> lock(mutex);
  pending.push_back(PendingSource{dir, FontSource::TEMPLATE, PendingSource::TEMPLATE_DIRECTORY});
}

void FontRegistry::scanPending() {
  if (pending.empty()) return;
  // font store sources first, so they win ties
  vector<PendingSource> todo;
  todo.swap(pending);
  std::stable_sort(todo.begin(), todo.end(), [](const PendingSource& a, const PendingSource& b) {
    return a.source < b.source;
  });
  for (auto const& src : todo) {
    if (src.kind == PendingSource::FILE) {
      scanFile(src.path, src.source);
    } else if (src.kind == PendingSource::DIRECTORY) {
      scanDirectory(src.path, src.source);
    } else {
      // only the Template packages in the directory
      vector<String> names;
      list_directory(src.path, names, LIST_DIRS);
      sort(names.begin(), names.end());
      for (const String& name : names) {
        if (starts_with(file_ext(name), _("mse-"))) {
          scanDirectory(join_path(src.path, name), src.source);
        }
      }
    }
  }
  cache.clear();
}

vector<String> list_font_files(const String& dir) {
  vector<String> out, names;
  list_directory(dir, names, LIST_FILES);
  sort(names.begin(), names.end());
  for (const String& name : names) {
    if (is_font_file_name(name)) out.push_back(join_path(dir, name));
  }
  names.clear();
  list_directory(dir, names, LIST_DIRS);
  sort(names.begin(), names.end());
  for (const String& name : names) {
    vector<String> sub = list_font_files(join_path(dir, name));
    out.insert(out.end(), sub.begin(), sub.end());
  }
  return out;
}

void FontRegistry::scanDirectory(const String& dir, FontSource source) {
  for (const String& path : list_font_files(dir)) scanFile(path, source);
}

void FontRegistry::scanFile(const String& path, FontSource source) {
  if (!known_files.insert(path).second) return;
  for (FontFace& f : read_font_faces(path)) {
    size_t i = entries.size();
    entries.push_back(Entry{make_shared<FontFace>(f), source});
    const FontFace& face = *entries.back().face;
    // index each face once per distinct lower case name
    set<String> families;
    for (const String& n : face.family_names) families.insert(to_lower(n));
    for (const String& n : families) by_family[n].push_back(i);
  }
}

// ----------------------------------------------------------------------------- : FontRegistry : resolving

FontFaceP FontRegistry::resolve(const String& family, bool bold, bool italic) {
  if (family.empty()) return FontFaceP();
  std::lock_guard<std::mutex> lock(mutex);
  scanPending();
  String name = to_lower(family);
  String key = name + (bold ? _("|b") : _("|")) + (italic ? _("i") : _(""));
  auto cached = cache.find(key);
  if (cached != cache.end()) return cached->second;
  // candidates
  // Like the fontconfig-based desktop reference: family names only (a full name such as
  // "Arsenal Regular" does not match), and an unknown family gets the fallback family.
  const vector<size_t>* candidates = nullptr;
  auto it = by_family.find(name);
  if (it == by_family.end()) it = by_family.find(to_lower(fallback_family));
  if (it != by_family.end()) candidates = &it->second;
  FontFaceP best;
  if (candidates) {
    // the requested slant first, then the closest weight (the heavier one of two equally close
    // weights, the lighter one for weights below normal), then source and scan order
    int target = bold ? 700 : 400;
    long best_score = 0;
    for (size_t i : *candidates) {
      const FontFace& face = *entries[i].face;
      int distance = abs(face.weight - target);
      bool heavier = face.weight > target;
      long score = (face.italic != italic ? 1000000L : 0L)
                 + distance * 4L
                 + (distance > 0 && heavier != (target >= 400) ? 2 : 0)
                 + (entries[i].source == FontSource::FONT_STORE ? 0 : 1);
      if (!best || score < best_score) {
        best = entries[i].face;
        best_score = score;
      }
    }
  }
  cache[key] = best;
  return best;
}

vector<FontFaceP> FontRegistry::faces() {
  std::lock_guard<std::mutex> lock(mutex);
  scanPending();
  vector<FontFaceP> out;
  for (auto const& e : entries) out.push_back(e.face);
  return out;
}

FontRegistry& font_registry() {
  static FontRegistry registry;
  return registry;
}
