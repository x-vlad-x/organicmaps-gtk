#pragma once

#include "app.hpp"

#include "map/routing_mark.hpp"

#include "routing/following_info.hpp"
#include "routing/router.hpp"
#include "routing/routing_callbacks.hpp"

#include "storage/storage_defines.hpp"

#include "platform/location.hpp"

#include <string>

namespace lichen
{
class MainWindow;

// Route planning panels and turn-by-turn navigation.
class RoutingUi
{
public:
  explicit RoutingUi(MainWindow & window);
  ~RoutingUi();

  void StartPlanning();
  void SetPointFromPlacePage(RouteMarkType type);
  void RemovePoint(RouteMarkType type, size_t intermediateIndex);
  void Cancel();

  bool IsNavigating() const;
  void OnLocationUpdate(location::GpsInfo const & info);

  void StartNavigationForTests() { StartNavigation(); }
  void SetRouterForTests(routing::RouterType t) { SetRouter(t); }
  void SaveForNextLaunch();
  void RestoreSavedRoute();
  void Shutdown();

private:
  void BuildPlanningPanels();
  void BuildNavigationPanels();
  void SetRouter(routing::RouterType type);
  void UpdateRouterButtons();
  void UpdatePointsView();
  void BuildRouteIfReady();
  void OnRouteBuilt(routing::RouterResultCode code, storage::CountriesSet const & absent);
  void OnRecommendation(int recommendation);
  void ShowPlanningStatus();
  void StartNavigation();
  void StopNavigation();
  void UpdateNavigation();
  void EnterPlanningMode();
  void LeaveRoutingModes();
  void AddMyPositionAsStart();
  void Speak(std::vector<std::string> const & phrases);

  MainWindow & m_window;

  // Planning.
  GtkWidget * m_planTop = nullptr;
  GtkWidget * m_planBottom = nullptr;
  GtkWidget * m_routerButtons[5] = {};
  GtkWidget * m_pointsBox = nullptr;
  GtkWidget * m_status = nullptr;
  GtkWidget * m_statusDetails = nullptr;
  GtkWidget * m_spinner = nullptr;
  GtkWidget * m_startButton = nullptr;
  GtkWidget * m_downloadButton = nullptr;
  GtkWidget * m_chartBox = nullptr;
  storage::CountriesSet m_absentCountries;
  bool m_updatingButtons = false;

  // Navigation.
  GtkWidget * m_navTop = nullptr;
  GtkWidget * m_navBottom = nullptr;
  GtkWidget * m_turnImage = nullptr;
  GtkWidget * m_turnExit = nullptr;
  GtkWidget * m_turnDistance = nullptr;
  GtkWidget * m_turnStreet = nullptr;
  GtkWidget * m_lanes = nullptr;
  GtkWidget * m_nextTurnBox = nullptr;
  GtkWidget * m_nextTurnImage = nullptr;
  GtkWidget * m_timeLeft = nullptr;
  GtkWidget * m_arrival = nullptr;
  GtkWidget * m_distLeft = nullptr;
  GtkWidget * m_speed = nullptr;
  GtkWidget * m_speedLimit = nullptr;
  GtkWidget * m_progress = nullptr;
  GtkWidget * m_voiceButton = nullptr;
  guint m_navTimer = 0;
  guint m_inhibitCookie = 0;
  double m_lastSpeed = -1;
  std::string m_lastTurnIcon;
};
}  // namespace lichen
