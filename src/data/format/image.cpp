//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gfx/canvas.hpp>
#include <gfx/image_load.hpp>
#include <util/tagged_string.hpp>
#include <data/format/formats.hpp>
#include <data/game.hpp>
#include <data/field/image.hpp>
#include <data/set.hpp>
#include <data/card.hpp>
#include <data/stylesheet.hpp>
#include <data/settings.hpp>
#include <script/functions/json.hpp>
#include <render/card/viewer.hpp>
#include <render/value/text.hpp>

static const int DEFAULT_FACE_PADDING = 2;

// ----------------------------------------------------------------------------- : Viewer

class ZoomedUnrotatedDataViewer : public DataViewer {
public:
  ZoomedUnrotatedDataViewer(double zoom, double bleed = 0.0) : zoom(zoom), bleed(bleed) {};
  virtual ~ZoomedUnrotatedDataViewer() {};
  Rotation getRotation() const override;
private:
  double zoom, bleed;
};

Rotation ZoomedUnrotatedDataViewer::getRotation() const {
  RealRect card_rect = stylesheet->getCardRect();
  RealRect bled_rect(card_rect.x + bleed, card_rect.y + bleed, card_rect.width, card_rect.height);
  return Rotation(0.0, bled_rect, zoom);
}

// ----------------------------------------------------------------------------- : Export groups

/// A card together with the settings to export it with
struct ExportPart {
  CardP card;
  Settings::ExportSettings settings;
};

/// A single card, or a front/back pair
using ExportGroup = vector<ExportPart>;

/// How to find the export settings for a card
using SettingsFor = std::function<Settings::ExportSettings(const CardP&)>;

/// Settings lookup for an ExportImageMode
static SettingsFor settings_for_mode(const SetP& set, ExportImageMode mode) {
  return [set, mode](const CardP& card) -> Settings::ExportSettings {
    switch (mode) {
      case ExportImageMode::DEFAULT:   return Settings::ExportSettings();
      case ExportImageMode::CLIPBOARD: return settings.clipboardSettingsFor(set->stylesheetFor(card));
      default:                         return settings.exportSettingsFor(set->stylesheetFor(card));
    }
  };
}

/// Group cards into single cards and front/back pairs
static vector<ExportGroup> group_faces(const SetP& set, const vector<CardP>& cards, const SettingsFor& settings_for) {
  vector<ExportGroup> groups;
  std::unordered_set<const Card*> already_added;
  for (const CardP& card : cards) {
    if (already_added.count(card.get())) continue;
    Settings::ExportSettings card_settings = settings_for(card);
    if (card_settings.dfc_export) {
      pair<CardP, CardP> faces = card->getFrontFaceBackFacePair(*set);
      if (faces.first && faces.second) {
        bool card_is_front = faces.first == card;
        const CardP& other = card_is_front ? faces.second : faces.first;
        ExportPart self{card, card_settings};
        ExportPart other_part{other, settings_for(other)};
        groups.push_back(card_is_front ? ExportGroup{self, other_part} : ExportGroup{other_part, self});
        already_added.insert(faces.first.get());
        already_added.insert(faces.second.get());
        continue;
      }
    }
    groups.push_back(ExportGroup{ExportPart{card, card_settings}});
  }
  return groups;
}

// ----------------------------------------------------------------------------- : Drawing one face

/// Render a single face: draw it, zoom, rotate and add the bleed edge.
static Image render_face(const SetP& set, const ExportPart& part) {
  const Settings::ExportSettings& face_settings = part.settings;
  // create, offset by bleed, and zoom
  int bleed = lround(face_settings.bleed_pixels);
  ZoomedUnrotatedDataViewer viewer(face_settings.zoom, bleed);
  viewer.setSet(set);
  viewer.setCard(part.card);
  RealSize size = viewer.getRotation().getExternalSize();
  OffscreenCanvasP canvas = new_canvas((int)size.width + 2 * bleed, (int)size.height + 2 * bleed, CanvasFormat::RGB);
  viewer.draw(*canvas);
  Image img = canvas->getImage();
  // rotate
  img = rotate_image(img, face_settings.angle_radians);
  // add print bleed edge (reports an error and leaves the image as is if it is too small)
  mirror_bleed_edge(img, bleed, bleed);
  return img;
}

// ----------------------------------------------------------------------------- : Layout dump

/// A number with exactly three decimals, independent of the C locale's decimal point; never "-0.000"
static String layout_number(double x) {
  long long milli = llround(x * 1000.0);
  String sign = milli < 0 ? _("-") : _("");
  if (milli < 0) milli = -milli;
  return sign + format_string(_("%lld.%03lld"), milli / 1000, milli % 1000);
}

