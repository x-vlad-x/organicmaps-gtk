#include "routing.hpp"

#include "elevation_chart.hpp"
#include "l10n.hpp"
#include "main_window.hpp"
#include "style.hpp"
#include "tts.hpp"

#include "map/elevation_info.hpp"
#include "map/framework.hpp"
#include "map/routing_manager.hpp"

#include "routing/routing_options.hpp"

#include "platform/localization.hpp"

#include "storage/storage.hpp"

#include "geometry/mercator.hpp"

#include "base/logging.hpp"

#include <cmath>

namespace omgtk
{
namespace
{
using routing::RouterType;
using routing::turns::CarDirection;
using routing::turns::PedestrianDirection;

struct RouterButton
{
  RouterType type;
  char const * icon;
};
RouterButton const kRouters[] = {
    {RouterType::Vehicle, "om-car-symbolic"},       {RouterType::Pedestrian, "om-pedestrian-symbolic"},
    {RouterType::Bicycle, "om-bicycle-symbolic"},   {RouterType::Transit, "om-transit-symbolic"},
    {RouterType::Ruler, "om-ruler-symbolic"},
};

std::string CarTurnIcon(CarDirection dir, uint32_t exitNum)
{
  switch (dir)
  {
  case CarDirection::GoStraight: return "om-turn-straight";
  case CarDirection::TurnRight: return "om-turn-right";
  case CarDirection::TurnSharpRight: return "om-turn-right-sharp";
  case CarDirection::TurnSlightRight: return "om-turn-right-slight";
  case CarDirection::TurnLeft: return "om-turn-left";
  case CarDirection::TurnSharpLeft: return "om-turn-left-sharp";
  case CarDirection::TurnSlightLeft: return "om-turn-left-slight";
  case CarDirection::UTurnLeft: return "om-turn-uleft";
  case CarDirection::UTurnRight: return "om-turn-uright";
  case CarDirection::EnterRoundAbout:
  case CarDirection::LeaveRoundAbout:
  case CarDirection::StayOnRoundAbout:
    if (exitNum >= 1 && exitNum <= 12)
      return "om-roundabout-exit-" + std::to_string(exitNum);
    return "om-turn-round";
  case CarDirection::ReachedYourDestination: return "om-turn-finish";
  case CarDirection::ExitHighwayToLeft: return "om-exit-highway-left";
  case CarDirection::ExitHighwayToRight: return "om-exit-highway-right";
  default: return {};
  }
}

std::string PedestrianTurnIcon(PedestrianDirection dir)
{
  switch (dir)
  {
  case PedestrianDirection::GoStraight: return "om-turn-straight";
  case PedestrianDirection::TurnRight: return "om-turn-right";
  case PedestrianDirection::TurnLeft: return "om-turn-left";
  case PedestrianDirection::ReachedYourDestination: return "om-turn-finish";
  default: return {};
  }
}

std::string ErrorMessage(routing::RouterResultCode code)
{
  using routing::RouterResultCode;
  switch (code)
  {
  case RouterResultCode::NoCurrentPosition: return T("dialog_routing_error_location_not_found");
  case RouterResultCode::StartPointNotFound: return T("dialog_routing_start_not_determined");
  case RouterResultCode::EndPointNotFound: return T("dialog_routing_end_not_determined");
  case RouterResultCode::IntermediatePointNotFound: return T("dialog_routing_change_intermediate");
  case RouterResultCode::PointsInDifferentMWM: return T("routing_failed_cross_mwm_building");
  case RouterResultCode::RouteNotFound:
  case RouterResultCode::RouteNotFoundRedressRouteError: return T("dialog_routing_unable_locate_route");
  case RouterResultCode::NeedMoreMaps: return T("routing_requires_all_map");
  case RouterResultCode::FileTooOld: return T("downloader_mwm_migration_dialog");
  case RouterResultCode::TransitRouteNotFoundNoNetwork: return T("dialog_pedestrian_route_is_long_message");
  case RouterResultCode::TransitRouteNotFoundTooLongPedestrian:
    return T("dialog_pedestrian_route_is_long_message");
  default: return T("dialog_routing_application_error");
  }
}

GtkWidget * Label(char const * css = nullptr, float xalign = 0.f)
{
  auto * l = gtk_label_new("");
  gtk_label_set_xalign(GTK_LABEL(l), xalign);
  gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
  if (css)
    gtk_widget_add_css_class(l, css);
  return l;
}
}  // namespace

RoutingUi::RoutingUi(MainWindow & window) : m_window(window)
{
  BuildPlanningPanels();
  BuildNavigationPanels();

  auto & rm = GetFramework().GetRoutingManager();
  rm.SetRouteBuildingListener([this](routing::RouterResultCode code, storage::CountriesSet const & absent)
  { RunOnUiThread([this, code, absent]() { OnRouteBuilt(code, absent); }); });
  rm.SetRouteRecommendationListener([this](RoutingManager::Recommendation r)
  { RunOnUiThread([this, r]() { OnRecommendation(static_cast<int>(r)); }); });
  rm.SetTurnNotificationsLocale(CurrentLanguage());
  rm.EnableTurnNotifications(tts::IsEnabled());
}

RoutingUi::~RoutingUi()
{
  Shutdown();
}

void RoutingUi::Shutdown()
{
  if (m_navTimer)
  {
    g_source_remove(m_navTimer);
    m_navTimer = 0;
  }
  if (m_inhibitCookie)
  {
    gtk_application_uninhibit(GTK_APPLICATION(GetApp().application), m_inhibitCookie);
    m_inhibitCookie = 0;
  }
}

void RoutingUi::BuildPlanningPanels()
{
  // Top: close, router types, route points.
  m_planTop = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_add_css_class(m_planTop, "map-top-panel");

  auto * typesRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
  auto * close = MakeIconButton("window-close-symbolic", T("close").c_str(), "flat");
  gtk_widget_add_css_class(close, "circular");
  OnClicked(close, [this]() { Cancel(); });
  gtk_box_append(GTK_BOX(typesRow), close);

  auto * types = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_widget_set_hexpand(types, TRUE);
  gtk_widget_set_halign(types, GTK_ALIGN_CENTER);
  for (size_t i = 0; i < std::size(kRouters); ++i)
  {
    auto * b = gtk_toggle_button_new();
    gtk_button_set_icon_name(GTK_BUTTON(b), kRouters[i].icon);
    gtk_widget_add_css_class(b, "flat");
    gtk_widget_add_css_class(b, "route-type-button");
    if (i > 0)
      gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(b), GTK_TOGGLE_BUTTON(m_routerButtons[0]));
    auto const type = kRouters[i].type;
    Connect<void(GtkToggleButton *)>(b, "toggled", [this, type](GtkToggleButton * tb)
    {
      if (!m_updatingButtons && gtk_toggle_button_get_active(tb))
        SetRouter(type);
    });
    m_routerButtons[i] = b;
    gtk_box_append(GTK_BOX(types), b);
  }
  gtk_box_append(GTK_BOX(typesRow), types);

