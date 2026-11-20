//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gui/util.hpp>
#include <util/error.hpp>
#include <util/rotation.hpp>
#include <util/file_utils.hpp>
#include <wx/renderer.h>
#include <wx/progdlg.h>
#include <wx/stdpaths.h>
#include <wx/mstream.h>
#include <gfx/gfx.hpp>
#include <gfx/wx_image_adapter.hpp>

#if wxUSE_UXTHEME && defined(__WXMSW__)
  #include <wx/msw/uxtheme.h>
  #if defined(HAVE_VSSYM32)
    #include <vssym32.h>
  #else
    #include <tmschema.h>
  #endif
  #include <shlobj.h>
  #include <wx/mstream.h>
#endif

// ----------------------------------------------------------------------------- : Window related

// Id of the control that has the focus, or -1 if no control has the focus
int focused_control(const Window* window) {
  Window* focused_window = wxWindow::FindFocus();
  // is this window actually inside this panel?
  if (focused_window && wxWindow::FindWindowById(focused_window->GetId(), window) == focused_window) {
    return focused_window->GetId();
  } else {
    return -1; // no window has the focus, or it has a different parent/ancestor
  }
}

void set_status_text(Window* wnd, const String& s) {
  while (wnd) {
    wxFrame* f = dynamic_cast<wxFrame*>(wnd);
    if (f) {
      f->SetStatusText(s);
      return;
    }
    wnd = wnd->GetParent();
  }
}


// ----------------------------------------------------------------------------- : set_help_text
// The idea is as follows:
//  - store the help text of a window in its ClientObject as a StoredStatusString
//  - Connect event handlers that use set_status_text
//  - The event handlers should be members of an EvtHandler somewhere,
//     but it is wasteful to make an object. Instead use nullptr as a 'fake' EvtHandler.
//     then the event handling functions will be called with this==nullptr

struct StoredStatusString : public wxClientData {
  String s;
};

// Don't use this!
struct FakeEvtHandlerClass : public wxEvtHandler {
  void onControlEnter(wxMouseEvent& ev) {
    wxWindow* wnd = (wxWindow*)ev.GetEventObject();
    if (wnd) {
      StoredStatusString* d = static_cast<StoredStatusString*>(wnd->GetClientObject());
      set_status_text(wnd, d->s);
    }
    ev.Skip();
  }
  void onControlLeave(wxMouseEvent& ev) {
    set_status_text((wxWindow*)ev.GetEventObject(), _(""));
    ev.Skip();
  }
};

void set_help_text(Window* wnd, const String& s) {
  StoredStatusString* d = static_cast<StoredStatusString*>(wnd->GetClientObject());
  if (d) {
    // already called before
  } else {
    // first time
    d = new StoredStatusString;
    wnd->SetClientObject(d);
    wnd->Connect(wxEVT_ENTER_WINDOW,wxMouseEventHandler(FakeEvtHandlerClass::onControlEnter),nullptr,nullptr);
    wnd->Connect(wxEVT_LEAVE_WINDOW,wxMouseEventHandler(FakeEvtHandlerClass::onControlLeave),nullptr,nullptr);
  }
  d->s = s;
}


// ----------------------------------------------------------------------------- : Progress

function<bool(int done, int total)> import_cards_progress() {
  auto dialog = make_shared<unique_ptr<wxProgressDialog>>();
  return [dialog](int done, int total) {
    if (!*dialog) {
      *dialog = make_unique<wxProgressDialog>(
        _TITLE_("importing cards"),
        wxString::Format(wxString(_LABEL_2_("importing cards", int_to_string(1), int_to_string(total)))),
        total,
        nullptr,
        wxPD_APP_MODAL | wxPD_AUTO_HIDE | wxPD_CAN_ABORT
      );
    }
    return (*dialog)->Update(done, wxString::Format(wxString(_LABEL_2_("importing cards", int_to_string(done+1), int_to_string(total)))));
  };
}

// ----------------------------------------------------------------------------- : Tool/menu bar

wxImage generate_disabled_image(wxImage const& image) {
  Image imgOut = from_wx_image(image.ConvertToGreyscale());
  set_alpha(imgOut, 0.33);
  return to_wx_image(imgOut);
}

void set_menu_item_image(wxMenuItem* item, const String& resource) {
  #if defined(__WXGTK__)
    if (item->GetKind() == wxITEM_CHECK) return; // check items can't have icons
  #endif
  wxImage bitmap = load_resource_tool_image(resource);
  item->SetBitmap(bitmap);
}