/// A JSON string literal
static String layout_string(const String& s) {
  String out = _("\"");
  for (Char c : s) {
    if      (c == '"')  out += _("\\\"");
    else if (c == '\\') out += _("\\\\");
    else if (c == '\n') out += _("\\n");
    else if (c == '\t') out += _("\\t");
    else if (c < 0x20)  out += format_string(_("\\u%04x"), (int)c);
    else                out += c;
  }
  return out + _("\"");
}

/// The export viewer, with access to its value viewers after drawing
class LayoutDumpDataViewer : public ZoomedUnrotatedDataViewer {
public:
  LayoutDumpDataViewer() : ZoomedUnrotatedDataViewer(1.0) {}
  const vector<ValueViewerP>& getViewers() const { return viewers; }
};

String card_layout_dump(const SetP& set, const CardP& card) {
  if (!set) throw Error(_("no set"));
  if (!card) throw Error(_("no card"));
  // Draw exactly like render_face at the default export settings, so the layout read back
  // below is the one that produced the pixels, not a re-measurement.
  LayoutDumpDataViewer viewer;
  viewer.setSet(set);
  viewer.setCard(card);
  RealSize size = viewer.getRotation().getExternalSize();
  OffscreenCanvasP canvas = new_canvas((int)size.width, (int)size.height, CanvasFormat::RGB);
  viewer.draw(*canvas);
  // Fixed key order and number format; fields in drawing (z) order, one text line per output line.
  String out = _("{\n");
  out += _("  \"fields\": [");
  bool first_field = true;
  for (const ValueViewerP& v : viewer.getViewers()) {
    const Style& style = *v->getStyle();
    out += first_field ? _("\n") : _(",\n");
    first_field = false;
    out += _("    {\"angle\": ") + layout_number(style.angle);
    out += _(", \"height\": ") + layout_number(style.height);
    out += _(", \"left\": ")   + layout_number(style.left);
    out += _(", \"name\": ")   + layout_string(style.fieldP->name);
    out += _(", \"top\": ")    + layout_number(style.top);
    out += String(_(", \"visible\": ")) + (v->isVisible() ? _("true") : _("false"));
    out += _(", \"width\": ")  + layout_number(style.width);
    out += _(", \"z\": ")      + format_string(_("%d"), style.z_index);
    const TextValueViewer* tv = dynamic_cast<const TextValueViewer*>(v.get());
    if (tv && v->isVisible()) {
      out += _(", \"text_scale\": ") + layout_number(tv->getTextViewer().getScale());
      out += _(", \"lines\": [");
      bool first_line = true;
      for (const TextViewer::LineGeometry& l : tv->getTextViewer().lineGeometry()) {
        out += first_line ? _("\n") : _(",\n");
        first_line = false;
        out += _("      {\"end\": ")   + format_string(_("%d"), (int)l.end);
        out += _(", \"height\": ")     + layout_number(l.height);
        out += _(", \"left\": ")       + layout_number(l.left);
        out += _(", \"start\": ")      + format_string(_("%d"), (int)l.start);
        out += _(", \"top\": ")        + layout_number(l.top);
        out += _(", \"width\": ")      + layout_number(l.width);
        out += _("}");
      }
      out += first_line ? _("]") : _("\n    ]");
    }
    out += _("}");
  }
  out += first_field ? _("],\n") : _("\n  ],\n");
  out += _("  \"height\": ") + layout_number(size.height) + _(",\n");
  out += _("  \"stylesheet\": ") + layout_string(set->stylesheetFor(card).name()) + _(",\n");
  out += _("  \"width\": ") + layout_number(size.width) + _("\n");
  out += _("}\n");
  return out;
}

// ----------------------------------------------------------------------------- : Rendering for a host

/// The export viewer at any zoom, with access to its value viewers after drawing
class RegionDataViewer : public ZoomedUnrotatedDataViewer {
public:
  RegionDataViewer(double zoom) : ZoomedUnrotatedDataViewer(zoom) {}
  const vector<ValueViewerP>& getViewers() const { return viewers; }
};

RealSize card_render_size(const SetP& set, const CardP& card) {
  if (!set) throw Error(_("no set"));
  if (!card) throw Error(_("no card"));
  RegionDataViewer viewer(1.0);
  viewer.setSet(set);
  viewer.setCard(card);
  return viewer.getRotation().getExternalSize();
}