  auto * swap = MakeIconButton("om-swap-symbolic", nullptr, "flat");
  gtk_widget_add_css_class(swap, "circular");
  OnClicked(swap, [this]()
  {
    auto & rm = GetFramework().GetRoutingManager();
    auto points = rm.GetRoutePoints();
    if (points.size() < 2)
      return;
    // Reverse the whole route.
    rm.RemoveRoutePoints();
    for (size_t i = 0; i < points.size(); ++i)
    {
      auto p = points[points.size() - 1 - i];
      p.m_pointType = i == 0                   ? RouteMarkType::Start
                    : i + 1 == points.size()   ? RouteMarkType::Finish
                                               : RouteMarkType::Intermediate;
      p.m_intermediateIndex = i == 0 ? 0 : i - 1;
      rm.AddRoutePoint(std::move(p), false);
    }
    UpdatePointsView();
    BuildRouteIfReady();
  });
  gtk_box_append(GTK_BOX(typesRow), swap);
  gtk_box_append(GTK_BOX(m_planTop), typesRow);

  m_pointsBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_set_margin_start(m_pointsBox, 12);
  gtk_widget_set_margin_end(m_pointsBox, 12);
  gtk_widget_set_margin_bottom(m_pointsBox, 4);
  gtk_box_append(GTK_BOX(m_planTop), m_pointsBox);
  gtk_widget_set_visible(m_planTop, FALSE);
  gtk_box_append(GTK_BOX(m_window.TopSlot()), m_planTop);

  // Bottom: status and the Start button.
  m_planBottom = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_add_css_class(m_planBottom, "map-bottom-panel");
  auto * statusRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  m_spinner = gtk_spinner_new();
  gtk_box_append(GTK_BOX(statusRow), m_spinner);
  auto * texts = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  gtk_widget_set_hexpand(texts, TRUE);
  m_status = Label("title-3");
  gtk_label_set_wrap(GTK_LABEL(m_status), TRUE);
  gtk_label_set_ellipsize(GTK_LABEL(m_status), PANGO_ELLIPSIZE_NONE);
  m_statusDetails = Label("dim-label");
  gtk_label_set_wrap(GTK_LABEL(m_statusDetails), TRUE);
  gtk_label_set_ellipsize(GTK_LABEL(m_statusDetails), PANGO_ELLIPSIZE_NONE);
  gtk_box_append(GTK_BOX(texts), m_status);
  gtk_box_append(GTK_BOX(texts), m_statusDetails);
  gtk_box_append(GTK_BOX(statusRow), texts);

