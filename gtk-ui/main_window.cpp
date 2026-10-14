#include "main_window.hpp"

#include "bookmarks.hpp"
#include "downloader.hpp"
#include "l10n.hpp"
#include "place_page.hpp"
#include "routing.hpp"
#include "search.hpp"
#include "settings.hpp"
#include "style.hpp"

#include "map/framework.hpp"

#include "storage/storage.hpp"

#include "platform/platform.hpp"
#include "platform/settings.hpp"
#include "platform/style_utils.hpp"

#include "indexer/map_style.hpp"

#include "base/file_name_utils.hpp"
#include "base/logging.hpp"

namespace omgtk
{
namespace
{
std::string ExecutableDir()
{
  char path[4096] = {};
  ssize_t const n = ::readlink("/proc/self/exe", path, sizeof(path) - 1);
  if (n <= 0)
    return ".";
  std::string s(path, static_cast<size_t>(n));
  return s.substr(0, s.rfind('/'));
}

char const * MyPositionIcon(location::EMyPositionMode mode)
{
  switch (mode)
  {
  case location::PendingPosition: return "om-location-notfollow-symbolic";
  case location::NotFollowNoPosition: return "om-location-off-symbolic";
  case location::NotFollow: return "om-location-notfollow-symbolic";
  case location::Follow: return "om-location-follow-symbolic";
  case location::FollowAndRotate: return "om-location-rotate-symbolic";
  }
  return "om-location-notfollow-symbolic";
}
}  // namespace

MainWindow::MainWindow(AdwApplication * app)
{
  m_window = ADW_APPLICATION_WINDOW(adw_application_window_new(GTK_APPLICATION(app)));
  gtk_window_set_title(GTK_WINDOW(m_window), "Organic Maps (GTK)");
  gtk_window_set_icon_name(GTK_WINDOW(m_window), kAppId);
  gtk_window_set_default_size(GTK_WINDOW(m_window), 420, 860);

  auto * iconTheme = gtk_icon_theme_get_for_display(gdk_display_get_default());
  gtk_icon_theme_add_search_path(iconTheme, base::JoinPath(ExecutableDir(), "..", "icons").c_str());
  LoadCss();

  m_map = std::make_unique<MapView>();
  m_placePage = std::make_unique<PlacePage>(*this);
  BuildUi();
  // These put their panels into the map overlay slots created by BuildUi().
  m_search = std::make_unique<SearchUi>(*this);
  m_routing = std::make_unique<RoutingUi>(*this);
  m_downloader = std::make_unique<DownloaderPage>(*this);
  m_bookmarks = std::make_unique<BookmarksPage>(*this);

  SetupFramework();
  SetMode(Mode::Default);
  UpdateKeepScreenOn();

  Connect<void(GObject *, GParamSpec *)>(m_window, "notify::suspended",
                                         [this](GObject *, GParamSpec *) { OnSuspendedChanged(); });
  Connect<gboolean(GtkWindow *)>(m_window, "close-request", [this](GtkWindow *)
  {
    // Keep the active route for the next launch, like the phone apps do.
    m_routing->SaveForNextLaunch();
    return FALSE;
  });

  SyncAppColorScheme();
  auto * styleManager = adw_style_manager_get_default();
  Connect<void(GObject *, GParamSpec *)>(styleManager, "notify::dark",
                                         [this](GObject *, GParamSpec *) { SyncNightMode(); });
  SyncNightMode();
}

MainWindow::~MainWindow() = default;

void MainWindow::Present()
{
  gtk_window_present(GTK_WINDOW(m_window));
}

void MainWindow::Shutdown()
{
  if (m_shutdown)
    return;
  m_shutdown = true;
  m_routing->SaveForNextLaunch();
  m_location.Stop();
  if (m_storageSubscription)
    GetFramework().GetStorage().Unsubscribe(m_storageSubscription);
  m_search->Shutdown();
  m_routing->Shutdown();
  m_downloader->Shutdown();
  m_map->Shutdown();
}

void MainWindow::LoadCss()
{
  auto * provider = gtk_css_provider_new();
  gtk_css_provider_load_from_string(provider, kAppCss);
  gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(provider);
}

void MainWindow::BuildUi()
{
  m_toasts = ADW_TOAST_OVERLAY(adw_toast_overlay_new());
  m_nav = ADW_NAVIGATION_VIEW(adw_navigation_view_new());
  adw_toast_overlay_set_child(m_toasts, GTK_WIDGET(m_nav));

  m_overlay = gtk_overlay_new();
  gtk_overlay_set_child(GTK_OVERLAY(m_overlay), m_map->Widget());
  BuildMapControls(m_overlay);

  m_sheet = ADW_BOTTOM_SHEET(adw_bottom_sheet_new());
  adw_bottom_sheet_set_content(m_sheet, m_overlay);
  adw_bottom_sheet_set_modal(m_sheet, FALSE);
  adw_bottom_sheet_set_show_drag_handle(m_sheet, TRUE);
  adw_bottom_sheet_set_full_width(m_sheet, TRUE);
  adw_bottom_sheet_set_sheet(m_sheet, m_placePage->Widget());
  Connect<void(GObject *, GParamSpec *)>(m_sheet, "notify::open", [this](GObject *, GParamSpec *)
  {
    if (!adw_bottom_sheet_get_open(m_sheet))
      m_placePage->OnSheetClosed();
    UpdateMapInsets();
  });
  Connect<void(GObject *, GParamSpec *)>(m_sheet, "notify::sheet-height",
                                         [this](GObject *, GParamSpec *) { UpdateMapInsets(); });

  // Keep the map gestures separate from the standard draggable window header.
  auto * mapToolbar = adw_toolbar_view_new();
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(mapToolbar), adw_header_bar_new());
  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(mapToolbar), GTK_WIDGET(m_sheet));
  m_mapPage = adw_navigation_page_new(mapToolbar, "Organic Maps (GTK)");
  adw_navigation_page_set_tag(m_mapPage, "map");
  adw_navigation_view_add(m_nav, m_mapPage);

  adw_application_window_set_content(m_window, GTK_WIDGET(m_toasts));

  // Hardware/back gesture on the map: close what's open, like Android's back button.
  auto * keys = gtk_event_controller_key_new();
  Connect<gboolean(GtkEventControllerKey *, guint, guint, GdkModifierType)>(
      keys, "key-pressed", [this](GtkEventControllerKey *, guint key, guint, GdkModifierType) -> gboolean
  {
    if (!IsMapVisible())
      return FALSE;
    auto & fw = GetFramework();
    switch (key)
    {
    case GDK_KEY_Escape:
      if (IsSheetOpen())
      {
        fw.DeactivateMapSelection();
        return TRUE;
      }
      return FALSE;
    case GDK_KEY_plus:
    case GDK_KEY_equal:
    case GDK_KEY_KP_Add: fw.Scale(Framework::SCALE_MAG, true); return TRUE;
    case GDK_KEY_minus:
    case GDK_KEY_KP_Subtract: fw.Scale(Framework::SCALE_MIN, true); return TRUE;
    case GDK_KEY_Left: fw.Move(0.2, 0, true); return TRUE;
    case GDK_KEY_Right: fw.Move(-0.2, 0, true); return TRUE;
    case GDK_KEY_Up: fw.Move(0, -0.2, true); return TRUE;
    case GDK_KEY_Down: fw.Move(0, 0.2, true); return TRUE;
    default: return FALSE;
    }
  });
  gtk_widget_add_controller(GTK_WIDGET(m_window), keys);
}

