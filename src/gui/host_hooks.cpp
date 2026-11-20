//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make card games                |
//| Copyright:    (C) Twan van Laarhoven and the other MSE developers          |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gui/host_hooks.hpp>
#include <gui/web_request_window.hpp>
#include <gui/new_window.hpp>
#include <gui/set/window.hpp>
#include <cli/text_io_handler.hpp>
#include <data/settings.hpp>
#include <data/stylesheet.hpp>
#include <data/installer.hpp>
#include <gfx/generated_image.hpp>
#include <gfx/color.hpp>
#include <gfx/wx_image_codec.hpp>
#include <gfx/wx_canvas.hpp>
#include <gfx/font_registry.hpp>
#include <util/file_utils.hpp>
#include <util/error.hpp>
#include <wx/webrequest.h>
#include <wx/evtloop.h>
#include <wx/timer.h>

// ----------------------------------------------------------------------------- : Downloading files

static DownloadResult download_file(const String& url, OutputStream& out, const DownloadProgressCallback& progress) {
  wxEvtHandler handler;
  wxWebRequest request = wxWebSession::GetDefault().CreateRequest(&handler, url);
  if (!request.IsOk()) return DownloadResult::FAILED;
  request.SetStorage(wxWebRequest::Storage_None);
  // state shared with the event handlers below
  bool write_failed  = false; // could not write to the temporary file
  bool unauthorized  = false; // the server wants credentials, we have none
  bool user_aborted  = false; // the progress callback asked us to stop
  bool cancel_sent   = false;
  bool finished      = false; // the request has reached a final state
  bool in_callback   = false; // guard against re-entrancy, the callback may process events
  wxEventLoop loop;
  handler.Bind(wxEVT_WEBREQUEST_DATA, [&](wxWebRequestEvent& ev) {
    if (write_failed) return;
    if (!out.write(ev.GetDataBuffer(), ev.GetDataSize())) write_failed = true;
  });
  handler.Bind(wxEVT_WEBREQUEST_STATE, [&](wxWebRequestEvent& ev) {
    switch (ev.GetState()) {
      case wxWebRequest::State_Unauthorized:
        unauthorized = true; // cancelled by the timer
        break;
      case wxWebRequest::State_Completed:
      case wxWebRequest::State_Failed:
      case wxWebRequest::State_Cancelled:
        finished = true;
        loop.Exit();
        break;
      default:
        break;
    }
  });
  // report progress and handle cancelling from a timer
  wxTimer timer(&handler);
  handler.Bind(wxEVT_TIMER, [&](wxTimerEvent&) {
    if (finished || cancel_sent || in_callback) return;
    if (write_failed || unauthorized) {
      cancel_sent = true;
      request.Cancel();
      return;
    }
    if (progress) {
      in_callback = true;
      bool keep_going = progress(request.GetBytesReceived(), request.GetBytesExpectedToReceive());
      in_callback = false;
      if (!keep_going && !finished) {
        user_aborted = true;
        cancel_sent  = true;
        request.Cancel();
      }
    }
  });
  // go
  request.Start();
  timer.Start(100);
  loop.Run();
  timer.Stop();
  // check the result
  bool ok = !user_aborted && !write_failed
         && request.GetState() == wxWebRequest::State_Completed
         && request.GetResponse().IsOk()
         && request.GetResponse().GetStatus() < 400;
  if (user_aborted) return DownloadResult::ABORTED;
  return ok ? DownloadResult::OK : DownloadResult::FAILED;
}

// ----------------------------------------------------------------------------- : Running programs

static int run_program(const vector<String>& arguments, bool wait) {
  if (arguments.empty()) return -1;
  vector<const wxChar*> argv;
  for (const String& a : arguments) argv.push_back(a.c_str());
  argv.push_back(nullptr);
  long result = wxExecute(const_cast<wxChar**>(argv.data()), wait ? wxEXEC_SYNC : wxEXEC_ASYNC);
  if (wait) return (int)result;
  return result == 0 ? -1 : 0; // async: result is the process id, 0 on failure
}

// ----------------------------------------------------------------------------- : Installing hooks