wxMenuItem* make_menu_item(wxMenu* menu, int id, const String& resource, const String& text, const String& help, wxItemKind kind, wxMenu* submenu) {
  wxMenuItem* item = new wxMenuItem(menu, id, text, help, kind, submenu);
  if (!resource.empty()) set_menu_item_image(item, resource);
  return item;
}
wxMenuItem* make_menu_item_tr(wxMenu* menu, int id, const String& resource, const String& locale_key, wxItemKind kind, wxMenu* submenu) {
  wxMenuItem* item = new wxMenuItem(menu, id, tr(LOCALE_CAT_MENU, locale_key), tr(LOCALE_CAT_HELP, locale_key), kind, submenu);
  if (!resource.empty()) set_menu_item_image(item, resource);
  return item;
}
wxMenuItem* add_menu_item(wxMenu* menu, int id, const String& resource, const String& text, const String& help, wxItemKind kind, wxMenu* submenu) {
  wxMenuItem* item = make_menu_item(menu, id, resource, text, help, kind, submenu);
  menu->Append(item);
  return item;
}
wxMenuItem* add_menu_item_tr(wxMenu* menu, int id, const String& resource, const String& locale_key, wxItemKind kind, wxMenu* submenu) {
  wxMenuItem* item = make_menu_item_tr(menu, id, resource, locale_key, kind, submenu);
  menu->Append(item);
  return item;
}

wxToolBarToolBase* add_tool(wxToolBar* toolbar, int id, const String& resource, const String& label, const String& tooltip, const String& help, wxItemKind kind) {
  if (!resource.empty()) {
    // Note: the bitmap must match the toolbar bitmap size, otherwise disabled and normal bitmap look different
    // if the size doesn't match, we center the image
    auto size = toolbar->GetToolBitmapSize();
    wxImage bitmap = load_resource_tool_image(resource);
    if (bitmap.GetSize() != size) {
      wxPoint pos((size.GetWidth() - bitmap.GetWidth()) / 2, (size.GetHeight() - bitmap.GetHeight()) / 2);
      bitmap.Resize(size, pos);
    }
    #if defined(__WXGTK__)
      wxBitmap disabled_bitmap = wxNullBitmap; // gtk can produce decent disabled bitmaps by itself
    #else
      wxImage disabled_bitmap = generate_disabled_image(bitmap);
    #endif
    return toolbar->AddTool(id, label, bitmap, disabled_bitmap, kind, tooltip, help);
  } else {
    return toolbar->AddTool(id, label, wxNullBitmap, wxNullBitmap, kind, tooltip,  help);
  }
}

wxToolBarToolBase* add_tool_tr(wxToolBar* toolbar, int id, const String& resource, const String& locale_key, bool label, wxItemKind kind) {
  return add_tool(toolbar, id, resource, label ? tr(LOCALE_CAT_TOOL,locale_key) : String(), tr(LOCALE_CAT_TOOLTIP, locale_key), tr(LOCALE_CAT_HELP, locale_key), kind);
}

// ----------------------------------------------------------------------------- : Resource related

wxCursor load_resource_cursor(const String& name) {
  #if defined(__WXMSW__) && !defined(__GNUC__)
    wxLogNull noLog;
    return wxCursor(_("cursor/") + name, wxBITMAP_TYPE_CUR_RESOURCE);
  #else
    return wxCursor(load_resource_wx_image(_("cursor/") + name));
  #endif
}

wxIcon load_resource_icon(const String& name) {
  wxLogNull noLog;
  #if defined(__WXMSW__) && !defined(__GNUC__)
    return wxIcon(_("icon/") + name);
  #else
    static String path = wxStandardPaths::Get().GetDataDir().ToStdWstring() + _("/resource/icon/");
    static String local_path = wxStandardPaths::Get().GetUserDataDir().ToStdWstring() + _("/resource/icon/");
    String icon_path = wxFileExists(path + name + _(".ico")) ? (path + name + _(".ico")) : (local_path + name + _(".ico"));
    wxIcon icon;
    retry_io([&]{ icon = wxIcon(icon_path, wxBITMAP_TYPE_ICO); return icon.IsOk(); });
    return icon;
  #endif
}

wxImage load_resource_wx_image(const String& name) {
  return to_wx_image(load_resource_image(name));
}

wxImage load_resource_tool_image(const String& name) {
  return load_resource_wx_image(_("tool/") + name);
}


#if defined(_UNICODE) && defined(_MSC_VER) && _MSC_VER >= 1400
// manifest to use new-style controls in Windows Vista / Windows 7
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif

// ----------------------------------------------------------------------------- : Platform look