void render_card(const SetP& set, const CardP& card, Canvas& canvas, double zoom, vector<CardFieldRegion>* regions) {
  if (!set) throw Error(_("no set"));
  if (!card) throw Error(_("no card"));
  RegionDataViewer viewer(zoom);
  viewer.setSet(set);
  viewer.setCard(card);
  viewer.draw(canvas);
  if (!regions) return;
  // the regions as this drawing pass laid them out, in drawing (z) order
  regions->clear();
  for (const ValueViewerP& v : viewer.getViewers()) {
    if (!v->isVisible()) continue;
    const Style& style = *v->getStyle();
    CardFieldRegion r;
    r.field    = style.fieldP->name;
    r.rect     = RealRect(style.left * zoom, style.top * zoom, style.width * zoom, style.height * zoom);
    r.angle    = style.angle;
    r.z_index  = style.z_index;
    r.editable = style.fieldP->editable;
    regions->push_back(r);
  }
}

// ----------------------------------------------------------------------------- : Stitching faces

/// Paste images side by side onto one transparent canvas, and report the x offset of each.
static Image stitch_row(const vector<Image>& imgs, int padding, vector<int>& offsets) {
  offsets.clear();
  if (imgs.size() == 1) {
    offsets.push_back(0);
    return imgs[0];
  }
  int global_width = 0;
  int global_height = 0;
  for (const Image& img : imgs) {
    offsets.push_back(global_width);
    global_width += padding + img.GetWidth();
    global_height = max(global_height, img.GetHeight());
  }
  global_width -= padding;
  Image global_img = Image(global_width, global_height);
  if (!global_img.Ok()) throw InternalError(_("Unable to create image"));
  global_img.InitAlpha();
  Byte* pixels = global_img.GetData();
  Byte* alpha = global_img.GetAlpha();
  // fill with transparent
  for (int i = 0; i < global_width*global_height; ++i) {
    pixels[3 * i + 0] = 0;
    pixels[3 * i + 1] = 0;
    pixels[3 * i + 2] = 0;
    alpha[i] = 0;
  }
  // paste card images
  for (size_t i = 0; i < imgs.size(); ++i) {
    global_img.Paste(imgs[i], offsets[i], 0);
  }
  return global_img;
}

/// The metadata describing all faces in a row, as stored in a png description
static String row_metadata(const SetP& set, const vector<ExportPart>& parts, const vector<Image>& imgs, const vector<int>& offsets) {
  String metadata = _("<mse-card-data>[");
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i > 0) metadata += _(",");
    const Settings::ExportSettings& face_settings = parts[i].settings;
    int width = imgs[i].GetWidth(), height = imgs[i].GetHeight();
    bool rotated = is_rad90(face_settings.angle_radians) || is_rad270(face_settings.angle_radians); // we stored width and height after rotation, but export_metadata expects them before rotation
    metadata += export_metadata(set, parts[i].card, face_settings.zoom, face_settings.angle_radians, rotated ? height : width, rotated ? width : height, face_settings.bleed_pixels + offsets[i], face_settings.bleed_pixels);
  }
  metadata += _("]</mse-card-data>");
  return metadata;
}

/// Render faces side by side in one image, optionally with metadata.
static Image render_row(const SetP& set, const vector<ExportPart>& parts, int padding, bool write_metadata) {
  if (!set) throw Error(_("no set"));
  if (parts.empty()) throw Error(_("no cards"));
  vector<Image> imgs;
  for (const ExportPart& part : parts) {
    imgs.push_back(render_face(set, part));
  }
  vector<int> offsets;
  Image img = stitch_row(imgs, padding, offsets);
  if (write_metadata) {
    img.SetMetadata(row_metadata(set, parts, imgs, offsets));
  }
  return img;
}

// ----------------------------------------------------------------------------- : Image export

Image export_image(const SetP& set, const CardP& card, bool write_metadata, const Settings::ExportSettings& card_settings, bool other_face_same_settings) {
  if (!set) throw Error(_("no set"));
  // this card, plus its other face if card_settings ask for it
  vector<CardP> cards{card};
  SettingsFor settings_for = [&](const CardP& c) -> Settings::ExportSettings {
    return c == card || other_face_same_settings ? card_settings : settings.exportSettingsFor(set->stylesheetFor(c));
  };
  ExportGroup group = group_faces(set, cards, settings_for).front();
  return render_row(set, group, DEFAULT_FACE_PADDING, write_metadata);
}

Image export_image(const SetP& set, const vector<CardP>& cards, int padding, ExportImageMode mode) {
  if (!set) throw Error(_("no set"));
  if (cards.size() == 0) throw Error(_("no cards"));
  // front and back faces next to each other, missing faces added
  vector<ExportPart> parts;
  for (const ExportGroup& group : group_faces(set, cards, settings_for_mode(set, mode))) {
    parts.insert(parts.end(), group.begin(), group.end());
  }
  return render_row(set, parts, padding, true);
}

