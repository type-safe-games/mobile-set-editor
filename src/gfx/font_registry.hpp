//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

/** @file gfx/font_registry.hpp
 *  The font registry (ADR 0004, 0005): which font file a Style's font name means.
 *
 *  It catalogues the TrueType/OpenType files shipped inside installed Templates and in the
 *  shared font store (fonts a Template import found outside any Template; on the desktop: the
 *  "fonts" folders next to the executable and the directories in MSE_FONT_STORE), reading each
 *  file's own family names, weight and slant. It then resolves a family name plus bold/italic,
 *  case-insensitively, to the best matching face. Font names never reach a rendering backend
 *  that can load files directly: it gets the resolved FontFace.
 *
 *  Sources are scanned lazily, on the first resolve after they were added.
 */

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gfx/canvas.hpp>
#include <mutex>

// ----------------------------------------------------------------------------- : FontFace

/// A face in a font file
class FontFace {
public:
  String path;           ///< absolute path of the font file
  int    index = 0;      ///< index of the face in a font collection (.ttc), 0 otherwise
  String family;         ///< family name (name id 1, preferably in English)
  String full_name;      ///< full name (name id 4, preferably in English)
  vector<String> family_names; ///< all family names (name ids 1 and 16, in all languages)
  vector<String> full_names;   ///< all full names (name id 4, in all languages)
  String style_name;     ///< subfamily name (name id 2), e.g. "Bold Italic"
  int    weight = 400;   ///< weight class, 100 (thin) to 900 (black); 400 = normal, 700 = bold
  bool   italic = false; ///< italic or oblique

  bool isBold() const { return weight >= 600; }
};

/// Read the faces in a TrueType/OpenType font file (.ttf, .otf, .ttc); empty if it is not one
vector<FontFace> read_font_faces(const String& path);

// ----------------------------------------------------------------------------- : FontRegistry

/// Where a font file comes from; earlier sources win when two faces match equally well
enum class FontSource {
  FONT_STORE, ///< the shared font store
  TEMPLATE,   ///< inside an installed Template
};

class FontRegistry {
public:
  /// Add a font file
  void addFile(const String& path, FontSource source);
  /// Add all font files in a directory and its subdirectories
  void addDirectory(const String& dir, FontSource source);
  /// Add the fonts inside the Template packages (*.mse-*) in a directory of installed Templates
  void addTemplateDirectory(const String& dir);

  /// The face for a family name, bold and italic.
  /** Families match case-insensitively on the family and typographic family names (in any
   *  language), never on the full name, as fontconfig does for the desktop reference (ADR 0004).
   *  An unknown family gets fallback_family; null if that isn't registered either.
   *  Among the faces of the family, the one with the requested slant and the closest weight is used.
   */
  FontFaceP resolve(const String& family, bool bold, bool italic);

  /// Family used for unknown names; fontconfig falls back to Noto Sans in the reference environment
  String fallback_family = _("Noto Sans");

  /// All faces, after scanning
  vector<FontFaceP> faces();

private:
  struct PendingSource {
    String     path;
    FontSource source;
    enum Kind { FILE, DIRECTORY, TEMPLATE_DIRECTORY } kind;
  };
  struct Entry {
    FontFaceP  face;
    FontSource source;
  };
  std::mutex                    mutex;
  vector<PendingSource>         pending;   ///< sources not scanned yet
  set<String>                   known_files;
  vector<Entry>                 entries;
  map<String, vector<size_t>>   by_family;    ///< lower case family name -> entries
  map<String, FontFaceP>        cache;        ///< resolved requests

  void scanPending();
  void scanFile(const String& path, FontSource source);
  void scanDirectory(const String& dir, FontSource source);
};

/// The font registry of the application
FontRegistry& font_registry();

/// Is this the name of a font file (.ttf, .otf, .ttc)?
bool is_font_file_name(const String& name);