  m_startButton = gtk_button_new_with_label(T("p2p_start").c_str());
  gtk_widget_add_css_class(m_startButton, "suggested-action");
  gtk_widget_add_css_class(m_startButton, "pill");
  gtk_widget_set_valign(m_startButton, GTK_ALIGN_CENTER);
  OnClicked(m_startButton, [this]() { StartNavigation(); });
  gtk_box_append(GTK_BOX(statusRow), m_startButton);
  gtk_box_append(GTK_BOX(m_planBottom), statusRow);
  m_chartBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append(GTK_BOX(m_planBottom), m_chartBox);

  m_downloadButton = gtk_button_new_with_label(T("routing_download_maps_along").c_str());
  gtk_widget_add_css_class(m_downloadButton, "pill");
  OnClicked(m_downloadButton, [this]()
  {
    auto & storage = GetFramework().GetStorage();
    for (auto const & id : m_absentCountries)
      storage.DownloadNode(id);
    m_window.ShowDownloaderPage();
  });
  gtk_box_append(GTK_BOX(m_planBottom), m_downloadButton);
  gtk_widget_set_visible(m_planBottom, FALSE);
  gtk_box_append(GTK_BOX(m_window.BottomSlot()), m_planBottom);
}

void RoutingUi::BuildNavigationPanels()
{
  m_navTop = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

  auto * turnPanel = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_add_css_class(turnPanel, "nav-turn-panel");
  auto * turnBox = gtk_overlay_new();
  m_turnImage = gtk_image_new();
  gtk_image_set_pixel_size(GTK_IMAGE(m_turnImage), 64);
  gtk_overlay_set_child(GTK_OVERLAY(turnBox), m_turnImage);
  m_turnExit = Label("caption-heading", 0.5f);
  gtk_widget_set_halign(m_turnExit, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(m_turnExit, GTK_ALIGN_CENTER);
  gtk_overlay_add_overlay(GTK_OVERLAY(turnBox), m_turnExit);
  gtk_box_append(GTK_BOX(turnPanel), turnBox);

  auto * turnTexts = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_hexpand(turnTexts, TRUE);
  gtk_widget_set_valign(turnTexts, GTK_ALIGN_CENTER);
  m_turnDistance = Label("nav-turn-distance");
  m_turnStreet = Label("nav-turn-street");
  gtk_label_set_wrap(GTK_LABEL(m_turnStreet), TRUE);
  gtk_label_set_lines(GTK_LABEL(m_turnStreet), 2);
  gtk_box_append(GTK_BOX(turnTexts), m_turnDistance);
  gtk_box_append(GTK_BOX(turnTexts), m_turnStreet);
  gtk_box_append(GTK_BOX(turnPanel), turnTexts);

  m_speedLimit = gtk_label_new("");
  gtk_widget_add_css_class(m_speedLimit, "speed-limit");
  gtk_widget_set_valign(m_speedLimit, GTK_ALIGN_CENTER);
  gtk_box_append(GTK_BOX(turnPanel), m_speedLimit);
  gtk_box_append(GTK_BOX(m_navTop), turnPanel);

  m_lanes = gtk_label_new("");
  gtk_widget_add_css_class(m_lanes, "nav-next-turn");
  gtk_widget_set_halign(m_lanes, GTK_ALIGN_CENTER);
  gtk_box_append(GTK_BOX(m_navTop), m_lanes);

  m_nextTurnBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_add_css_class(m_nextTurnBox, "nav-next-turn");
  gtk_widget_set_halign(m_nextTurnBox, GTK_ALIGN_START);
  gtk_box_append(GTK_BOX(m_nextTurnBox), gtk_label_new(T("then_turn").c_str()));
  m_nextTurnImage = gtk_image_new();
  gtk_image_set_pixel_size(GTK_IMAGE(m_nextTurnImage), 28);
  gtk_box_append(GTK_BOX(m_nextTurnBox), m_nextTurnImage);
  gtk_box_append(GTK_BOX(m_navTop), m_nextTurnBox);
  gtk_widget_set_visible(m_navTop, FALSE);
  gtk_box_append(GTK_BOX(m_window.TopSlot()), m_navTop);

  // Bottom: stop, time/arrival/distance, speed, voice.
  m_navBottom = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_add_css_class(m_navBottom, "map-bottom-panel");
  m_progress = gtk_progress_bar_new();
  gtk_box_append(GTK_BOX(m_navBottom), m_progress);
  auto * row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  auto * stop = MakeIconButton("window-close-symbolic", T("navigation_stop_button").c_str(), "destructive-action");
  gtk_widget_add_css_class(stop, "circular");
  gtk_widget_set_valign(stop, GTK_ALIGN_CENTER);
  gtk_widget_set_size_request(stop, 48, 48);
  OnClicked(stop, [this]() { Cancel(); });
  gtk_box_append(GTK_BOX(row), stop);

  auto * stats = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_set_homogeneous(GTK_BOX(stats), TRUE);
  gtk_widget_set_hexpand(stats, TRUE);
  m_timeLeft = Label("nav-stats-value", 0.5f);
  m_arrival = Label("nav-stats-value", 0.5f);
  m_distLeft = Label("nav-stats-value", 0.5f);
  gtk_box_append(GTK_BOX(stats), m_timeLeft);
  gtk_box_append(GTK_BOX(stats), m_arrival);
  gtk_box_append(GTK_BOX(stats), m_distLeft);
  gtk_box_append(GTK_BOX(row), stats);

  m_speed = Label("nav-speed", 0.5f);
  gtk_label_set_justify(GTK_LABEL(m_speed), GTK_JUSTIFY_CENTER);
  gtk_label_set_ellipsize(GTK_LABEL(m_speed), PANGO_ELLIPSIZE_NONE);
  gtk_box_append(GTK_BOX(row), m_speed);

  m_voiceButton = MakeIconButton(tts::IsEnabled() ? "om-volume-symbolic" : "om-volume-off-symbolic",
                                 T("pref_tts_enable_title").c_str(), "flat");
  gtk_widget_add_css_class(m_voiceButton, "circular");
  gtk_widget_set_valign(m_voiceButton, GTK_ALIGN_CENTER);
  OnClicked(m_voiceButton, [this]()
  {
    if (!tts::IsAvailable())
    {
      m_window.Toast(T("pref_tts_enable_title") + ": " + T("pref_tts_unavailable"));
      return;
    }
    bool const enabled = !tts::IsEnabled();
    tts::SetEnabled(enabled);
    GetFramework().GetRoutingManager().EnableTurnNotifications(enabled);
    gtk_button_set_icon_name(GTK_BUTTON(m_voiceButton), enabled ? "om-volume-symbolic" : "om-volume-off-symbolic");
  });
  gtk_box_append(GTK_BOX(row), m_voiceButton);
  gtk_box_append(GTK_BOX(m_navBottom), row);
  gtk_widget_set_visible(m_navBottom, FALSE);
  gtk_box_append(GTK_BOX(m_window.BottomSlot()), m_navBottom);
}

bool RoutingUi::IsNavigating() const
{
  return GetFramework().GetRoutingManager().IsRoutingFollowing();
}

void RoutingUi::EnterPlanningMode()
{
  m_window.SetMode(MainWindow::Mode::RoutePlanning);
  gtk_widget_set_visible(m_navTop, FALSE);
  gtk_widget_set_visible(m_navBottom, FALSE);
  gtk_widget_set_visible(m_planTop, TRUE);
  gtk_widget_set_visible(m_planBottom, TRUE);
  UpdateRouterButtons();
  UpdatePointsView();
}

void RoutingUi::LeaveRoutingModes()
{
  gtk_widget_set_visible(m_planTop, FALSE);
  gtk_widget_set_visible(m_planBottom, FALSE);
  gtk_widget_set_visible(m_navTop, FALSE);
  gtk_widget_set_visible(m_navBottom, FALSE);
  m_window.SetMode(MainWindow::Mode::Default);
}

void RoutingUi::AddMyPositionAsStart()
{
  auto const & last = m_window.Location().LastLocation();
  if (!last)
    return;
  RouteMarkData start;
  start.m_pointType = RouteMarkType::Start;
  start.m_isMyPosition = true;
  start.m_title = T("p2p_your_location");
  start.m_position = mercator::FromLatLon(last->m_latitude, last->m_longitude);
  GetFramework().GetRoutingManager().AddRoutePoint(std::move(start), false);
}

void RoutingUi::StartPlanning()
{
  auto & fw = GetFramework();
  auto & rm = fw.GetRoutingManager();
  if (rm.IsRoutingFollowing())
    return;
  m_window.StartLocation();
  bool hasStart = false;
  for (auto const & p : rm.GetRoutePoints())
    hasStart |= p.m_pointType == RouteMarkType::Start;
  if (!hasStart)
    AddMyPositionAsStart();
  fw.DeactivateMapSelection();
  EnterPlanningMode();
  BuildRouteIfReady();
  if (rm.GetRoutePointsCount() < 2)
    ShowPlanningStatus();
}

void RoutingUi::SetPointFromPlacePage(RouteMarkType type)
{
  auto & fw = GetFramework();
  if (!fw.HasPlacePageInfo())
    return;
  auto const & info = fw.GetCurrentPlacePageInfo();
  auto & rm = fw.GetRoutingManager();

  RouteMarkData point;
  point.m_pointType = type;
  point.m_isMyPosition = info.IsMyPosition();
  point.m_title = info.IsMyPosition() ? T("p2p_your_location") : info.GetTitle();
  point.m_subTitle = info.GetSubtitle();
  if (point.m_title.empty())
    point.m_title = info.GetSecondarySubtitle();
  point.m_position = info.GetMercator();
  bool const optimize = type == RouteMarkType::Intermediate && routing::RoutingOptions::LoadRouteOptimizationFromSettings();
  rm.AddRoutePoint(std::move(point), optimize);

  if (type == RouteMarkType::Finish)
  {
    bool hasStart = false;
    for (auto const & p : rm.GetRoutePoints())
      hasStart |= p.m_pointType == RouteMarkType::Start;
    if (!hasStart)
      AddMyPositionAsStart();
    m_window.StartLocation();
  }

  fw.DeactivateMapSelection();
  EnterPlanningMode();
  BuildRouteIfReady();
}

void RoutingUi::RemovePoint(RouteMarkType type, size_t intermediateIndex)
{
  auto & fw = GetFramework();
  auto & rm = fw.GetRoutingManager();
  rm.RemoveRoutePoint(type, intermediateIndex);
  fw.DeactivateMapSelection();
  UpdatePointsView();
  BuildRouteIfReady();
}

void RoutingUi::Cancel()
{
  auto & rm = GetFramework().GetRoutingManager();
  bool const wasFollowing = rm.IsRoutingFollowing();
  rm.CloseRouting(true /* remove route points */);
  rm.DeleteSavedRoutePoints();
  if (wasFollowing)
    StopNavigation();
  LeaveRoutingModes();
}

void RoutingUi::SetRouter(RouterType type)
{
  auto & rm = GetFramework().GetRoutingManager();
  if (rm.GetRouter() == type && rm.IsRouteBuilt())
    return;
  rm.SetRouter(type);
  BuildRouteIfReady();
}

void RoutingUi::UpdateRouterButtons()
{
  auto const type = GetFramework().GetRoutingManager().GetRouter();
  m_updatingButtons = true;
  for (size_t i = 0; i < std::size(kRouters); ++i)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_routerButtons[i]), kRouters[i].type == type);
  m_updatingButtons = false;
}

