//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#pragma once

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/error.hpp>
#include <data/settings.hpp>
#include <util/real_point.hpp>

class Game;
class Canvas;
DECLARE_POINTER_TYPE(Set);
DECLARE_POINTER_TYPE(Card);

// ----------------------------------------------------------------------------- : FileFormat

/// A filter for a specific file format
class FileFormat : public IntrusivePtrVirtualBase {
public:
  virtual ~FileFormat() {}
  /// File extension used by this file format
  virtual String extension() = 0;
  /// What to match against
  virtual String matches() {
    return _("*.") + extension();
  }
  /// Name of the filter
  virtual String name() = 0;
  /// Can it be used for importing sets?
  virtual bool canImport() = 0;
  /// Can it be used for exporting sets for a particular game?
  virtual bool canExport(const Game&) = 0;
  /// Import using this filter
  virtual SetP importSet(const String& filename) {
    throw InternalError(_("Import not supported by this file format"));
  }
  /// Export using this filter
  /** If is_copy, then the set should not be modified */
  virtual void exportSet(Set& set, const String& filename, bool is_copy = false) {
    throw InternalError(_("Export not supported by this file format"));
  }
};

using FileFormatP = unique_ptr<FileFormat>;

// ----------------------------------------------------------------------------- : Formats

/// Initialize the list of file formats
/** Must be called before any other methods of this header */
void init_file_formats();

/// List of supported import formats
/** Formated as _("All supported (type1,...)|type1,...|name|type|...|All files(*.*)|*.*").
 *  For use in file selection dialogs.
 */
String import_formats();

// List of supported export formats that a set in a specific game can be exported.
/** Similair format as importFormats, except for 'all supported' and 'all files'
 */
String export_formats(const Game& game);

/// Opens a set with the specified filename.
/** File format is chosen based on the extension, default is fileFormats[0]
 *  (which is the MSE2 file filter)
 *  throws on error, always returns a valid set
 * 
 *  NOTE: String parameter must be passed by valueso we get a copy, otherwise
 *  changing the recent set list could change the filename while we are opening it
 *  (which would be bad)
 */
SetP import_set(String name);

/// Save a set under the specified name.
/** format_index specifies what format to use for saving, used as index in the list of file formats
 */
void export_set(Set& set, const String& filename, size_t format_index, bool is_copy = false);

// ----------------------------------------------------------------------------- : The formats

FileFormatP mse1_file_format();
FileFormatP mse2_file_format();
FileFormatP mtg_editor_file_format();

// ----------------------------------------------------------------------------- : Other ways to export

/// The settings to use when rendering an image for multiple cards
enum class ExportImageMode {
  EXPORT,
  CLIPBOARD,
  DEFAULT,
};

/// Generate an image of one or more cards
/** The other face of a front/back pair (if card_settings.dfc_export) uses the app's export settings
 *  for its Style, or card_settings too if other_face_same_settings. */
Image export_image(const SetP& set, const CardP& card, bool write_metadata = true, const Settings::ExportSettings& card_settings = Settings::ExportSettings(), bool other_face_same_settings = false);
Image export_image(const SetP& set, const vector<CardP>& cards, int padding = 2, ExportImageMode mode = ExportImageMode::EXPORT);

/// Export the image of one or more cards to a given filename, using the app's zoom, rotation and bleed settings, and including metadata
void export_image(const SetP& set, const CardP& card, const String& filename);
void export_image(const SetP& set, const vector<CardP>& cards, const String& path, const String& filename_template, FilenameConflicts conflicts);

/// Render a card the way export_image does (zoom 1, no rotation or bleed) and describe its layout:
/// each field's region, and each laid-out text line. Deterministic, for exact comparison (ADR 0010).
String card_layout_dump(const SetP& set, const CardP& card);

/// A visible field's region on a card drawn by render_card
struct CardFieldRegion {
  String   field;    ///< the field's name
  RealRect rect = RealRect(0, 0, 0, 0); ///< the box the field occupies (its style's left, top, width, height), in canvas pixels
  double   angle = 0; ///< the field's rotation in degrees, counter clockwise, as in the Style
  int      z_index = 0; ///< drawing order, higher is on top
  bool     editable = false; ///< can the user edit this field?
};

/// The size of a card drawn by render_card at zoom 1 (as export_image draws it, unrotated)
RealSize card_render_size(const SetP& set, const CardP& card);

/// Draw a card unrotated at the given zoom onto a canvas of (about) card_render_size times zoom,
/// exactly like export_image without bleed, and report the visible fields' regions from the same pass.
void render_card(const SetP& set, const CardP& card, Canvas& canvas, double zoom, vector<CardFieldRegion>* regions = nullptr);

/// The editable field at a point of a card drawn by render_card (in its pixels at zoom 1), as the
/// desktop's card editor picks it: the topmost visible one whose mask (if any) is opaque there.
/// Empty if there is none.
String card_field_at(const SetP& set, const CardP& card, const RealPoint& point);

/// Write the metadata for a card
// Assuming first the zoom is applied, then the rotation, then the offset.
// This means that width and height need to be already scaled by a factor of zoom, but not already rotated
// while offset_x and offset_y need to be already scaled and already rotated.
String export_metadata(const SetP& set, const CardP& card, double zoom, Radians angle_radians, int width, int height, double offset_x, double offset_y);

/// Export a set to Magic Workstation format
void export_mws(const SetP& set, const String& filename);