void MainWindow::BuildMapControls(GtkWidget * overlay)
{
  // Top slot: search query bar, route planning header, navigation turn panel.
  m_topSlot = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_valign(m_topSlot, GTK_ALIGN_START);
  gtk_overlay_add_overlay(GTK_OVERLAY(overlay), m_topSlot);

  // Layers button (top-left), like on Android.
  m_layersButton = MakeIconButton("om-layers-symbolic", T("layers_title").c_str(), "map-button");
  gtk_widget_set_halign(m_layersButton, GTK_ALIGN_START);
  gtk_widget_set_valign(m_layersButton, GTK_ALIGN_START);
  gtk_widget_set_margin_start(m_layersButton, 12);
  gtk_widget_set_margin_top(m_layersButton, 12);
  OnClicked(m_layersButton, [this]() { ShowLayers(); });
  gtk_overlay_add_overlay(GTK_OVERLAY(overlay), m_layersButton);

  // Track recording indicator (red, under the layers button); tap to stop and save.
  m_recordingButton = gtk_button_new();
  auto * recIcon = gtk_image_new_from_icon_name("om-track-recording");
  gtk_image_set_pixel_size(GTK_IMAGE(recIcon), 40);
  gtk_button_set_child(GTK_BUTTON(m_recordingButton), recIcon);
  gtk_widget_add_css_class(m_recordingButton, "map-button");
  gtk_widget_set_tooltip_text(m_recordingButton, T("stop_track_recording").c_str());
  gtk_widget_set_halign(m_recordingButton, GTK_ALIGN_START);
  gtk_widget_set_valign(m_recordingButton, GTK_ALIGN_START);
  gtk_widget_set_margin_start(m_recordingButton, 12);
  gtk_widget_set_margin_top(m_recordingButton, 72);
  gtk_widget_set_visible(m_recordingButton, FALSE);
  OnClicked(m_recordingButton, [this]() { m_bookmarks->ToggleTrackRecording(); });
  gtk_overlay_add_overlay(GTK_OVERLAY(overlay), m_recordingButton);

  // Zoom buttons: right side, vertically centred.
  m_rightButtons = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
  gtk_widget_set_halign(m_rightButtons, GTK_ALIGN_END);
  gtk_widget_set_valign(m_rightButtons, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_end(m_rightButtons, 12);
  auto * zoomIn = MakeIconButton("list-add-symbolic", T("zoom_in").c_str(), "map-button");
  auto * zoomOut = MakeIconButton("list-remove-symbolic", T("zoom_out").c_str(), "map-button");
  OnClicked(zoomIn, []() { GetFramework().Scale(Framework::SCALE_MAG, true); });
  OnClicked(zoomOut, []() { GetFramework().Scale(Framework::SCALE_MIN, true); });
  gtk_box_append(GTK_BOX(m_rightButtons), zoomIn);
  gtk_box_append(GTK_BOX(m_rightButtons), zoomOut);
  gtk_overlay_add_overlay(GTK_OVERLAY(overlay), m_rightButtons);

  // Bottom area: download card, my position button and the bottom menu (or mode panels).
  auto * bottom = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
  gtk_widget_set_valign(bottom, GTK_ALIGN_END);
  gtk_widget_add_css_class(bottom, "map-bottom");

  m_downloadCard = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_add_css_class(m_downloadCard, "card");
  gtk_widget_add_css_class(m_downloadCard, "download-card");
  gtk_widget_set_halign(m_downloadCard, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_start(m_downloadCard, 24);
  gtk_widget_set_margin_end(m_downloadCard, 24);
  m_downloadTitle = gtk_label_new("");
  gtk_label_set_wrap(GTK_LABEL(m_downloadTitle), TRUE);
  gtk_label_set_justify(GTK_LABEL(m_downloadTitle), GTK_JUSTIFY_CENTER);
  gtk_widget_add_css_class(m_downloadTitle, "title-4");
  m_downloadProgress = gtk_progress_bar_new();
  m_downloadButton = gtk_button_new_with_label("");
  gtk_widget_add_css_class(m_downloadButton, "suggested-action");
  gtk_widget_add_css_class(m_downloadButton, "pill");
  OnClicked(m_downloadButton, [this]()
  {
    auto & storage = GetFramework().GetStorage();
    if (m_currentCountry.empty())
      return;
    storage::NodeAttrs attrs;
    storage.GetNodeAttrs(m_currentCountry, attrs);
    if (attrs.m_status == storage::NodeStatus::Downloading || attrs.m_status == storage::NodeStatus::InQueue)
      storage.CancelDownloadNode(m_currentCountry);
    else if (attrs.m_status == storage::NodeStatus::Error)
      storage.RetryDownloadNode(m_currentCountry);
    else
      storage.DownloadNode(m_currentCountry);
    UpdateDownloadCard();
  });
  gtk_box_append(GTK_BOX(m_downloadCard), m_downloadTitle);
  gtk_box_append(GTK_BOX(m_downloadCard), m_downloadProgress);
  gtk_box_append(GTK_BOX(m_downloadCard), m_downloadButton);
  gtk_widget_set_visible(m_downloadCard, FALSE);
  gtk_box_append(GTK_BOX(bottom), m_downloadCard);

  m_myPositionButton = MakeIconButton(MyPositionIcon(location::PendingPosition), T("core_my_position").c_str(),
                                      "map-button");
  gtk_widget_set_halign(m_myPositionButton, GTK_ALIGN_END);
  gtk_widget_set_margin_end(m_myPositionButton, 12);
  OnClicked(m_myPositionButton, [this]()
  {
    StartLocation();
    GetFramework().SwitchMyPositionNextMode();
  });
  gtk_box_append(GTK_BOX(bottom), m_myPositionButton);

  m_bottomSlot = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append(GTK_BOX(bottom), m_bottomSlot);

  // Keep map attribution readable and keyboard-accessible, with links to both sources.
  m_mapAttribution = gtk_label_new(nullptr);
  gtk_label_set_markup(GTK_LABEL(m_mapAttribution),
      "Map data © <a href=\"https://www.openstreetmap.org/copyright\">OpenStreetMap</a> and "
      "<a href=\"https://organicmaps.app\">Organic Maps</a>");
  gtk_label_set_wrap(GTK_LABEL(m_mapAttribution), TRUE);
  gtk_label_set_justify(GTK_LABEL(m_mapAttribution), GTK_JUSTIFY_CENTER);
  gtk_widget_set_halign(m_mapAttribution, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class(m_mapAttribution, "caption");
  gtk_widget_add_css_class(m_mapAttribution, "map-attribution");
  gtk_box_append(GTK_BOX(bottom), m_mapAttribution);

  m_bottomMenu = BuildBottomMenu();
  gtk_box_append(GTK_BOX(bottom), m_bottomMenu);

  gtk_overlay_add_overlay(GTK_OVERLAY(overlay), bottom);

  // Let the engine know which parts of the map are covered (ruler and compass placement).
  Connect<gboolean(GtkOverlay *, GtkWidget *, GdkRectangle *)>(
      overlay, "get-child-position", [this](GtkOverlay *, GtkWidget *, GdkRectangle *)
  {
    // Called during allocation; defer to keep it out of the layout pass.
    g_idle_add_once([](gpointer data) { static_cast<MainWindow *>(data)->UpdateMapInsets(); }, this);
    return FALSE;
  });
}

GtkWidget * MainWindow::BuildBottomMenu()
{
  auto * bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_set_homogeneous(GTK_BOX(bar), TRUE);
  gtk_widget_add_css_class(bar, "bottom-menu");

  struct Item
  {
    char const * icon;
    char const * label;
    std::function<void()> action;
  };
  Item const items[] = {
      {"system-search-symbolic", "search", [this]() { m_search->ShowSearchPage(); }},
      {"om-route-symbolic", "route", [this]() { m_routing->StartPlanning(); }},
      {"om-bookmarks-symbolic", "bookmarks_and_tracks", [this]() { ShowBookmarksPage(); }},
      {"open-menu-symbolic", "menu", [this]() { ShowMenu(); }},
  };
  for (auto const & item : items)
  {
    auto * button = gtk_button_new();
    auto * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    auto * image = gtk_image_new_from_icon_name(item.icon);
    gtk_image_set_icon_size(GTK_IMAGE(image), GTK_ICON_SIZE_LARGE);
    auto * label = gtk_label_new(T(item.label).c_str());
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_widget_add_css_class(label, "caption");
    gtk_box_append(GTK_BOX(box), image);
    gtk_box_append(GTK_BOX(box), label);
    gtk_button_set_child(GTK_BUTTON(button), box);
    gtk_widget_add_css_class(button, "flat");
    gtk_widget_add_css_class(button, "bottom-menu-button");
    OnClicked(button, item.action);
    gtk_box_append(GTK_BOX(bar), button);
  }
  return bar;
}

void MainWindow::UpdateMapInsets()
{
  if (m_shutdown)
    return;
  int bottom = gtk_widget_get_height(m_mapAttribution);
  if (gtk_widget_get_visible(m_bottomMenu))
    bottom += gtk_widget_get_height(m_bottomMenu);
  if (gtk_widget_get_visible(m_bottomSlot))
    bottom += gtk_widget_get_height(m_bottomSlot);
  int const top = gtk_widget_get_visible(m_topSlot) ? gtk_widget_get_height(m_topSlot) : 0;
  // Keep the selected object visible above the place page, like Android does.
  if (adw_bottom_sheet_get_open(m_sheet))
    bottom = std::max(bottom, adw_bottom_sheet_get_sheet_height(m_sheet));
  if (top == m_lastInsets.first && bottom == m_lastInsets.second)
    return;
  m_lastInsets = {top, bottom};
  m_map->SetInsets(top, bottom, 0, 0);
}

void MainWindow::SetMode(Mode mode)
{
  m_mode = mode;
  m_controlsHidden = false;
  gtk_widget_set_visible(m_myPositionButton, TRUE);
  bool const def = mode == Mode::Default;
  gtk_widget_set_visible(m_bottomMenu, def);
  gtk_widget_set_visible(m_layersButton, def);
  gtk_widget_set_visible(m_rightButtons, ZoomButtonsEnabled());
  UpdateDownloadCard();
  UpdateMapInsets();
  UpdateTrackRecordingState();
}

void MainWindow::SetupFramework()
{
  auto & fw = GetFramework();

  fw.SetPlacePageListeners([this]() { RunOnUiThread([this]() { m_placePage->Open(); }); },
                           [this]() { RunOnUiThread([this]() { m_placePage->Close(); }); },
                           [this]() { RunOnUiThread([this]() { m_placePage->Update(); }); },
                           [this]() { RunOnUiThread([this]() { ToggleMapControls(); }); });

  fw.SetMyPositionModeListener([this](location::EMyPositionMode mode, bool routingActive)
  { RunOnUiThread([this, mode, routingActive]() { OnMyPositionModeChanged(mode, routingActive); }); });

  fw.SetCurrentCountryChangedListener([this](storage::CountryId const & id)
  { RunOnUiThread([this, id]() { OnCountryChanged(id); }); });

  m_storageSubscription = fw.GetStorage().Subscribe(
      [this](storage::CountryId const & id)
  {
    if (id == m_currentCountry || GetFramework().GetStorage().GetParentIdFor(m_currentCountry) == id)
      UpdateDownloadCard();
    m_downloader->OnCountryChanged(id);
  },
      [this](storage::CountryId const & id, downloader::Progress const & progress)
  {
    if (id == m_currentCountry)
      UpdateDownloadCard();
    m_downloader->OnProgress(id, progress);
  });

  m_location.m_onLocation = [this](location::GpsInfo const & info)
  {
    GetFramework().OnLocationUpdate(info);
    m_routing->OnLocationUpdate(info);
    m_placePage->OnLocationUpdate();
  };
  m_location.m_onError = [this](location::TLocationError error)
  {
    GetFramework().OnLocationError(error);
    if (error == location::EDenied)
      Toast(T("location_is_disabled_long_text"));
  };
  m_location.m_onCompass = [](double bearing)
  {
    location::CompassInfo info;
    info.m_bearing = bearing;
    GetFramework().OnCompassUpdate(info);
  };

  m_map->m_onEngineCreated = [this]()
  {
    auto & fw = GetFramework();
    fw.LoadBookmarks();
    m_routing->RestoreSavedRoute();
    if (m_pendingUrl)
    {
      auto fn = std::move(m_pendingUrl);
      m_pendingUrl = nullptr;
      fn();
    }
    // Like Android: ask for the location on start, the engine shows "pending" until the first fix.
    StartLocation();
    UpdateMapInsets();
    UpdateTrackRecordingState();
  };
}

void MainWindow::StartLocation()
{
  if (!m_location.IsStarted())
    m_location.Start();
}

void MainWindow::UpdateTrackRecordingState()
{
  bool const recording = GetFramework().IsTrackRecordingEnabled();
  gtk_widget_set_visible(m_recordingButton, recording && m_mode != Mode::Navigation);
  // Keep the phone awake (screen may turn off) while recording, like the Android foreground service.
  auto * app = gtk_window_get_application(GTK_WINDOW(m_window));
  if (recording && !m_recordingCookie)
    m_recordingCookie = gtk_application_inhibit(app, GTK_WINDOW(m_window), GTK_APPLICATION_INHIBIT_SUSPEND,
                                                T("track_recording").c_str());
  else if (!recording && m_recordingCookie)
  {
    gtk_application_uninhibit(app, m_recordingCookie);
    m_recordingCookie = 0;
  }
}

void MainWindow::ToggleMapControls()
{
  // Long tap on the map hides/shows all controls, like on Android.
  if (m_mode != Mode::Default && !m_controlsHidden)
    return;
  m_controlsHidden = !m_controlsHidden;
  bool const show = !m_controlsHidden;
  gtk_widget_set_visible(m_bottomMenu, show);
  gtk_widget_set_visible(m_layersButton, show);
  gtk_widget_set_visible(m_rightButtons, show && ZoomButtonsEnabled());
  gtk_widget_set_visible(m_myPositionButton, show);
  UpdateMapInsets();
}

void MainWindow::UpdateKeepScreenOn()
{
  bool const keep = KeepScreenOn() && !m_suspended;
  if (keep && !m_keepScreenOnCookie)
    m_keepScreenOnCookie = gtk_application_inhibit(gtk_window_get_application(GTK_WINDOW(m_window)),
                                                   GTK_WINDOW(m_window), GTK_APPLICATION_INHIBIT_IDLE, "Organic Maps");
  else if (!keep && m_keepScreenOnCookie)
  {
    gtk_application_uninhibit(gtk_window_get_application(GTK_WINDOW(m_window)), m_keepScreenOnCookie);
    m_keepScreenOnCookie = 0;
  }
}

void MainWindow::OnMyPositionModeChanged(location::EMyPositionMode mode, bool routingActive)
{
  m_myPositionMode = mode;
  gtk_button_set_icon_name(GTK_BUTTON(m_myPositionButton), MyPositionIcon(mode));
  bool const active = mode == location::Follow || mode == location::FollowAndRotate;
  if (active)
    gtk_widget_add_css_class(m_myPositionButton, "following");
  else
    gtk_widget_remove_css_class(m_myPositionButton, "following");
  if (mode == location::PendingPosition)
    gtk_widget_add_css_class(m_myPositionButton, "pending");
  else
    gtk_widget_remove_css_class(m_myPositionButton, "pending");
  UNUSED_VALUE(routingActive);
}

void MainWindow::OnCountryChanged(storage::CountryId const & countryId)
{
  m_currentCountry = countryId;
  UpdateDownloadCard();
}

void MainWindow::UpdateDownloadCard()
{
  if (m_shutdown)
    return;
  bool show = !m_currentCountry.empty() && m_mode != Mode::Navigation;
  if (show)
  {
    auto & storage = GetFramework().GetStorage();
    storage::NodeAttrs attrs;
    storage.GetNodeAttrs(m_currentCountry, attrs);
    using storage::NodeStatus;
    switch (attrs.m_status)
    {
    case NodeStatus::NotDownloaded:
    case NodeStatus::Partly:
    case NodeStatus::Undefined:
      gtk_label_set_text(GTK_LABEL(m_downloadTitle), attrs.m_nodeLocalName.c_str());
      gtk_button_set_label(GTK_BUTTON(m_downloadButton),
                           (T("download") + " (" + FormatSize(attrs.m_mwmSize) + ")").c_str());
      gtk_widget_set_visible(m_downloadProgress, FALSE);
      break;
    case NodeStatus::Downloading:
    case NodeStatus::InQueue:
    case NodeStatus::Applying:
    {
      gtk_label_set_text(GTK_LABEL(m_downloadTitle), attrs.m_nodeLocalName.c_str());
      gtk_button_set_label(GTK_BUTTON(m_downloadButton), T("cancel").c_str());
      gtk_widget_set_visible(m_downloadProgress, TRUE);
      auto const & p = attrs.m_downloadingProgress;
      double const frac = p.m_bytesTotal > 0 ? static_cast<double>(p.m_bytesDownloaded) / p.m_bytesTotal : 0.0;
      gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(m_downloadProgress), frac);
      break;
    }
    case NodeStatus::Error:
      gtk_label_set_text(GTK_LABEL(m_downloadTitle),
                         (attrs.m_nodeLocalName + "\n" + T("country_status_download_failed")).c_str());
      gtk_button_set_label(GTK_BUTTON(m_downloadButton), T("try_again").c_str());
      gtk_widget_set_visible(m_downloadProgress, FALSE);
      break;
    default: show = false;
    }
  }
  gtk_widget_set_visible(m_downloadCard, show);
}

void MainWindow::Toast(std::string const & text)
{
  auto * toast = adw_toast_new(text.c_str());
  adw_toast_set_timeout(toast, 3);
  adw_toast_overlay_add_toast(m_toasts, toast);
}

void MainWindow::OpenFile(GFile * file)
{
  if (char * path = g_file_get_path(file))
  {
    std::string const p = path;
    g_free(path);
    m_bookmarks->ImportFile(p);
    return;
  }
  // geo:, om:// and omaps.app links.
  char * uri = g_file_get_uri(file);
  if (!uri)
    return;
  std::string const url = uri;
  g_free(uri);
  auto const open = [this, url]()
  {
    auto & fw = GetFramework();
    using UrlType = url_scheme::ParsedMapApi::UrlType;
    switch (fw.ParseAndSetApiURL(url))
    {
    case UrlType::Incorrect: Toast(T("dialog_routing_application_error")); return;
    case UrlType::Route:
    {
      auto const data = fw.GetParsedRoutingData();
      auto & rm = fw.GetRoutingManager();
      rm.RemoveRoutePoints();
      rm.SetRouter(data.m_type);
      for (size_t i = 0; i < data.m_points.size(); ++i)
      {
        RouteMarkData pt;
        pt.m_title = data.m_points[i].m_name;
        pt.m_position = data.m_points[i].m_org;
        pt.m_pointType = i == 0 ? RouteMarkType::Start
                       : (i + 1 == data.m_points.size() ? RouteMarkType::Finish : RouteMarkType::Intermediate);
        pt.m_intermediateIndex = i == 0 ? 0 : i - 1;
        rm.AddRoutePoint(std::move(pt), false);
      }
      m_routing->StartPlanning();
      return;
    }
    case UrlType::Search:
      m_search->ShowSearchPage(fw.GetParsedSearchRequest().m_query);
      return;
    default: fw.ExecuteMapApiRequest(); return;
    }
  };
  // The engine may not exist yet when the app is started by the link.
  if (m_map->IsEngineCreated())
    open();
  else
    m_pendingUrl = open;
}

void MainWindow::PushPage(AdwNavigationPage * page)
{
  adw_navigation_view_push(m_nav, page);
}

void MainWindow::PopToMap()
{
  adw_navigation_view_pop_to_tag(m_nav, "map");
}

bool MainWindow::IsMapVisible() const
{
  return adw_navigation_view_get_visible_page(m_nav) == m_mapPage;
}

void MainWindow::SetSheetContent(GtkWidget * content)
{
  adw_bottom_sheet_set_sheet(m_sheet, content);
}

void MainWindow::SetSheetOpen(bool open)
{
  adw_bottom_sheet_set_open(m_sheet, open);
}

bool MainWindow::IsSheetOpen() const
{
  return adw_bottom_sheet_get_open(m_sheet);
}

void MainWindow::ShowDownloaderPage(std::string const & rootId)
{
  m_downloader->Show(rootId);
}

void MainWindow::ShowBookmarksPage()
{
  m_bookmarks->Show();
}

void MainWindow::ShowSettings()
{
  ShowSettingsDialog(*this);
}

void MainWindow::ShowMenu()
{
  auto * dialog = adw_dialog_new();
  adw_dialog_set_title(dialog, T("menu").c_str());
  adw_dialog_set_content_width(dialog, 420);

  auto * toolbar = adw_toolbar_view_new();
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar), adw_header_bar_new());

  auto * list = gtk_list_box_new();
  gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_NONE);
  gtk_widget_add_css_class(list, "boxed-list");
  SetMarginAll(list, 12);

  auto const addRow = [&](char const * icon, std::string const & title, std::function<void()> action)
  {
    auto * row = adw_action_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), title.c_str());
    adw_action_row_add_prefix(ADW_ACTION_ROW(row), gtk_image_new_from_icon_name(icon));
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), TRUE);
    Connect<void(AdwActionRow *)>(row, "activated", [dialog, action](AdwActionRow *)
    {
      adw_dialog_close(dialog);
      action();
    });
    gtk_list_box_append(GTK_LIST_BOX(list), row);
  };

  auto & fw = GetFramework();
  addRow("om-download-symbolic", T("download_maps"), [this]() { ShowDownloaderPage(); });
  bool const recording = fw.IsTrackRecordingEnabled();
  addRow("media-record-symbolic", recording ? T("stop_track_recording") : T("start_track_recording"),
         [this]() { m_bookmarks->ToggleTrackRecording(); });
  addRow("send-to-symbolic", T("share_my_location"), [this]()
  {
    auto const & last = m_location.LastLocation();
    if (!last)
    {
      Toast(T("unknown_current_position"));
      return;
    }
    auto const data = GetFramework().GetShareDataForMyPosition(last->GetLatLon());
    CopyToClipboard(data.m_text.empty() ? data.m_url : data.m_text);
    Toast(TF("copied_to_clipboard", {data.m_url}));
  });
  addRow("emblem-system-symbolic", T("settings"), [this]() { ShowSettings(); });
  addRow("help-about-symbolic", T("about_menu_title"), [this]() { ShowAboutDialog(*this); });

  auto * content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append(GTK_BOX(content), list);
  auto * attribution = gtk_label_new(nullptr);
  gtk_label_set_markup(GTK_LABEL(attribution),
      "Based on the <a href=\"https://organicmaps.app\">Organic Maps Project</a>\n"
      "Map data © <a href=\"https://www.openstreetmap.org/copyright\">OpenStreetMap</a> and "
      "<a href=\"https://organicmaps.app\">Organic Maps</a>");
  gtk_label_set_wrap(GTK_LABEL(attribution), TRUE);
  gtk_label_set_justify(GTK_LABEL(attribution), GTK_JUSTIFY_CENTER);
  gtk_widget_add_css_class(attribution, "caption");
  SetMarginAll(attribution, 12);
  gtk_box_append(GTK_BOX(content), attribution);
  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar), content);
  adw_dialog_set_child(dialog, toolbar);
  adw_dialog_present(dialog, GTK_WIDGET(m_window));
}

void MainWindow::ShowLayers()
{
  ShowLayersDialog(*this);
}

void MainWindow::SyncNightMode()
{
  if (style_utils::GetNightModeSetting() != style_utils::NightMode::System)
    return;
  auto & fw = GetFramework();
  bool const dark = adw_style_manager_get_dark(adw_style_manager_get_default());
  auto const current = fw.GetMapStyle();
  auto const desired = dark ? GetDarkMapStyleVariant(current) : GetLightMapStyleVariant(current);
  if (desired != current)
    fw.SetMapStyle(desired);
}

void MainWindow::OnSuspendedChanged()
{
  bool const suspended = gtk_window_is_suspended(GTK_WINDOW(m_window));
  if (suspended == m_suspended || !m_map->IsEngineCreated())
    return;
  m_suspended = suspended;
  auto & fw = GetFramework();
  // Keep everything running while navigating (voice prompts, route following).
  if (suspended && !m_routing->IsNavigating())
    fw.EnterBackground();
  else if (!suspended)
    fw.EnterForeground();
  UpdateKeepScreenOn();
}
}  // namespace omgtk