void RoutingUi::UpdatePointsView()
{
  ClearBox(m_pointsBox);
  auto & rm = GetFramework().GetRoutingManager();
  auto points = rm.GetRoutePoints();
  std::sort(points.begin(), points.end(), [](RouteMarkData const & a, RouteMarkData const & b)
  {
    auto const rank = [](RouteMarkData const & p)
    {
      return p.m_pointType == RouteMarkType::Start ? 0 : (p.m_pointType == RouteMarkType::Finish ? 2 : 1);
    };
    if (rank(a) != rank(b))
      return rank(a) < rank(b);
    return a.m_intermediateIndex < b.m_intermediateIndex;
  });

  bool hasStart = false, hasFinish = false;
  auto const addRow = [this](char const * dotClass, std::string const & text, bool hint)
  {
    auto * row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    auto * dot = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(dot, "route-point-dot");
    gtk_widget_add_css_class(dot, dotClass);
    gtk_widget_set_valign(dot, GTK_ALIGN_CENTER);
    auto * label = Label(hint ? "dim-label" : nullptr);
    gtk_label_set_text(GTK_LABEL(label), text.c_str());
    gtk_box_append(GTK_BOX(row), dot);
    gtk_box_append(GTK_BOX(row), label);
    gtk_box_append(GTK_BOX(m_pointsBox), row);
  };

  for (auto const & p : points)
  {
    std::string title = p.m_isMyPosition ? T("p2p_your_location") : p.m_title;
    if (title.empty())
      title = p.m_subTitle;
    switch (p.m_pointType)
    {
    case RouteMarkType::Start:
      hasStart = true;
      addRow("start", title, false);
      break;
    case RouteMarkType::Intermediate: addRow("via", title, false); break;
    case RouteMarkType::Finish:
      if (!hasStart)
        addRow("start", T("p2p_from_here") + "…", true);
      hasStart = true;
      hasFinish = true;
      addRow("finish", title, false);
      break;
    }
  }
  if (!hasStart)
    addRow("start", T("p2p_from_here") + "…", true);
  if (!hasFinish)
    addRow("finish", T("pick_destination") + "…", true);
}

