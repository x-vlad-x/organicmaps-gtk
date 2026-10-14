// Remote control for testing on the phone without touching it:
//   gdbus call --session --dest io.github.x_vlad_x.Lichen.Devel
//     --object-path /io/github/x_vlad_x/Lichen/Devel
//     --method org.gtk.Actions.Activate debug "[<'tap 200 400'>]" "{}"
// Commands: tap X Y | longtap X Y | drag X1 Y1 X2 Y2 | search-row N | center LAT LON ZOOM |
//           location LAT LON [SPEED BEARING] | back | key NAME

#include "debug.hpp"

#include "main_window.hpp"
#include "routing.hpp"
#include "search.hpp"
#include "settings.hpp"

#include "map/framework.hpp"

#include "drape_frontend/user_event_stream.hpp"

#include "geometry/mercator.hpp"

#include "base/logging.hpp"

#include <sstream>

namespace lichen
{
namespace
{
void Touch(df::TouchEvent::ETouchType type, double x, double y, double scale)
{
  df::TouchEvent e;
  e.SetTouchType(type);
  df::Touch t;
  t.m_id = 1000;
  t.m_location = m2::PointF(static_cast<float>(x * scale), static_cast<float>(y * scale));
  e.SetFirstTouch(t);
  e.SetFirstMaskedPointer(0);
  GetFramework().TouchEvent(e);
}

void Later(guint ms, std::function<void()> fn)
{
  auto * heap = new std::function<void()>(std::move(fn));
  g_timeout_add_full(G_PRIORITY_DEFAULT, ms, [](gpointer d) -> gboolean
  {
    (*static_cast<std::function<void()> *>(d))();
    return G_SOURCE_REMOVE;
  }, heap, [](gpointer d) { delete static_cast<std::function<void()> *>(d); });
}

// Activates the first visible AdwActionRow whose title contains |text| (searching dialogs first).
bool ActivateRow(GtkWidget * root, std::string const & text)
{
  for (GtkWidget * c = gtk_widget_get_first_child(root); c; c = gtk_widget_get_next_sibling(c))
  {
    if (!gtk_widget_get_mapped(c))
      continue;
    if (ADW_IS_PREFERENCES_ROW(c))
    {
      char const * title = adw_preferences_row_get_title(ADW_PREFERENCES_ROW(c));
      if (title && std::string(title).find(text) != std::string::npos)
      {
        if (ADW_IS_ACTION_ROW(c))
          g_signal_emit_by_name(c, "activated");
        else if (ADW_IS_BUTTON_ROW(c))
          g_signal_emit_by_name(c, "activated");
        else
          gtk_widget_activate(c);
        return true;
      }
    }
    if (ActivateRow(c, text))
      return true;
  }
  return false;
}

void Run(MainWindow & w, std::string const & cmdline)
{
  LOG(LINFO, ("Debug command:", cmdline));
  std::istringstream in(cmdline);
  std::string cmd;
  in >> cmd;
  double const s = w.Map().Scale();
  if (cmd == "tap" || cmd == "longtap")
  {
    double x, y;
    in >> x >> y;
    Touch(df::TouchEvent::TOUCH_DOWN, x, y, s);
    Later(cmd == "tap" ? 60 : 1200, [x, y, s]() { Touch(df::TouchEvent::TOUCH_UP, x, y, s); });
  }
  else if (cmd == "drag")
  {
    double x1, y1, x2, y2;
    in >> x1 >> y1 >> x2 >> y2;
    Touch(df::TouchEvent::TOUCH_DOWN, x1, y1, s);
    int const steps = 12;
    for (int i = 1; i <= steps; ++i)
    {
      double const k = static_cast<double>(i) / steps;
      Later(20 * i, [=]() { Touch(df::TouchEvent::TOUCH_MOVE, x1 + (x2 - x1) * k, y1 + (y2 - y1) * k, s); });
    }
    Later(20 * steps + 20, [=]() { Touch(df::TouchEvent::TOUCH_UP, x2, y2, s); });
  }
  else if (cmd == "search-row")
  {
    size_t n = 0;
    in >> n;
    w.Search().SelectResult(n);
  }
  else if (cmd == "center")
  {
    double lat, lon;
    int zoom = 15;
    in >> lat >> lon >> zoom;
    w.PopToMap();
    GetFramework().SetViewportCenter(mercator::FromLatLon(lat, lon), zoom, true);
  }
  else if (cmd == "location")
  {
    location::GpsInfo info;
    info.m_source = location::EUser;
    in >> info.m_latitude >> info.m_longitude;
    double speed = -1, bearing = -1;
    if (in >> speed >> bearing)
    {
      info.m_speed = speed;
      info.m_bearing = bearing;
    }
    info.m_horizontalAccuracy = 10;
    info.m_timestamp = static_cast<double>(g_get_real_time()) / 1e6;
    w.Location().InjectForTests(info);
  }
  else if (cmd == "back")
    w.PopToMap();
  else if (cmd == "open")
  {
    std::string what;
    in >> what;
    if (what == "downloader")
      w.ShowDownloaderPage();
    else if (what == "settings")
      w.ShowSettings();
    else if (what == "layers")
      w.ShowLayers();
    else if (what == "bookmarks")
      w.ShowBookmarksPage();
    else if (what == "menu")
      w.ShowMenu();
  }
  else if (cmd == "row")
  {
    std::string text;
    std::getline(in >> std::ws, text);
    if (!ActivateRow(GTK_WIDGET(w.Window()), text))
      LOG(LWARNING, ("Row not found:", text));
  }
  else if (cmd == "night")
  {
    int mode = 0;
    in >> mode;
    ApplyNightMode(static_cast<NightMode>(mode));
  }
  else if (cmd == "close-dialog")
  {
    if (auto * d = adw_application_window_get_visible_dialog(ADW_APPLICATION_WINDOW(w.Window())))
      adw_dialog_close(d);
  }
  else if (cmd == "deselect")
    GetFramework().DeactivateMapSelection();
  else if (cmd == "route-to")
  {
    // route-to LAT LON: like tapping a point and pressing "Route to".
    double lat, lon;
    in >> lat >> lon;
    place_page::BuildInfo info;
    info.m_mercator = mercator::FromLatLon(lat, lon);
    info.m_source = place_page::BuildInfo::Source::Other;
    GetFramework().BuildAndSetPlacePageInfo(info);
    w.Routing().SetPointFromPlacePage(RouteMarkType::Finish);
  }
  else if (cmd == "router")
  {
    int t = 0;
    in >> t;
    w.Routing().SetRouterForTests(static_cast<routing::RouterType>(t));
  }
  else if (cmd == "start-nav")
    w.Routing().StartNavigationForTests();
  else
    LOG(LWARNING, ("Unknown debug command", cmd));
}
}  // namespace

void RegisterDebugActions(MainWindow & window, GApplication * app)
{
  auto * action = g_simple_action_new("debug", G_VARIANT_TYPE_STRING);
  Connect<void(GSimpleAction *, GVariant *)>(action, "activate", [&window](GSimpleAction *, GVariant * param)
  { Run(window, g_variant_get_string(param, nullptr)); });
  g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(action));
  g_object_unref(action);
}
}  // namespace lichen
