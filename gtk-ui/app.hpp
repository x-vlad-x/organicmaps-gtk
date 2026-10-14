#pragma once

#include <adwaita.h>
#include <gtk/gtk.h>

#include <functional>
#include <string>

class Framework;

namespace lichen
{
// Application ID: D-Bus name, desktop file and icon name. The CoMaps prototype uses its own ID
// and name, so it installs and runs beside the regular Lichen.
inline constexpr char kAppId[] = "io.github.x_vlad_x.Lichen.Devel";
inline constexpr char kAppName[] = "Lichen Preview";
inline constexpr char kProjectUrl[] = "https://github.com/x-vlad-x/lichen";
// Attribution shown on the map, in the main menu and in About (Pango markup).
inline constexpr char kBasedOnMarkup[] = "Based on <a href=\"https://www.comaps.app\">CoMaps</a>";
inline constexpr char kMapDataMarkup[] =
    "Map data © <a href=\"https://www.openstreetmap.org/copyright\">OpenStreetMap contributors</a>";

class MapView;
class MainWindow;

// Global application state. Everything here lives on the GTK main thread.
struct App
{
  Framework * framework = nullptr;
  AdwApplication * application = nullptr;
  MainWindow * window = nullptr;
};

App & GetApp();
Framework & GetFramework();

// Runs |fn| on the GTK main loop (safe to call from any thread).
void RunOnUiThread(std::function<void()> fn);

// Small helpers around plain GTK C API.
GtkWidget * MakeIconButton(char const * iconName, char const * tooltip, char const * cssClass = nullptr);
void SetMarginAll(GtkWidget * w, int margin);
void ClearBox(GtkWidget * box);
void ClearListBox(GtkListBox * list);
void ShowToast(std::string const & text);
void OpenUri(std::string const & uri);
void CopyToClipboard(std::string const & text);
std::string Trim(std::string s);

// A link and a message for sharing a location, built the way the CoMaps Android app does.
struct ShareData
{
  std::string m_url;
  std::string m_text;
};
ShareData MakeShareData(double lat, double lon, std::string const & name, bool myPosition);

// Connects a lambda to a GObject signal with no extra arguments besides the instance.
template <typename Fn>
void OnClicked(GtkWidget * button, Fn && fn)
{
  auto * heap = new std::function<void()>(std::forward<Fn>(fn));
  g_signal_connect_data(button, "clicked", G_CALLBACK(+[](GtkWidget *, gpointer data)
  { (*static_cast<std::function<void()> *>(data))(); }), heap,
                        [](gpointer data, GClosure *) { delete static_cast<std::function<void()> *>(data); },
                        GConnectFlags(0));
}

// Generic signal connection: Connect<void(GtkWidget *, int)>(obj, "signal", lambda).
// The std::function signature must match the C signal handler without the trailing user_data.
template <typename R, typename... Args>
struct SignalThunk
{
  static R Call(Args... args, gpointer data) { return (*static_cast<std::function<R(Args...)> *>(data))(args...); }
};

template <typename R, typename... Args>
gulong ConnectImpl(gpointer instance, char const * signal, std::function<R(Args...)> && fn)
{
  auto * heap = new std::function<R(Args...)>(std::move(fn));
  auto const thunk = &SignalThunk<R, Args...>::Call;
  return g_signal_connect_data(instance, signal, G_CALLBACK(thunk), heap,
                               [](gpointer data, GClosure *) { delete static_cast<std::function<R(Args...)> *>(data); },
                               GConnectFlags(0));
}

template <typename Signature, typename Fn>
gulong Connect(gpointer instance, char const * signal, Fn && fn)
{
  return ConnectImpl(instance, signal, std::function<Signature>(std::forward<Fn>(fn)));
}
}  // namespace lichen