#if wxUSE_UXTHEME && defined(__WXMSW__)
RECT msw_rect(wxRect const& rect, int dl = 0, int dt = 0, int dr = 0, int db=0) {
  RECT r;
  r.left = rect.x - dl;
  r.top = rect.y - dt;
  r.right = rect.x + rect.width + dr;
  r.bottom = rect.y + rect.height + db;
  return r;
}
#endif

// Draw a basic 3D border
void draw3DBorder(wxDC& dc, int x1, int y1, int x2, int y2) {
  dc.SetPen(wxSystemSettings::GetColour(wxSYS_COLOUR_3DDKSHADOW));
  dc.DrawLine(x1, y1, x2, y1);
  dc.DrawLine(x1, y1, x1, y2);
  dc.SetPen(wxSystemSettings::GetColour(wxSYS_COLOUR_3DSHADOW));
  dc.DrawLine(x1-1, y1-1, x2+1, y1-1);
  dc.DrawLine(x1-1, y1-1, x1-1, y2+1);
  dc.SetPen(wxSystemSettings::GetColour(wxSYS_COLOUR_3DLIGHT));
  dc.DrawLine(x1, y2, x2, y2);
  dc.DrawLine(x2, y1, x2, y2+1);
  dc.SetPen(wxSystemSettings::GetColour(wxSYS_COLOUR_3DHIGHLIGHT));
  dc.DrawLine(x1-1, y2+1, x2+1, y2+1);
  dc.DrawLine(x2+1, y1-1, x2+1, y2+2);
}

void draw_control_box(Window* win, wxDC& dc, const wxRect& rect, bool focused, bool enabled) {
  #if wxUSE_UXTHEME && defined(__WXMSW__)
    RECT r = msw_rect(rect, 1,1,1,1);
    if (wxUxThemeIsActive()) {
      HTHEME hTheme = (HTHEME)::OpenThemeData(GetHwndOf(win), VSCLASS_EDIT);
      if (hTheme) {
        ::DrawThemeBackground(
          hTheme,
          (HDC)dc.GetHDC(),
          EP_EDITTEXT,
          !enabled ? ETS_DISABLED : focused ? ETS_NORMAL : ETS_NORMAL,
          &r,
          NULL
        );
        return;
      }
    }
  #endif
  // otherwise, draw a standard border
  // clear the background
  dc.SetPen(*wxTRANSPARENT_PEN);
  dc.SetBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
  dc.DrawRectangle(rect);
  // draw the border
  #if wxUSE_UXTHEME && defined(__WXMSW__)
    r.left   = rect.x - 2;
    r.top    = rect.y - 2;
    r.right  = rect.x + rect.width  + 2;
    r.bottom = rect.y + rect.height + 2;
    DrawEdge((HDC)dc.GetHDC(), &r, EDGE_SUNKEN, BF_RECT);
  #else
    // draw a 3D border
    draw3DBorder(dc, rect.x - 1, rect.y - 1, rect.x + rect.width, rect.y + rect.height);
  #endif
}

void draw_button(Window* win, wxDC& dc, const wxRect& rect, bool focused, bool down, bool enabled) {
  #if wxVERSION_NUMBER >= 2700
    wxRendererNative& rn = wxRendererNative::GetDefault();
    rn.DrawPushButton(win, dc, rect, (focused ? wxCONTROL_FOCUSED : 0) | (down ? wxCONTROL_PRESSED : 0) | (enabled ? 0 : wxCONTROL_DISABLED));
  #else
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
    dc.DrawRectangle(rect.x, rect.y, rect.width, rect.height);
    dc.SetPen(wxSystemSettings::GetColour(down ? wxSYS_COLOUR_BTNSHADOW : wxSYS_COLOUR_BTNHIGHLIGHT));
    dc.DrawLine(rect.x,rect.y,rect.x+rect.width,rect.y);
    dc.DrawLine(rect.x,rect.y,rect.x,rect.y+rect.height);
    dc.SetPen(wxSystemSettings::GetColour(down ? wxSYS_COLOUR_BTNHIGHLIGHT : wxSYS_COLOUR_BTNSHADOW));
    dc.DrawLine(rect.x+rect.width-1,rect.y,rect.x+rect.width-1,rect.y+rect.height);
    dc.DrawLine(rect.x,rect.y+rect.height-1,rect.x+rect.width,rect.y+rect.height-1);
  #endif
}

