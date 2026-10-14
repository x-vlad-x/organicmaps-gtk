#include "settings.hpp"

#include "app.hpp"
#include "l10n.hpp"
#include "main_window.hpp"
#include "tts.hpp"

#include "map/framework.hpp"
#include "map/routing_manager.hpp"

#include "routing/routing_options.hpp"
#include "routing/speed_camera_manager.hpp"

#include "platform/measurement_utils.hpp"
#include "platform/platform.hpp"
#include "platform/settings.hpp"

#include "indexer/map_style.hpp"

namespace lichen
{
namespace
{
char const kZoomButtonsKey[] = "GtkZoomButtons";
char const kKeepScreenOnKey[] = "GtkKeepScreenOn";
char const kNightModeKey[] = "GtkNightMode";

GtkWidget * SwitchRow(std::string const & title, std::string const & subtitle, bool active,
                      std::function<void(bool)> onChange)
{
  auto * row = adw_switch_row_new();
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), title.c_str());
  if (!subtitle.empty())
    adw_action_row_set_subtitle(ADW_ACTION_ROW(row), subtitle.c_str());
  adw_switch_row_set_active(ADW_SWITCH_ROW(row), active);
  Connect<void(GObject *, GParamSpec *)>(row, "notify::active", [row, onChange](GObject *, GParamSpec *)
  { onChange(adw_switch_row_get_active(ADW_SWITCH_ROW(row))); });
  return row;
}

GtkWidget * ComboRow(std::string const & title, std::vector<std::string> const & items, guint selected,
                     std::function<void(guint)> onChange)
{
  auto * model = gtk_string_list_new(nullptr);
  for (auto const & i : items)
    gtk_string_list_append(model, i.c_str());
  auto * row = adw_combo_row_new();
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), title.c_str());
  adw_combo_row_set_model(ADW_COMBO_ROW(row), G_LIST_MODEL(model));
  g_object_unref(model);
  adw_combo_row_set_selected(ADW_COMBO_ROW(row), selected);
  Connect<void(GObject *, GParamSpec *)>(row, "notify::selected", [row, onChange](GObject *, GParamSpec *)
  { onChange(adw_combo_row_get_selected(ADW_COMBO_ROW(row))); });
  return row;
}

}  // namespace

NightMode GetNightMode()
{
  // A GNOME app follows the system dark style unless the user chose otherwise.
  int v = static_cast<int>(NightMode::System);
  settings::TryGet(kNightModeKey, v);
  return v >= 0 && v <= 2 ? static_cast<NightMode>(v) : NightMode::System;
}

void SyncAppColorScheme()
{
  // The UI follows the map: dark map style -> dark Adwaita, like the Android app.
  auto const mode = GetNightMode();
  auto const scheme = mode == NightMode::On    ? ADW_COLOR_SCHEME_FORCE_DARK
                    : mode == NightMode::Off   ? ADW_COLOR_SCHEME_FORCE_LIGHT
                                               : ADW_COLOR_SCHEME_DEFAULT;
  adw_style_manager_set_color_scheme(adw_style_manager_get_default(), scheme);
}

void ApplyNightMode(NightMode mode)
{
  settings::Set(kNightModeKey, static_cast<int>(mode));
  SyncAppColorScheme();
  bool dark = mode == NightMode::On;
  if (mode == NightMode::System)
    dark = adw_style_manager_get_dark(adw_style_manager_get_default());
  GetFramework().SwitchToMapAppearance(dark ? MapAppearance::Dark : MapAppearance::Light);
}