void RoutingUi::BuildRouteIfReady()
{
  auto & rm = GetFramework().GetRoutingManager();
  UpdatePointsView();
  UpdateRouterButtons();
  bool hasStart = false, hasFinish = false;
  for (auto const & p : rm.GetRoutePoints())
  {
    hasStart |= p.m_pointType == RouteMarkType::Start;
    hasFinish |= p.m_pointType == RouteMarkType::Finish;
  }
  if (!hasStart || !hasFinish)
  {
    rm.RemoveRoute(false);
    ShowPlanningStatus();
    return;
  }
  rm.BuildRoute();
  gtk_spinner_start(GTK_SPINNER(m_spinner));
  gtk_widget_set_visible(m_spinner, TRUE);
  ClearBox(m_chartBox);
  gtk_label_set_text(GTK_LABEL(m_status), (T("downloader_loading_ios") + "…").c_str());
  gtk_widget_set_visible(m_statusDetails, FALSE);
  gtk_widget_set_sensitive(m_startButton, FALSE);
  gtk_widget_set_visible(m_downloadButton, FALSE);
}

void RoutingUi::ShowPlanningStatus()
{
  auto & rm = GetFramework().GetRoutingManager();
  gtk_spinner_stop(GTK_SPINNER(m_spinner));
  gtk_widget_set_visible(m_spinner, FALSE);
  gtk_widget_set_visible(m_downloadButton, FALSE);
  ClearBox(m_chartBox);
  if (!rm.IsRouteBuilt())
  {
    gtk_label_set_text(GTK_LABEL(m_status), T("pick_destination").c_str());
    gtk_widget_set_visible(m_statusDetails, FALSE);
    gtk_widget_set_sensitive(m_startButton, FALSE);
    return;
  }

  routing::FollowingInfo info;
  rm.GetRouteFollowingInfo(info);
  auto const type = rm.GetRouter();
  std::string status, details;
  if (type == RouterType::Transit)
  {
    auto const transit = rm.GetTransitRouteInfo();
    status = FormatDuration(transit.m_totalTimeInSec) + " • " + FormatDistance(transit.m_totalDistInMeters);
  }
  else if (info.IsValid())
  {
    status = (type == RouterType::Ruler ? std::string() : FormatDuration(info.m_time) + " • ") +
             info.m_distToTarget.ToString();
    if (type != RouterType::Ruler)
      details = TF("routing_arrive", {FormatArrivalTime(info.m_time)});
  }
  if (type == RouterType::Pedestrian || type == RouterType::Bicycle)
  {
    ElevationInfo ei;
    if (rm.GetRouteElevationInfo(ei))
    {
      auto const alt = ei.CalculateAltitudesInfo(ElevationInfo::kDefThresholdMWM);
      if (!details.empty())
        details += "   ";
      details += "↗ " + platform::Distance::FormatAltitude(alt.GetTotalAscent()) + "  ↘ " +
                 platform::Distance::FormatAltitude(alt.GetTotalDescent());
      if (!ei.IsEmpty())
        gtk_box_append(GTK_BOX(m_chartBox), MakeElevationChart(ei, 90));
    }
  }
  gtk_label_set_text(GTK_LABEL(m_status), status.c_str());
  gtk_label_set_text(GTK_LABEL(m_statusDetails), details.c_str());
  gtk_widget_set_visible(m_statusDetails, !details.empty());
  gtk_widget_set_sensitive(m_startButton, type != RouterType::Ruler);
}