// portable, based on wxRendererGeneric::DrawComboBoxDropButton
void draw_menu_arrow(Window* win, wxDC& dc, const wxRect& rect, bool active) {
  wxPoint pt[] =
    {  wxPoint(0, 0)
    ,  wxPoint(4, 4)
    ,  wxPoint(0, 8)
    };
  dc.SetPen(*wxTRANSPARENT_PEN);
  dc.SetBrush(wxSystemSettings::GetColour(active ? wxSYS_COLOUR_HIGHLIGHTTEXT : wxSYS_COLOUR_WINDOWTEXT));
  dc.DrawPolygon(3, pt, rect.x + rect.width - 6, rect.y + (rect.height - 9) / 2);
}

void draw_drop_down_arrow(Window* win, wxDC& dc, const wxRect& rect, bool active) {
  wxRendererNative& rn = wxRendererNative::GetDefault();
  int w = wxSystemSettings::GetMetric(wxSYS_VSCROLL_ARROW_X); // drop down arrow is same size
  if (w == -1) {
    w = wxSystemSettings::GetMetric(wxSYS_VSCROLL_X); // Try just the scrollbar, then.
  }
  rn.DrawComboBoxDropButton(win, dc,
    wxRect(rect.x + rect.width - w, rect.y, w, rect.height)
    , active ? wxCONTROL_PRESSED : 0);
}

void clearDC(wxDC& dc, const Color& color) {
  WxCanvas(dc).clear(color);
}

void draw_checkbox(const Window* win, wxDC& dc, const wxRect& rect, bool checked, bool enabled) {
  #if defined(__WXMSW__)
    if (win == nullptr) win = dc.GetWindow();
  #endif
  if (win) {
    wxRendererNative& rn = wxRendererNative::GetDefault();
    rn.DrawCheckBox(const_cast<wxWindow*>(win), dc, rect, (checked ? wxCONTROL_CHECKED : 0) | (enabled ? 0 : wxCONTROL_DISABLED));
    return;
  } else {
    WxCanvas canvas(dc);
    draw_checkbox_portable(canvas, rect, checked, enabled);
  }
}

void draw_radiobox(const Window* win, wxDC& dc, const wxRect& rect, bool checked, bool enabled) {
  #if defined(__WXMSW__)
    if (win == nullptr) win = dc.GetWindow();
  #endif
  if (win) {
    wxRendererNative& rn = wxRendererNative::GetDefault();
    rn.DrawRadioBitmap(const_cast<wxWindow*>(win), dc, rect, (checked ? wxCONTROL_CHECKED : 0) | (enabled ? 0 : wxCONTROL_DISABLED));
    return;
  } else {
    // circle drawing on windows looks absolutely horrible
    // so use rounded rectangles instead
    dc.SetPen(wxSystemSettings::GetColour(enabled ? wxSYS_COLOUR_WINDOWTEXT: wxSYS_COLOUR_GRAYTEXT));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    //dc.DrawEllipse(rect.x, rect.y, rect.width, rect.height);
    dc.DrawRoundedRectangle(rect.x, rect.y, rect.width, rect.height, rect.width*0.5-1);
    if (checked) {
      dc.SetBrush(wxSystemSettings::GetColour(enabled ? wxSYS_COLOUR_WINDOWTEXT: wxSYS_COLOUR_GRAYTEXT));
      dc.SetPen(*wxTRANSPARENT_PEN);
      //dc.DrawEllipse(rect.x+2,rect.y+2,rect.width-4,rect.height-4);
      dc.DrawRoundedRectangle(rect.x+3, rect.y+3, rect.width-6, rect.height-6, rect.width*0.5-4);
    }
  }
}

void draw_selection_rectangle(Window* win, wxDC& dc, const wxRect& rect, bool selected, bool focused, bool hot) {
  #if wxUSE_UXTHEME && defined(__WXMSW__)
    HTHEME hTheme = (HTHEME)::OpenThemeData(GetHwndOf(win), VSCLASS_LISTVIEW);
    if (hTheme) {
      RECT r = msw_rect(rect);
      ::DrawThemeBackground(
        hTheme,
        (HDC)dc.GetHDC(),
        LVP_LISTITEM,
        hot&&selected ? LISS_HOTSELECTED : hot ? LISS_HOT :selected&&focused ? LISS_SELECTED : selected ? LISS_SELECTEDNOTFOCUS : LISS_NORMAL,
        &r,
        NULL
      );
      return;
    }
  #endif
}

void enable_themed_selection_rectangle(Window* win) {
  #if wxUSE_UXTHEME && defined(__WXMSW__)
    if (wxUxThemeIsActive()) {
      ::SetWindowTheme((HWND)win->GetHWND(), L"Explorer", NULL);
    }
  #endif
}
