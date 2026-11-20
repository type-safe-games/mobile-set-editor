//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gfx/gfx.hpp>
#include <gfx/canvas.hpp>
#include <util/error.hpp>

void blur_image(const Image& img_in, Image& img_out);

// ----------------------------------------------------------------------------- : Resampled text

// scaling factor to use when drawing resampled text
const int text_scaling = 4;

// Draw text by first drawing it using a larger font and then downsampling it
// optionally rotated by an angle
void draw_resampled_text(Canvas& dc, const String& text, const RealPoint& pos, const RealRect& rect, Radians angle, Color color, int blur_radius, Color stroke_color, int stroke_radius, double stretch) {
  // transparent text can be ignored
  if (color.Alpha() == 0) return;
  // enlarge slightly; some fonts are larger then the GetTextExtent tells us (especially italic fonts)
  int w = static_cast<int>(rect.width) + 4, h = static_cast<int>(rect.height) + 2;
  // determine sub-pixel position
  int xi = static_cast<int>(rect.x),
      yi = static_cast<int>(rect.y);
  int xsub = static_cast<int>(text_scaling * (pos.x - xi)),
      ysub = static_cast<int>(text_scaling * (pos.y - yi));
  // draw text as mask (white text on black background)
  OffscreenCanvasP mdc = new_canvas(w * text_scaling, h * text_scaling, CanvasFormat::RGB);
  mdc->clear(Color(0,0,0));
  mdc->setFont(dc.getFont());
  mdc->setTextColor(Color(255,255,255));
  mdc->drawText(text, xsub, ysub, rad_to_deg(angle));
  Image buffer = mdc->getImage();
  mdc.reset();
  // downsample
  double ca = fabs(cos(angle)), sa = fabs(sin(angle));
  w += int(w * (stretch - 1) * ca); // GCC makes annoying conversion warnings if *= is used here.
  h += int(h * (stretch - 1) * sa);
  Image img(w, h, false);
  downsample_to_alpha(buffer, img);
  // if there is no stroke effect, just add blur and draw
  if (stroke_radius == 0) {
    if (blur_radius == 0) {
      if (color.Alpha() != 255) {
        set_alpha(img, color.Alpha() / 255.);
      }
      fill_image(img, color);
      dc.drawImage(img, xi, yi);
    }
    else {
      Image s_img = make_stroke_image(img, color, 0, blur_radius);
      dc.drawImage(s_img, xi-blur_radius, yi-blur_radius);
    }
  }
  // otherwise add stroke effect, add copy of text on top, draw
  else {
    int radius = blur_radius + stroke_radius;
    Image s_img = make_stroke_image(img, stroke_color, stroke_radius, blur_radius);
    if (stroke_color != color) {
      fill_image(img, color);
      if (color.Alpha() != 255) {
        set_alpha(img, color.Alpha() / 255.);
      }
      s_img.Paste(img, radius, radius, IMAGE_ALPHA_BLEND_COMPOSE);
    }
    dc.drawImage(s_img, xi-radius, yi-radius);
  }
}