void RoutingUi::OnRouteBuilt(routing::RouterResultCode code, storage::CountriesSet const & absent)
{
  using routing::RouterResultCode;
  auto & rm = GetFramework().GetRoutingManager();
  m_absentCountries = absent;

  if (code == RouterResultCode::NoError || code == RouterResultCode::HasWarnings)
  {
    if (rm.IsRoutingFollowing())
      return;  // Rebuilt while navigating.
    ShowPlanningStatus();
    return;
  }
  if (code == RouterResultCode::Cancelled)
    return;

  gtk_spinner_stop(GTK_SPINNER(m_spinner));
  gtk_widget_set_visible(m_spinner, FALSE);
  gtk_widget_set_sensitive(m_startButton, FALSE);
  gtk_label_set_text(GTK_LABEL(m_status), T("routing_planning_error").c_str());
  std::string details = ErrorMessage(code);
  if (code == RouterResultCode::NeedMoreMaps && !absent.empty())
  {
    auto & storage = GetFramework().GetStorage();
    uint64_t total = 0;
    std::string names;
    for (auto const & id : absent)
    {
      storage::NodeAttrs attrs;
      storage.GetNodeAttrs(id, attrs);
      total += attrs.m_mwmSize;
      if (!names.empty())
        names += ", ";
      names += attrs.m_nodeLocalName;
    }
    details += "\n" + names + " (" + FormatSize(total) + ")";
    gtk_widget_set_visible(m_downloadButton, TRUE);
  }
  gtk_label_set_text(GTK_LABEL(m_statusDetails), details.c_str());
  gtk_widget_set_visible(m_statusDetails, TRUE);
  if (rm.IsRoutingFollowing())
    m_window.Toast(details);
}

void RoutingUi::OnRecommendation(int recommendation)
{
  if (recommendation != static_cast<int>(RoutingManager::Recommendation::RebuildAfterPointsLoading))
    return;
  auto & rm = GetFramework().GetRoutingManager();
  AddMyPositionAsStart();
  rm.BuildRoute();
}