// ----------------------------------------------------------------------------- : File export

static void save_image(const Image& img, const String& filename) {
  if (!retry_io([&]{ return image_save_file(img, filename); })) {
    throw Error(_("Unable to write image file '") + filename + _("'"));
  }
}

void export_image(const SetP& set, const CardP& card, const String& filename) {
  const StyleSheet& stylesheet = set->stylesheetFor(card);
  Settings::ExportSettings export_settings = settings.exportSettingsFor(stylesheet);
  save_image(export_image(set, card, true, export_settings), filename);
}

static String export_filename(const SetP& set, const ExportGroup& group, const Script& filename_script) {
  String result, ext;
  for (size_t i = 0; i < group.size(); ++i) {
    Context& ctx = set->getContext(group[i].card);
    String name = clean_filename(untag(ctx.eval(filename_script)->toString()));
    if (name.empty()) return String(); // no filename -> no saving
    if (i == 0) ext = file_ext(name); else result += _(" -- ");
    result += file_name_no_ext(name);
  }
  if (!ext.empty()) result += _(".") + ext;
  return result;
}

void export_image(const SetP& set, const vector<CardP>& cards, const String& path, const String& filename_template, FilenameConflicts conflicts) {
  // Script
  ScriptP filename_script = parse(filename_template, nullptr, true);
  // Export, one file per card or front/back pair
  std::set<String> used; // for CONFLICT_NUMBER_OVERWRITE
  for (const ExportGroup& group : group_faces(set, cards, settings_for_mode(set, ExportImageMode::EXPORT))) {
    // filename for this group
    String name = export_filename(set, group, *filename_script);
    if (name.empty()) continue; // no filename -> no saving
    // full path
    String filename = replace_file_name(path, name);
    // does the file exist?
    if (!resolve_filename_conflicts(filename, conflicts, used)) continue;
    // write image
    used.insert(filename);
    save_image(render_row(set, group, DEFAULT_FACE_PADDING, true), filename);
  }
}

// ----------------------------------------------------------------------------- : Metadata

String export_metadata(const SetP& set, const CardP& card, double zoom, Radians angle_radians, int width, int height, double offset_x, double offset_y) {
  IndexMap<FieldP, ValueP>& card_data = card->data;
  boost::json::object cardv = mse_to_json(card, set.get());
  StyleSheetP stylesheet = set->stylesheetForP(card);
  if (!settings.stylesheetSettingsFor(*stylesheet).card_notes_export()) cardv["notes"] = "";
  RealRect bounds_rect = RealRect(0, 0, width, height);
  int bounds_degrees = 0;
  RealRect::rotate(bounds_rect, bounds_degrees, width, height, lround(rad_to_deg(angle_radians)));
  RealRect::translate(bounds_rect, bounds_degrees, offset_x, offset_y);
  cardv.emplace("bounds", encodeRectInStdString(bounds_rect, bounds_degrees));
  if (!cardv.contains("data")) {
    cardv["data"] = boost::json::object();
  }
  // cardv.emplace may trigger a re-allocation, so only take a reference
  // to an object inside the map AFTER we're done emplacing
  boost::json::object& cardv_data = cardv["data"].as_object();
  // iterate over all image fields
  for (IndexMap<FieldP, ValueP>::iterator it = card_data.begin(); it != card_data.end(); ++it) {
    ImageValue* value = dynamic_cast<ImageValue*>(it->get());
    if (value && !value->filename.empty()) {
      FieldP field = (*it)->fieldP;
      ImageStyle* style = dynamic_cast<ImageStyle*>(stylesheet->card_style.at(field->index).get());
      if (style) {
        style->update(set->getContext(card));
        // store the entire image in the metadata
        if (style->store_in_metadata() && settings.stylesheetSettingsFor(*stylesheet).card_metaimage_export()) {
          Image img = value->getImage(set);
          cardv_data[encode_utf8(field->name)] = encodeImageInString(img);
        }
        // store only crop coordinates
        else {
          RealRect rect = style->getCanonicalExternalRect();
          int degrees = lround(style->angle());
          RealRect::scale(rect, degrees, zoom, zoom);
          RealRect::rotate(rect, degrees, width, height, lround(rad_to_deg(angle_radians))); // width and height are already scaled
          RealRect::translate(rect, degrees, offset_x, offset_y); // offset_x and offset_y are already scaled and rotated
          cardv_data[encode_utf8(field->name)] = encodeRectInStdString(rect, degrees);
        }
      }
    }
  }
  return json_ugly_print(cardv);
}
