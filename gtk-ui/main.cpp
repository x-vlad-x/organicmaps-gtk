#include "app.hpp"
#include "debug.hpp"
#include "l10n.hpp"
#include "main_window.hpp"

#include "map/framework.hpp"

#include "platform/platform.hpp"
#include "platform/settings.hpp"
#include "platform/style_utils.hpp"

#include "base/logging.hpp"

#include <QtCore/QCoreApplication>

#include <glib-unix.h>

#include <clocale>
#include <csignal>
#include <cstdlib>

namespace
{
void OnStartup(GApplication *, gpointer)
{
  using namespace omgtk;
  // gtk_init() resets the locale; the core parses doubles with a dot.
  std::setlocale(LC_NUMERIC, "C");

  InitLocalization();

  // A GNOME app follows the system dark style unless the user chose otherwise.
  if (int v = 0; !settings::Get("NightMode", v))
    style_utils::SetNightModeSetting(style_utils::NightMode::System);

  auto * fw = new Framework(FrameworkParams{});
  fw->SetupMeasurementSystem();
  // Localized strings the core uses itself (same list the Android app passes).
  for (char const * key : {"core_entrance", "core_exit", "core_my_places", "core_my_position",
                           "core_placepage_unknown_place", "open_in_app", "postal_code", "share_my_position",
                           "share_open_in_om_or_browser", "share_get_om"})
    fw->AddString(key, T(key));
  fw->AddString("wifi", T("category_wifi"));
  GetApp().framework = fw;

  GetApp().window = new MainWindow(GetApp().application);
  // Remote control for automated testing, only when explicitly requested.
  if (g_getenv("OMGTK_DEBUG"))
    RegisterDebugActions(*GetApp().window, G_APPLICATION(GetApp().application));
}

void OnActivate(GApplication *, gpointer)
{
  if (auto * w = omgtk::GetApp().window)
    w->Present();
}

void OnShutdown(GApplication *, gpointer)
{
  using namespace omgtk;
  auto & app = GetApp();
  if (app.window)
  {
    app.window->Shutdown();
    delete app.window;
    app.window = nullptr;
  }
  if (app.framework)
  {
    app.framework->PrepareToShutdown();
    delete app.framework;
    app.framework = nullptr;
  }
}
}  // namespace

int main(int argc, char * argv[])
{
  std::setlocale(LC_NUMERIC, "C");

  // The core posts its UI-thread tasks through Qt; Qt's GLib event dispatcher runs them from the
  // GTK main loop, so QCoreApplication::exec() is never called.
  int qtArgc = 1;
  char * qtArgv[] = {argv[0], nullptr};
  QCoreApplication qtApp(qtArgc, qtArgv);
  QCoreApplication::setApplicationName("OrganicMaps");

  Platform & platform = GetPlatform();
  LOG(LINFO, ("Organic Maps (GTK)", platform.Version(), "resources:", platform.ResourcesDir(),
              "writable:", platform.WritableDir()));

  auto * app = adw_application_new(omgtk::kAppId, G_APPLICATION_HANDLES_OPEN);
  omgtk::GetApp().application = app;
  g_signal_connect(app, "startup", G_CALLBACK(OnStartup), nullptr);
  g_signal_connect(app, "activate", G_CALLBACK(OnActivate), nullptr);
  g_signal_connect(app, "open", G_CALLBACK(+[](GApplication * a, GFile ** files, int n, char *, gpointer)
  {
    OnActivate(a, nullptr);
    if (auto * w = omgtk::GetApp().window)
      for (int i = 0; i < n; ++i)
        w->OpenFile(files[i]);
  }), nullptr);
  g_signal_connect(app, "shutdown", G_CALLBACK(OnShutdown), nullptr);

  // Close gracefully on SIGTERM/SIGINT too: saves the viewport, the active route and bookmarks.
  for (int sig : {SIGTERM, SIGINT, SIGHUP})
    g_unix_signal_add(sig, [](gpointer a) -> gboolean
    {
      g_application_quit(G_APPLICATION(a));
      return G_SOURCE_REMOVE;
    }, app);

  int const status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  LOG(LINFO, ("Organic Maps (GTK) finished with code", status));
  return status;
}
