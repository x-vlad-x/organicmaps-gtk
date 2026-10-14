#pragma once

#include "app.hpp"
#include "location.hpp"
#include "map_view.hpp"

#include "storage/storage_defines.hpp"

#include "platform/location.hpp"

#include <memory>
#include <string>

namespace lichen
{
class PlacePage;
class SearchUi;
class RoutingUi;
class DownloaderPage;
class BookmarksPage;

class MainWindow
{
public:
  explicit MainWindow(AdwApplication * app);
  ~MainWindow();

  GtkWindow * Window() const { return GTK_WINDOW(m_window); }
  MapView & Map() { return *m_map; }
  LocationService & Location() { return m_location; }
  PlacePage & GetPlacePage() { return *m_placePage; }
  SearchUi & Search() { return *m_search; }
  RoutingUi & Routing() { return *m_routing; }

  void Present();
  void Shutdown();
  void Toast(std::string const & text);
  void OpenFile(GFile * file);

  // Full-screen pages (search, bookmarks, downloader...).
  void PushPage(AdwNavigationPage * page);
  void PopToMap();
  bool IsMapVisible() const;

  // Place page bottom sheet.
  void SetSheetContent(GtkWidget * content);
  void SetSheetOpen(bool open);
  bool IsSheetOpen() const;

  // Overlay slots on top of the map. Visibility of the default controls depends on the mode.
  enum class Mode
  {
    Default,          // bottom menu + map buttons
    SearchOnMap,      // search results shown on the map
    RoutePlanning,    // route planning panels
    Navigation,       // turn-by-turn navigation
  };
  void SetMode(Mode mode);
  Mode GetMode() const { return m_mode; }
  GtkWidget * TopSlot() const { return m_topSlot; }
  GtkWidget * BottomSlot() const { return m_bottomSlot; }

  void UpdateMapInsets();
  void ShowDownloaderPage(std::string const & rootId = {});
  void ShowBookmarksPage();
  void ShowSettings();
  void ShowMenu();
  void ShowLayers();
  void StartLocation();
  void UpdateKeepScreenOn();
  void ToggleMapControls();
  void UpdateTrackRecordingState();

private:
  void BuildUi();
  void BuildMapControls(GtkWidget * overlay);
  GtkWidget * BuildBottomMenu();
  void LoadCss();
  void SetupFramework();
  void OnMyPositionModeChanged(location::EMyPositionMode mode, bool routingActive);
  void OnCountryChanged(storage::CountryId const & countryId);
  void UpdateDownloadCard();
  void SyncNightMode();
  void OnSuspendedChanged();

  AdwApplicationWindow * m_window = nullptr;
  AdwToastOverlay * m_toasts = nullptr;
  AdwNavigationView * m_nav = nullptr;
  AdwNavigationPage * m_mapPage = nullptr;
  AdwBottomSheet * m_sheet = nullptr;
  GtkWidget * m_overlay = nullptr;

  std::unique_ptr<MapView> m_map;
  LocationService m_location;

  // Map controls.
  GtkWidget * m_rightButtons = nullptr;
  GtkWidget * m_myPositionButton = nullptr;
  GtkWidget * m_layersButton = nullptr;
  GtkWidget * m_recordingButton = nullptr;
  guint m_recordingCookie = 0;
  GtkWidget * m_bottomMenu = nullptr;
  GtkWidget * m_topSlot = nullptr;
  GtkWidget * m_bottomSlot = nullptr;
  GtkWidget * m_mapAttribution = nullptr;
  GtkWidget * m_downloadCard = nullptr;
  GtkWidget * m_downloadTitle = nullptr;
  GtkWidget * m_downloadButton = nullptr;
  GtkWidget * m_downloadProgress = nullptr;
  storage::CountryId m_currentCountry;

  std::unique_ptr<PlacePage> m_placePage;
  std::unique_ptr<SearchUi> m_search;
  std::unique_ptr<RoutingUi> m_routing;
  std::unique_ptr<DownloaderPage> m_downloader;
  std::unique_ptr<BookmarksPage> m_bookmarks;

  Mode m_mode = Mode::Default;
  location::EMyPositionMode m_myPositionMode = location::PendingPosition;
  bool m_suspended = false;
  bool m_controlsHidden = false;
  std::pair<int, int> m_lastInsets{-1, -1};
  bool m_shutdown = false;
  int m_storageSubscription = 0;
  guint m_keepScreenOnCookie = 0;
  std::function<void()> m_pendingUrl;
};
}  // namespace lichen