/// Adds font file paths from the given directory into fontFilePaths
static void tally_resource_fonts(String fontsDirectoryPath, vector<String>& fontFilePaths, bool recursive) {
  vector<String> fontFileNames;
  list_directory(fontsDirectoryPath, fontFileNames);
  for (const String& fontFileName : fontFileNames) {
    String fontFilePath = fontsDirectoryPath + fontFileName;
    if (dir_exists(fontFilePath)) {
      if (recursive) {
        tally_resource_fonts(fontFilePath + path_separator(), fontFilePaths, true);
      }
    }
    else if (ends_with(fontFilePath, _(".ttf")) || ends_with(fontFilePath, _(".otf"))) {
      fontFilePaths.push_back(fontFilePath);
    }
  }
}

bool preload_resource_fonts(bool recursive) {
  vector<String> wxFontFilePaths; // all added at the end
  // the font store directories from the environment: for the font registry, and for wx, which
  // selects fonts by family name (an imported Template pack's loose fonts are only found there)
  if (const char* store = std::getenv("MSE_FONT_STORE")) {
    String dirs = decode_utf8_or_latin1(store, strlen(store));
    size_t start = 0;
    while (start <= dirs.size()) {
      size_t end = dirs.find(wxPATH_SEP[0], start);
      if (end == String::npos) end = dirs.size();
      if (end > start) {
        String dir = dirs.substr(start, end - start);
        font_registry().addDirectory(dir, FontSource::FONT_STORE);
        vector<String> paths = list_font_files(dir);
        wxFontFilePaths.insert(wxFontFilePaths.end(), paths.begin(), paths.end());
      }
      start = end + 1;
    }
  }
  String pathSeparator(1, path_separator());
  String appPath(path_only(executable_path()));
  vector<String> folders;
  if (!list_directory(appPath, folders, LIST_DIRS)) {
    add_wx_font_files(wxFontFilePaths);
    return true;
  }

  for (const String& folder : folders)
  {
    if (contains(to_lower(folder), _("fonts"))) {
      String folderPath = appPath + pathSeparator + folder + pathSeparator;
      
      // tally fonts
      vector<String> fontFilePaths;
      tally_resource_fonts(folderPath, fontFilePaths, recursive);

      // load fonts
      for (const String& fontFilePath : fontFilePaths) {
        font_registry().addFile(fontFilePath, FontSource::FONT_STORE);
        wxFontFilePaths.push_back(fontFilePath);
      }
    }
  }

  return !add_wx_font_files(wxFontFilePaths);
}

void install_desktop_hooks() {
  set_image_codec(make_wx_image_codec()); // (needs wxInitAllImageHandlers)
  set_canvas_factory(make_unique<WxCanvasFactory>());
  error_sink = [](MessageType type, String const& msg) {
    if (cli.haveConsole()) {
      // always write to stderr if possible
      cli.show_message(type, msg);
      return true; // TODO: is this the right thing to do?
    }
    if (type == MESSAGE_FATAL_ERROR) {
      // bring this to the user's attention right now!
      wxMessageBox(msg, _("Error"), wxOK | wxICON_ERROR);
    }
    return false;
  };
  settings.dark_mode_hook = [] { return wxSystemSettings::GetAppearance().IsDark(); };
  ui_color_hook = [](UIColor which) -> Color {
    switch (which) {
      case UIColor::WINDOW:        return wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
      case UIColor::WINDOW_TEXT:   return wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
      case UIColor::GRAY_TEXT:     return wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT);
      case UIColor::APP_WORKSPACE: return wxSystemSettings::GetColour(wxSYS_COLOUR_APPWORKSPACE);
    }
    return Color();
  };
  settings.on_recent_sets_changed = SetWindow::updateRecentSetsInAllWindows;
  StyleSheet::select_alternative = [](const Game& game, const String& failed_name, const String& failed_dep) {
    return select_stylesheet(game, failed_name, failed_dep);
  };
  download_image_hook = [](const String& url, ImageDownload& out) {
    WebRequestWindow wnd(url);
    if (wnd.ShowModal() != wxID_OK) return false;
    out.content_type = wnd.content_type;
    out.image = wnd.image_out;
    return true;
  };
  download_file_hook = download_file;
  run_command_hook = run_program;
}