void RoutingUi::StartNavigation()
{
  auto & fw = GetFramework();
  auto & rm = fw.GetRoutingManager();
  if (!rm.IsRouteBuilt())
    return;

  bool startIsMine = false;
  for (auto const & p : rm.GetRoutePoints())
    if (p.m_pointType == RouteMarkType::Start)
      startIsMine = p.m_isMyPosition;
  if (!startIsMine)
  {
    // Navigation is only possible from the current location: offer to rebuild from it.
    auto * dialog = adw_alert_dialog_new(T("p2p_only_from_current").c_str(), T("p2p_reroute_from_current").c_str());
    adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(dialog), "cancel", T("cancel").c_str(), "ok", T("ok").c_str(),
                                   nullptr);
    adw_alert_dialog_set_response_appearance(ADW_ALERT_DIALOG(dialog), "ok", ADW_RESPONSE_SUGGESTED);
    Connect<void(AdwAlertDialog *, char const *)>(dialog, "response", [this](AdwAlertDialog *, char const * r)
    {
      if (std::string_view(r) != "ok")
        return;
      if (!m_window.Location().LastLocation())
      {
        m_window.Toast(T("dialog_routing_error_location_not_found"));
        return;
      }
      auto & rm = GetFramework().GetRoutingManager();
      rm.RemoveRoutePoint(RouteMarkType::Start);
      AddMyPositionAsStart();
      BuildRouteIfReady();
    });
    adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(m_window.Window()));
    return;
  }

  rm.SetTurnNotificationsLocale(CurrentLanguage());
  rm.EnableTurnNotifications(tts::IsEnabled());
  rm.FollowRoute();

  m_window.SetMode(MainWindow::Mode::Navigation);
  gtk_widget_set_visible(m_planTop, FALSE);
  gtk_widget_set_visible(m_planBottom, FALSE);
  gtk_widget_set_visible(m_navTop, TRUE);
  gtk_widget_set_visible(m_navBottom, TRUE);
  gtk_button_set_icon_name(GTK_BUTTON(m_voiceButton), tts::IsEnabled() ? "om-volume-symbolic" : "om-volume-off-symbolic");

  if (!m_inhibitCookie)
    m_inhibitCookie = gtk_application_inhibit(GTK_APPLICATION(GetApp().application), m_window.Window(),
                                              static_cast<GtkApplicationInhibitFlags>(GTK_APPLICATION_INHIBIT_IDLE | GTK_APPLICATION_INHIBIT_SUSPEND),
                                              "Navigation");
  if (!m_navTimer)
    m_navTimer = g_timeout_add_seconds(1, [](gpointer data) -> gboolean
    {
      static_cast<RoutingUi *>(data)->UpdateNavigation();
      return G_SOURCE_CONTINUE;
    }, this);
  UpdateNavigation();
}

void RoutingUi::StopNavigation()
{
  Shutdown();
  m_lastTurnIcon.clear();
}

void RoutingUi::OnLocationUpdate(location::GpsInfo const & info)
{
  m_lastSpeed = info.HasSpeed() ? info.m_speed : -1;
  auto & rm = GetFramework().GetRoutingManager();
  if (!rm.IsRoutingActive())
    return;
  if (rm.IsRoutingFollowing())
  {
    UpdateNavigation();
    std::vector<std::string> notifications;
    rm.GenerateNotifications(notifications, true /* announceStreets */);
    if (!notifications.empty() && tts::IsEnabled())
      tts::Speak(notifications);
  }
  else if (rm.IsRouteBuilt())
  {
    ShowPlanningStatus();
  }
}