namespace
{
void SetRoadOption(routing::RoutingOptions::Option road, bool avoid)
{
  auto options = routing::RoutingOptions::LoadOptionsFromSettings(routing::VehicleType::Car);
  if (avoid)
    options.Add(road);
  else
    options.Remove(road);
  routing::RoutingOptions::SaveOptionsToSettings(options);
  auto & rm = GetFramework().GetRoutingManager();
  if (rm.IsRoutingActive() && !rm.IsRoutingFollowing() && rm.GetRoutePointsCount() >= 2)
    rm.BuildRoute();
}

GtkWidget * LayerTile(char const * icon, std::string const & title, bool active, std::function<void(bool)> onToggle)
{
  auto * button = gtk_toggle_button_new();
  gtk_widget_add_css_class(button, "flat");
  auto * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  auto * image = gtk_image_new_from_icon_name(icon);
  gtk_image_set_pixel_size(GTK_IMAGE(image), 56);
  auto * label = gtk_label_new(title.c_str());
  gtk_label_set_wrap(GTK_LABEL(label), TRUE);
  gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
  gtk_label_set_max_width_chars(GTK_LABEL(label), 10);
  gtk_widget_add_css_class(label, "caption");
  gtk_box_append(GTK_BOX(box), image);
  gtk_box_append(GTK_BOX(box), label);
  gtk_button_set_child(GTK_BUTTON(button), box);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), active);
  Connect<void(GtkToggleButton *)>(button, "toggled", [onToggle](GtkToggleButton * b)
  { onToggle(gtk_toggle_button_get_active(b)); });
  return button;
}
}  // namespace

bool ZoomButtonsEnabled()
{
  bool v = true;
  settings::TryGet(kZoomButtonsKey, v);
  return v;
}

bool KeepScreenOn()
{
  bool v = false;
  settings::TryGet(kKeepScreenOnKey, v);
  return v;
}

