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
  String primary_family; ///< the family name fontconfig lists first: the typographic family (name id 16) if there is one, else the family (name id 1), preferably in English
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
  SYSTEM,     ///< the system's fonts (only the desktop test host adds these, to match its Golden images)
};

class FontRegistry {
public:
  /// Add a font file
  void addFile(const String& path, FontSource source);
  /// Add all font files in a directory and its subdirectories
  void addDirectory(const String& dir, FontSource source);
  /// Add the fonts inside the Template packages (*.mse-*) in a directory of installed Templates
  void addTemplateDirectory(const String& dir);
  /// Forget every face from these sources (e.g. when Templates or stored fonts were removed), so a
  /// later add scans them again from what is on disk
  void forget(const set<FontSource>& sources);

  /// The face for a family name, bold and italic.
  /** This copies how Pango and fontconfig choose a font for the desktop reference (ADR 0004):
   *   - A family is known only if it is some face's primary family (the name fontconfig lists
   *     first), compared case-insensitively. A secondary family name, like "Gotham Pro Black"
   *     next to "Gotham Pro", or a full name like "Arsenal Regular", is not enough.
   *   - The faces of a known family are all faces with the name among their family names (in
   *     any language); of those, the one with the requested slant and the closest weight is used.
   *   - An unknown family gets the regular, upright face of fallback_family, whatever the
   *     requested weight and slant (Pango then emboldens or slants it synthetically); null if
   *     that family isn't registered either.
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
    String     path;
  };
  std::mutex                    mutex;
  vector<PendingSource>         pending;   ///< sources not scanned yet
  set<String>                   known_files;
  vector<Entry>                 entries;
  map<String, vector<size_t>>   by_family;    ///< lower case family name -> entries
  set<String>                   primary_families; ///< lower case primary family names
  map<String, FontFaceP>        cache;        ///< resolved requests

  void scanPending();
  void scanFile(const String& path, FontSource source);
  void index(size_t i);
  void scanDirectory(const String& dir, FontSource source);
};

/// The font registry of the application
FontRegistry& font_registry();

/// Add the system's font directories to the font registry (FontSource::SYSTEM).
/** Only for the desktop test host with the Skia canvas: its Golden images were rendered with the
 *  system's fonts as well as the Template pack's (e.g. Noto Sans, the fallback, and DejaVu). The
 *  directories are those in MSE_SYSTEM_FONT_DIRS (separated by ':') if it is set, otherwise the
 *  usual ones on Linux. */
void add_system_fonts_to_registry();

/// Is this the name of a font file (.ttf, .otf, .ttc)?
bool is_font_file_name(const String& name);

/// The font files in a directory and its subdirectories, in the order the registry scans them
vector<String> list_font_files(const String& dir);