void RoutingUi::UpdateNavigation()
{
  auto & fw = GetFramework();
  auto & rm = fw.GetRoutingManager();
  if (!rm.IsRoutingFollowing())
    return;

  if (rm.IsRouteFinished())
  {
    m_window.Toast(T("trip_finished"));
    Cancel();
    return;
  }

  routing::FollowingInfo info;
  rm.GetRouteFollowingInfo(info);
  if (!info.IsValid())
  {
    if (rm.IsRouteRebuildingOnly() || rm.IsRouteBuilding())
      gtk_label_set_text(GTK_LABEL(m_turnStreet), (T("downloader_loading_ios") + "…").c_str());
    return;
  }

  bool const pedestrian = rm.GetRouter() == RouterType::Pedestrian || rm.GetRouter() == RouterType::Transit;
  std::string const icon = pedestrian ? PedestrianTurnIcon(info.m_pedestrianTurn) : CarTurnIcon(info.m_turn, info.m_exitNum);
  if (icon != m_lastTurnIcon)
  {
    m_lastTurnIcon = icon;
    gtk_image_set_from_icon_name(GTK_IMAGE(m_turnImage), icon.c_str());
  }
  bool const roundabout = !pedestrian && info.m_exitNum > 0 && icon == "om-turn-round";
  gtk_label_set_text(GTK_LABEL(m_turnExit), roundabout ? std::to_string(info.m_exitNum).c_str() : "");
  gtk_label_set_text(GTK_LABEL(m_turnDistance), info.m_distToTurn.ToString().c_str());
  std::string street = info.m_nextStreetName.empty() ? info.m_currentStreetName : info.m_nextStreetName;
  gtk_label_set_text(GTK_LABEL(m_turnStreet), street.c_str());
  gtk_widget_set_visible(m_turnStreet, !street.empty());

  // Next turn hint when it comes right after the current one.
  bool const showNext = !pedestrian && info.m_nextTurn != CarDirection::None && info.m_distToTurn.GetDistance() < 1000;
  if (showNext)
    gtk_image_set_from_icon_name(GTK_IMAGE(m_nextTurnImage), CarTurnIcon(info.m_nextTurn, 0).c_str());
  gtk_widget_set_visible(m_nextTurnBox, showNext);

  // Lanes: arrows for each lane, the recommended direction highlighted (like the Android lanes strip).
  if (!info.m_lanes.empty())
  {
    using routing::turns::lanes::LaneWay;
    static std::pair<LaneWay, char const *> const kGlyphs[] = {
        {LaneWay::ReverseLeft, "↶"}, {LaneWay::SharpLeft, "↙"},    {LaneWay::Left, "↰"},
        {LaneWay::MergeToLeft, "↖"}, {LaneWay::SlightLeft, "↖"},   {LaneWay::Through, "↑"},
        {LaneWay::SlightRight, "↗"}, {LaneWay::MergeToRight, "↗"}, {LaneWay::Right, "↱"},
        {LaneWay::SharpRight, "↘"},  {LaneWay::ReverseRight, "↷"},
    };
    std::string markup;
    for (auto const & lane : info.m_lanes)
    {
      if (!markup.empty())
        markup += "<span alpha='35%'> │ </span>";
      bool any = false;
      for (auto const & [way, glyph] : kGlyphs)
      {
        if (!lane.laneWays.Contains(way))
          continue;
        bool const rec = lane.recommendedWay == way;
        markup += std::string(rec ? "<span alpha='100%' weight='bold'>" : "<span alpha='40%'>") + glyph + "</span>";
        any = true;
      }
      if (!any)
        markup += lane.recommendedWay != LaneWay::None ? "<b>↑</b>" : "<span alpha='40%'>↑</span>";
    }
    gtk_label_set_markup(GTK_LABEL(m_lanes), ("<span size='x-large'>" + markup + "</span>").c_str());
  }
  gtk_widget_set_visible(m_lanes, !info.m_lanes.empty());

  if (info.m_speedLimitMps > 0)
  {
    auto const units = measurement_utils::GetMeasurementUnits();
    gtk_label_set_text(GTK_LABEL(m_speedLimit),
                       std::to_string(measurement_utils::FormatSpeed(info.m_speedLimitMps, units)).c_str());
  }
  gtk_widget_set_visible(m_speedLimit, info.m_speedLimitMps > 0);

  gtk_label_set_text(GTK_LABEL(m_timeLeft), FormatDuration(info.m_time).c_str());
  gtk_label_set_text(GTK_LABEL(m_arrival), FormatArrivalTime(info.m_time).c_str());
  gtk_label_set_text(GTK_LABEL(m_distLeft), info.m_distToTarget.ToString().c_str());
  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(m_progress), std::clamp(info.m_completionPercent / 100.0, 0.0, 1.0));

  if (m_lastSpeed >= 0)
  {
    auto const units = measurement_utils::GetMeasurementUnits();
    auto const markup = measurement_utils::FormatSpeedNumeric(m_lastSpeed, units) + "\n<small>" +
                        platform::GetLocalizedSpeedUnits(units) + "</small>";
    gtk_label_set_markup(GTK_LABEL(m_speed), markup.c_str());
    if (rm.IsSpeedCamLimitExceeded() || (info.m_speedLimitMps > 0 && m_lastSpeed > info.m_speedLimitMps + 1.5))
      gtk_widget_add_css_class(m_speed, "over");
    else
      gtk_widget_remove_css_class(m_speed, "over");
  }
  gtk_widget_set_visible(m_speed, m_lastSpeed >= 0);
}

void RoutingUi::SaveForNextLaunch()
{
  auto & rm = GetFramework().GetRoutingManager();
  if (rm.IsRoutingActive() && rm.IsRoutingFollowing())
    rm.SaveRoutePoints();
}

void RoutingUi::RestoreSavedRoute()
{
  auto & rm = GetFramework().GetRoutingManager();
  if (!rm.HasSavedRoutePoints())
    return;
  rm.LoadRoutePoints([this](bool success)
  {
    if (!success)
      return;
    RunOnUiThread([this]()
    {
      EnterPlanningMode();
      BuildRouteIfReady();
    });
  });
}
}  // namespace omgtk