void ShowSettingsDialog(MainWindow & window)
{
  auto & fw = GetFramework();
  auto * dialog = adw_preferences_dialog_new();
  adw_dialog_set_title(ADW_DIALOG(dialog), T("settings").c_str());

  // General.
  auto * page = adw_preferences_page_new();
  adw_preferences_page_set_title(ADW_PREFERENCES_PAGE(page), T("prefs_group_interface").c_str());
  adw_preferences_page_set_icon_name(ADW_PREFERENCES_PAGE(page), "emblem-system-symbolic");

  auto * general = adw_preferences_group_new();
  auto const units = measurement_utils::GetMeasurementUnits();
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(general),
                            ComboRow(T("measurement_units"), {T("kilometres"), T("miles")},
                                     units == measurement_utils::Units::Metric ? 0 : 1, [](guint i)
  {
    settings::Set(settings::kMeasurementUnits,
                  i == 0 ? measurement_utils::Units::Metric : measurement_utils::Units::Imperial);
    GetFramework().SetupMeasurementSystem();
  }));

  // The same choices as the CoMaps Android app: off, on, auto.
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(general),
                            ComboRow(T("pref_map_style_title"), {T("off"), T("on"), T("auto")},
                                     static_cast<guint>(GetNightMode()), [](guint i)
  { ApplyNightMode(static_cast<NightMode>(i)); }));

  bool allow3d = true, allow3dBuildings = true;
  fw.Load3dMode(allow3d, allow3dBuildings);
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(general), SwitchRow(T("pref_map_3d_title"), {}, allow3d, [](bool on)
  {
    bool a = true, b = true;
    GetFramework().Load3dMode(a, b);
    GetFramework().Save3dMode(on, b);
    GetFramework().Allow3dMode(on, b);
  }));
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(general),
                            SwitchRow(T("pref_map_3d_buildings_title"), {}, allow3dBuildings, [](bool on)
  {
    bool a = true, b = true;
    GetFramework().Load3dMode(a, b);
    GetFramework().Save3dMode(a, on);
    GetFramework().Allow3dMode(a, on);
  }));
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(general),
                            SwitchRow(T("pref_map_auto_zoom"), {}, fw.LoadAutoZoom(), [](bool on)
  {
    GetFramework().AllowAutoZoom(on);
    GetFramework().SaveAutoZoom(on);
  }));
  {
    // CoMaps replaced the "large font" switch with a label scale, 100-400% as in its Android app.
    auto * row = adw_spin_row_new_with_range(100, 400, 25);
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), T("pref_font_size").c_str());
    adw_spin_row_set_value(ADW_SPIN_ROW(row), fw.LoadFontScaleFactor() * 100);
    Connect<void(GObject *, GParamSpec *)>(row, "notify::value", [row](GObject *, GParamSpec *)
    { GetFramework().SetFontScaleFactor(adw_spin_row_get_value(ADW_SPIN_ROW(row)) / 100); });
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(general), row);
  }
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(general),
                            SwitchRow(T("transliteration_title"), {},
                                      fw.LoadTransliteration(), [](bool on)
  {
    GetFramework().SaveTransliteration(on);
    GetFramework().AllowTransliteration(on);
  }));
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(general),
                            SwitchRow(T("pref_zoom_title"), {}, ZoomButtonsEnabled(), [&window](bool on)
  {
    settings::Set(kZoomButtonsKey, on);
    window.SetMode(window.GetMode());
  }));
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(general),
                            SwitchRow(T("enable_keep_screen_on"), {}, KeepScreenOn(), [&window](bool on)
  {
    settings::Set(kKeepScreenOnKey, on);
    window.UpdateKeepScreenOn();
  }));
  adw_preferences_page_add(ADW_PREFERENCES_PAGE(page), ADW_PREFERENCES_GROUP(general));
  adw_preferences_dialog_add(ADW_PREFERENCES_DIALOG(dialog), ADW_PREFERENCES_PAGE(page));

  // Navigation.
  auto * navPage = adw_preferences_page_new();
  adw_preferences_page_set_title(ADW_PREFERENCES_PAGE(navPage), T("prefs_group_route").c_str());
  adw_preferences_page_set_icon_name(ADW_PREFERENCES_PAGE(navPage), "om-route-symbolic");

  auto * voice = adw_preferences_group_new();
  adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(voice), T("pref_tts_enable_title").c_str());
  auto * ttsRow = SwitchRow(T("pref_tts_enable_title"), tts::IsAvailable() ? std::string() : T("pref_tts_unavailable"),
                            tts::IsEnabled(), [](bool on)
  {
    tts::SetEnabled(on);
    GetFramework().GetRoutingManager().EnableTurnNotifications(on);
  });
  gtk_widget_set_sensitive(ttsRow, tts::IsAvailable());
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(voice), ttsRow);
  auto * testVoice = adw_button_row_new();
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(testVoice), T("pref_tts_test_voice_title").c_str());
  gtk_widget_set_sensitive(testVoice, tts::IsAvailable());
  Connect<void(AdwButtonRow *)>(testVoice, "activated", [](AdwButtonRow *)
  {
    // A phrase the core has in every language.
    tts::Speak(T("pref_tts_enable_title"));
  });
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(voice), testVoice);

  auto & speedCams = fw.GetRoutingManager().GetSpeedCamManager();
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(voice),
                            ComboRow(T("speedcams_alert_title"),
                                     {T("pref_tts_speedcams_auto"), T("pref_tts_speedcams_always"),
                                      T("pref_tts_speedcams_never")},
                                     static_cast<guint>(speedCams.GetMode()), [](guint i)
  {
    GetFramework().GetRoutingManager().GetSpeedCamManager().SetMode(static_cast<routing::SpeedCameraManagerMode>(i));
  }));
  adw_preferences_page_add(ADW_PREFERENCES_PAGE(navPage), ADW_PREFERENCES_GROUP(voice));

  auto * avoid = adw_preferences_group_new();
  adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(avoid), T("routing_options_title").c_str());
  auto const options = routing::RoutingOptions::LoadOptionsFromSettings(routing::VehicleType::Car);
  using Road = routing::RoutingOptions::Option;
  struct
  {
    char const * key;
    Road road;
  } const roads[] = {{"avoid_tolls", Road::Toll}, {"avoid_unpaved", Road::Dirty}, {"avoid_ferry", Road::Ferry},
                     {"avoid_motorways", Road::Motorway}};
  for (auto const & r : roads)
  {
    auto const road = r.road;
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(avoid), SwitchRow(T(r.key), {}, options.Has(road), [road](bool on)
    { SetRoadOption(road, on); }));
  }
  // CoMaps has no route optimization switch: it always reorders new intermediate stops.
  adw_preferences_page_add(ADW_PREFERENCES_PAGE(navPage), ADW_PREFERENCES_GROUP(avoid));
  adw_preferences_dialog_add(ADW_PREFERENCES_DIALOG(dialog), ADW_PREFERENCES_PAGE(navPage));

  adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(window.Window()));
}

void ShowLayersDialog(MainWindow & window)
{
  auto & fw = GetFramework();
  auto * dialog = adw_dialog_new();
  adw_dialog_set_title(dialog, T("layers_title").c_str());
  adw_dialog_set_content_width(dialog, 420);
  auto * toolbar = adw_toolbar_view_new();
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar), adw_header_bar_new());

  auto * flow = gtk_flow_box_new();
  gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flow), GTK_SELECTION_NONE);
  gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(flow), TRUE);
  gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(flow), 3);
  gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow), 5);
  SetMarginAll(flow, 12);

  // CoMaps shows hiking and cycling routes, road and transit details through exclusive map modes
  // (on Linux the walking mode is the default) and keeps outdoors and contour lines as layers.
  struct Mode
  {
    MapMode mode;
    char const * icon;
    char const * title;
  } const modes[] = {{MapMode::Walking, "om-layer-hiking", "mode_walking"},
                     {MapMode::Cycling, "om-layer-cycling", "mode_cycling"},
                     {MapMode::Driving, "om-layer-traffic", "mode_driving"},
                     {MapMode::PublicTransport, "om-layer-subway", "mode_public-transport"}};
  GtkToggleButton * group = nullptr;
  for (auto const & m : modes)
  {
    auto const mode = m.mode;
    auto * tile = LayerTile(m.icon, T(m.title), fw.CurrentMapMode() == mode, [mode](bool on)
    {
      if (on)
        GetFramework().SwitchToMapMode(mode);
    });
    if (group)
      gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(tile), group);
    else
      group = GTK_TOGGLE_BUTTON(tile);
    gtk_flow_box_append(GTK_FLOW_BOX(flow), tile);
  }
  gtk_flow_box_append(GTK_FLOW_BOX(flow), LayerTile("om-layer-outdoors", T("button_layer_outdoor"),
                                                    fw.HasOutdoorLayer(),
                                                    [](bool on) { GetFramework().SetOutdoorLayer(on); }));
  gtk_flow_box_append(GTK_FLOW_BOX(flow), LayerTile("om-layer-isolines", T("button_layer_isolines"),
                                                    fw.HasContourLinesLayer(),
                                                    [](bool on) { GetFramework().SetContourLinesLayer(on); }));

  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar), flow);
  adw_dialog_set_child(dialog, toolbar);
  adw_dialog_present(dialog, GTK_WIDGET(window.Window()));
}

void ShowAboutDialog(MainWindow & window)
{
  auto * about = adw_about_dialog_new();
  adw_about_dialog_set_application_name(ADW_ABOUT_DIALOG(about), kAppName);
  adw_about_dialog_set_application_icon(ADW_ABOUT_DIALOG(about), kAppId);
  adw_about_dialog_set_version(ADW_ABOUT_DIALOG(about), LICHEN_VERSION);
  adw_about_dialog_set_developer_name(ADW_ABOUT_DIALOG(about), "Vladislav Shadiuk");
  adw_about_dialog_set_website(ADW_ABOUT_DIALOG(about), kProjectUrl);
  adw_about_dialog_add_link(ADW_ABOUT_DIALOG(about), "CoMaps", "https://www.comaps.app");
  adw_about_dialog_set_issue_url(ADW_ABOUT_DIALOG(about), (std::string(kProjectUrl) + "/issues").c_str());
  adw_about_dialog_set_license_type(ADW_ABOUT_DIALOG(about), GTK_LICENSE_APACHE_2_0);
  auto const dataVersion = std::to_string(GetFramework().GetCurrentDataVersion());
  adw_about_dialog_set_comments(ADW_ABOUT_DIALOG(about),
      (std::string("A prototype of an unofficial GTK interface on the CoMaps core. "
                   "Not affiliated with or endorsed by CoMaps.\n\n") +
       kBasedOnMarkup + "\n" + kMapDataMarkup + "\n\n" +
       "Core: " + GetPlatform().Version() + "\nMap data: " + dataVersion).c_str());
  adw_about_dialog_add_legal_section(ADW_ABOUT_DIALOG(about), "OpenStreetMap", "© OpenStreetMap contributors",
                                     GTK_LICENSE_CUSTOM, "Map data is available under the Open Database License (ODbL).");
  adw_dialog_present(ADW_DIALOG(about), GTK_WIDGET(window.Window()));
}
}  // namespace lichen
