#include "bookmarks.hpp"

#include "l10n.hpp"
#include "main_window.hpp"
#include "style.hpp"

#include "map/bookmark_helpers.hpp"
#include "map/framework.hpp"

#include "platform/localization.hpp"

#include "geometry/distance_on_sphere.hpp"
#include "geometry/mercator.hpp"

#include "base/string_utils.hpp"

namespace lichen
{
namespace
{
struct ColorInfo
{
  kml::PredefinedColor color;
  char const * key;
  char const * rgb;
};
ColorInfo const kColors[] = {
    {kml::PredefinedColor::Red, "red", "#e51b23"},
    {kml::PredefinedColor::Pink, "pink", "#ff4182"},
    {kml::PredefinedColor::Purple, "purple", "#9b24b2"},
    {kml::PredefinedColor::DeepPurple, "deep_purple", "#6639bf"},
    {kml::PredefinedColor::Blue, "blue", "#0066cc"},
    {kml::PredefinedColor::LightBlue, "light_blue", "#249cf2"},
    {kml::PredefinedColor::Cyan, "cyan", "#14becd"},
    {kml::PredefinedColor::Teal, "teal", "#00a58c"},
    {kml::PredefinedColor::Green, "green", "#3c8c3c"},
    {kml::PredefinedColor::Lime, "lime", "#93bf39"},
    {kml::PredefinedColor::Yellow, "yellow", "#ffc800"},
    {kml::PredefinedColor::Orange, "orange", "#ff9600"},
    {kml::PredefinedColor::DeepOrange, "deep_orange", "#f06432"},
    {kml::PredefinedColor::Brown, "brown", "#804633"},
    {kml::PredefinedColor::Gray, "gray", "#737373"},
    {kml::PredefinedColor::BlueGray, "blue_gray", "#597380"},
};

char const * ColorRgb(kml::PredefinedColor c)
{
  for (auto const & ci : kColors)
    if (ci.color == c)
      return ci.rgb;
  return "#e51b23";
}

// A small coloured circle (CSS background via a per-widget provider).
GtkWidget * ColorDot(char const * rgb, int size = 18)
{
  auto * dot = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_size_request(dot, size, size);
  gtk_widget_set_valign(dot, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class(dot, "color-dot");
  auto * provider = gtk_css_provider_new();
  std::string const css = std::string(".color-dot { background-color: ") + rgb + "; }";
  gtk_css_provider_load_from_string(provider, css.c_str());
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_style_context_add_provider(gtk_widget_get_style_context(dot), GTK_STYLE_PROVIDER(provider),
                                 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  G_GNUC_END_IGNORE_DEPRECATIONS
  g_object_unref(provider);
  return dot;
}

std::string BookmarkSubtitle(MainWindow & window, Bookmark const & bm)
{
  std::string s = bm.GetDescription();
  if (s.size() > 80)
    s = s.substr(0, 80) + "…";
  if (auto const & last = window.Location().LastLocation())
  {
    auto const d = ms::DistanceOnEarth(last->GetLatLon(), mercator::ToLatLon(bm.GetPivot()));
    s = FormatDistance(d) + (s.empty() ? "" : " • " + s);
  }
  return s;
}
}  // namespace

void AskText(GtkWidget * parent, std::string const & title, std::string const & initial,
             std::function<void(std::string const &)> onOk)
{
  auto * dialog = adw_alert_dialog_new(title.c_str(), nullptr);
  auto * entry = gtk_entry_new();
  gtk_editable_set_text(GTK_EDITABLE(entry), initial.c_str());
  gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
  adw_alert_dialog_set_extra_child(ADW_ALERT_DIALOG(dialog), entry);
  adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(dialog), "cancel", T("cancel").c_str(), "ok", T("ok").c_str(),
                                 nullptr);
  adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "ok");
  adw_alert_dialog_set_response_appearance(ADW_ALERT_DIALOG(dialog), "ok", ADW_RESPONSE_SUGGESTED);
  Connect<void(AdwAlertDialog *, char const *)>(dialog, "response", [entry, onOk](AdwAlertDialog *, char const * r)
  {
    if (std::string_view(r) == "ok")
      onOk(Trim(gtk_editable_get_text(GTK_EDITABLE(entry))));
  });
  adw_dialog_present(ADW_DIALOG(dialog), parent);
  gtk_widget_grab_focus(entry);
}

BookmarksPage::BookmarksPage(MainWindow & window) : m_window(window)
{
  m_listsToolbar = adw_toolbar_view_new();
  auto * header = adw_header_bar_new();
  auto * add = MakeIconButton("list-add-symbolic", T("bookmarks_create_new_group").c_str());
  OnClicked(add, [this]() { CreateList([this](kml::MarkGroupId) { FillLists(); }); });
  adw_header_bar_pack_end(ADW_HEADER_BAR(header), add);
  auto * import = MakeIconButton("document-open-symbolic", T("bookmarks_import").c_str());
  OnClicked(import, [this]() { ChooseFileToImport(); });
  adw_header_bar_pack_end(ADW_HEADER_BAR(header), import);
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(m_listsToolbar), header);

  m_listsPage = adw_navigation_page_new(m_listsToolbar, T("bookmarks_and_tracks").c_str());
  g_object_ref_sink(m_listsPage);
  Connect<void(AdwNavigationPage *)>(m_listsPage, "showing", [this](AdwNavigationPage *) { FillLists(); });
}

BookmarksPage::~BookmarksPage()
{
  g_object_unref(m_listsPage);
}

void BookmarksPage::Show()
{
  m_window.PushPage(m_listsPage);
}

void BookmarksPage::FillLists()
{
  auto & bm = GetFramework().GetBookmarkManager();
  auto * page = adw_preferences_page_new();

  if (GetFramework().IsTrackRecordingEnabled())
  {
    auto * recGroup = adw_preferences_group_new();
    auto * row = adw_action_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), T("track_recording").c_str());
    auto * icon = gtk_image_new_from_icon_name("om-track-recording");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 32);
    adw_action_row_add_prefix(ADW_ACTION_ROW(row), icon);
    auto * stop = gtk_button_new_with_label(T("stop_track_recording").c_str());
    gtk_widget_set_valign(stop, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class(stop, "destructive-action");
    OnClicked(stop, [this]() { ToggleTrackRecording(); });
    adw_action_row_add_suffix(ADW_ACTION_ROW(row), stop);
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(recGroup), row);
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(page), ADW_PREFERENCES_GROUP(recGroup));
  }

  auto * group = adw_preferences_group_new();
  adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(group), T("bookmark_lists").c_str());

  auto * toggleAll = gtk_button_new_with_label(
      (bm.AreAllCategoriesVisible() ? T("bookmark_lists_hide_all") : T("bookmark_lists_show_all")).c_str());
  gtk_widget_add_css_class(toggleAll, "flat");
  OnClicked(toggleAll, [this]()
  {
    auto & bm = GetFramework().GetBookmarkManager();
    bm.SetAllCategoriesVisibility(!bm.AreAllCategoriesVisible());
    FillLists();
  });
  adw_preferences_group_set_header_suffix(ADW_PREFERENCES_GROUP(group), toggleAll);

  for (auto const id : bm.GetSortedBmGroupIdList())
  {
    auto * row = adw_action_row_new();
    adw_preferences_row_set_use_markup(ADW_PREFERENCES_ROW(row), FALSE);
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), bm.GetCategoryName(id).c_str());
    auto const bookmarks = bm.GetUserMarkIds(id).size();
    auto const tracks = bm.GetTrackIds(id).size();
    std::string subtitle = T("bookmarks") + ": " + std::to_string(bookmarks);
    if (tracks > 0)
      subtitle += ", " + T("tracks_title") + ": " + std::to_string(tracks);
    adw_action_row_set_subtitle(ADW_ACTION_ROW(row), subtitle.c_str());

    auto * eye = gtk_toggle_button_new();
    bool const visible = bm.IsVisible(id);
    gtk_button_set_icon_name(GTK_BUTTON(eye), visible ? "view-reveal-symbolic" : "view-conceal-symbolic");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(eye), visible);
    gtk_widget_add_css_class(eye, "flat");
    gtk_widget_set_valign(eye, GTK_ALIGN_CENTER);
    Connect<void(GtkToggleButton *)>(eye, "toggled", [id](GtkToggleButton * b)
    {
      bool const v = gtk_toggle_button_get_active(b);
      GetFramework().GetBookmarkManager().GetEditSession().SetIsVisible(id, v);
      gtk_button_set_icon_name(GTK_BUTTON(b), v ? "view-reveal-symbolic" : "view-conceal-symbolic");
    });
    adw_action_row_add_prefix(ADW_ACTION_ROW(row), eye);
    adw_action_row_add_suffix(ADW_ACTION_ROW(row), gtk_image_new_from_icon_name("go-next-symbolic"));
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), TRUE);
    Connect<void(AdwActionRow *)>(row, "activated", [this, id](AdwActionRow *) { ShowCategory(id); });
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), row);
  }
  adw_preferences_page_add(ADW_PREFERENCES_PAGE(page), ADW_PREFERENCES_GROUP(group));
  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(m_listsToolbar), page);
}

void BookmarksPage::ShowCategory(kml::MarkGroupId id)
{
  auto & bm = GetFramework().GetBookmarkManager();
  auto * toolbar = adw_toolbar_view_new();
  auto * header = adw_header_bar_new();

  auto * menuButton = gtk_menu_button_new();
  gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menuButton), "view-more-symbolic");
  auto * popover = gtk_popover_new();
  auto * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  auto const addItem = [&](std::string const & label, std::function<void()> fn, bool destructive = false)
  {
    auto * b = gtk_button_new_with_label(label.c_str());
    gtk_widget_add_css_class(b, "flat");
    if (destructive)
      gtk_widget_add_css_class(b, "error");
    gtk_widget_set_halign(gtk_button_get_child(GTK_BUTTON(b)), GTK_ALIGN_START);
    OnClicked(b, [popover, fn]()
    {
      gtk_popover_popdown(GTK_POPOVER(popover));
      fn();
    });
    gtk_box_append(GTK_BOX(box), b);
  };
  addItem(T("zoom_to_country"), [this, id]()
  {
    m_window.PopToMap();
    GetFramework().ShowBookmarkCategory(id);
  });
  addItem(T("edit"), [this, id, toolbar]()
  {
    RenameList(id, [this, id, toolbar]() { FillCategory(id, toolbar); });
  });
  addItem(T("delete_list"), [this, id]()
  {
    auto & bm = GetFramework().GetBookmarkManager();
    if (bm.GetBmGroupsCount() <= 1)
      return;  // At least one list must remain.
    auto * dialog = adw_alert_dialog_new(TF("delete_track_dialog_title", {bm.GetCategoryName(id)}).c_str(), nullptr);
    adw_alert_dialog_add_responses(ADW_ALERT_DIALOG(dialog), "cancel", T("cancel").c_str(), "delete",
                                   T("delete").c_str(), nullptr);
    adw_alert_dialog_set_response_appearance(ADW_ALERT_DIALOG(dialog), "delete", ADW_RESPONSE_DESTRUCTIVE);
    Connect<void(AdwAlertDialog *, char const *)>(dialog, "response", [this, id](AdwAlertDialog *, char const * r)
    {
      if (std::string_view(r) != "delete")
        return;
      GetFramework().GetBookmarkManager().GetEditSession().DeleteBmCategory(id, false);
      Show();
      m_window.PopToMap();
      Show();
    });
    adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(m_window.Window()));
  }, true);
  gtk_popover_set_child(GTK_POPOVER(popover), box);
  gtk_menu_button_set_popover(GTK_MENU_BUTTON(menuButton), popover);
  adw_header_bar_pack_end(ADW_HEADER_BAR(header), menuButton);
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar), header);

  auto * page = adw_navigation_page_new(toolbar, bm.GetCategoryName(id).c_str());
  Connect<void(AdwNavigationPage *)>(page, "showing", [this, id, toolbar](AdwNavigationPage *)
  {
    if (GetFramework().GetBookmarkManager().HasBmCategory(id))
      FillCategory(id, toolbar);
  });
  m_window.PushPage(page);
}

void BookmarksPage::FillCategory(kml::MarkGroupId id, GtkWidget * toolbar)
{
  auto & bm = GetFramework().GetBookmarkManager();
  auto * page = adw_preferences_page_new();
  auto const & bmIds = bm.GetUserMarkIds(id);
  auto const & trackIds = bm.GetTrackIds(id);

  if (bmIds.empty() && trackIds.empty())
  {
    auto * status = adw_status_page_new();
    adw_status_page_set_icon_name(ADW_STATUS_PAGE(status), "starred-symbolic");
    adw_status_page_set_title(ADW_STATUS_PAGE(status), T("bookmarks_empty_list_title").c_str());
    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar), status);
    return;
  }

  if (!trackIds.empty())
  {
    auto * group = adw_preferences_group_new();
    adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(group), T("tracks_title").c_str());
    for (auto const tid : trackIds)
    {
      auto const * track = bm.GetTrack(tid);
      if (!track)
        continue;
      auto * row = adw_action_row_new();
      adw_preferences_row_set_use_markup(ADW_PREFERENCES_ROW(row), FALSE);
      adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), track->GetName().c_str());
      adw_action_row_set_subtitle(ADW_ACTION_ROW(row), FormatDistance(track->GetLengthMeters()).c_str());
      auto const c = track->GetColor(0);
      char rgb[16];
      std::snprintf(rgb, sizeof(rgb), "#%02x%02x%02x", c.GetRed(), c.GetGreen(), c.GetBlue());
      adw_action_row_add_prefix(ADW_ACTION_ROW(row), ColorDot(rgb, 14));
      gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), TRUE);
      Connect<void(AdwActionRow *)>(row, "activated", [this, tid](AdwActionRow *)
      {
        m_window.PopToMap();
        GetFramework().ShowTrack(tid);
      });
      adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), row);
    }
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(page), ADW_PREFERENCES_GROUP(group));
  }

  if (!bmIds.empty())
  {
    auto * group = adw_preferences_group_new();
    adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(group), T("bookmarks").c_str());
    std::vector<std::pair<std::string, kml::MarkId>> sorted;
    for (auto const mid : bmIds)
      if (auto const * b = bm.GetBookmark(mid))
        sorted.emplace_back(b->GetPreferredName(), mid);
    std::sort(sorted.begin(), sorted.end(), [](auto const & a, auto const & b)
    { return g_utf8_collate(a.first.c_str(), b.first.c_str()) < 0; });
    for (auto const & [name, mid] : sorted)
    {
      auto const * b = bm.GetBookmark(mid);
      auto * row = adw_action_row_new();
      adw_preferences_row_set_use_markup(ADW_PREFERENCES_ROW(row), FALSE);
      adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), name.c_str());
      auto const subtitle = BookmarkSubtitle(m_window, *b);
      if (!subtitle.empty())
        adw_action_row_set_subtitle(ADW_ACTION_ROW(row), subtitle.c_str());
      adw_action_row_add_prefix(ADW_ACTION_ROW(row), ColorDot(ColorRgb(b->GetColor())));
      gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), TRUE);
      Connect<void(AdwActionRow *)>(row, "activated", [this, mid](AdwActionRow *)
      {
        m_window.PopToMap();
        GetFramework().ShowBookmark(mid);
      });
      adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), row);
    }
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(page), ADW_PREFERENCES_GROUP(group));
  }
  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar), page);
}

void BookmarksPage::CreateList(std::function<void(kml::MarkGroupId)> onCreated)
{
  AskText(GTK_WIDGET(m_window.Window()), T("bookmarks_create_new_group"), T("bookmarks_new_list_hint"),
          [this, onCreated](std::string const & name)
  {
    auto & bm = GetFramework().GetBookmarkManager();
    if (name.empty())
    {
      m_window.Toast(T("bookmarks_error_message_empty_list_name"));
      return;
    }
    if (bm.IsUsedCategoryName(name))
    {
      m_window.Toast(T("bookmarks_error_title_list_name_already_taken"));
      return;
    }
    auto const id = bm.CreateBookmarkCategory(name);
    if (onCreated)
      onCreated(id);
  });
}

void BookmarksPage::RenameList(kml::MarkGroupId id, std::function<void()> onDone)
{
  auto & bm = GetFramework().GetBookmarkManager();
  AskText(GTK_WIDGET(m_window.Window()), T("bookmark_set_name"), bm.GetCategoryName(id),
          [id, onDone](std::string const & name)
  {
    if (name.empty())
      return;
    GetFramework().GetBookmarkManager().GetEditSession().SetCategoryName(id, name);
    onDone();
  });
}

void BookmarksPage::ChooseFileToImport()
{
  auto * dialog = gtk_file_dialog_new();
  gtk_file_dialog_set_title(dialog, T("bookmarks_import").c_str());
  auto * filter = gtk_file_filter_new();
  gtk_file_filter_set_name(filter, "KML, KMZ, GPX, GeoJSON");
  for (char const * p : {"*.kml", "*.kmz", "*.gpx", "*.geojson", "*.json", "*.kmb"})
    gtk_file_filter_add_pattern(filter, p);
  auto * filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
  g_list_store_append(filters, filter);
  gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
  g_object_unref(filters);
  g_object_unref(filter);
  gtk_file_dialog_open(dialog, m_window.Window(), nullptr, [](GObject * src, GAsyncResult * res, gpointer data)
  {
    GFile * file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(src), res, nullptr);
    if (!file)
      return;
    if (char * path = g_file_get_path(file))
    {
      static_cast<BookmarksPage *>(data)->ImportFile(path);
      g_free(path);
    }
    g_object_unref(file);
  }, this);
  g_object_unref(dialog);
}

void BookmarksPage::ImportFile(std::string const & path)
{
  GetFramework().AddBookmarksFile(path, false /* isTemporaryFile */);
  m_window.Toast(T("load_kmz_title") + "…");
}

void BookmarksPage::ToggleTrackRecording()
{
  auto & fw = GetFramework();
  if (!fw.IsTrackRecordingEnabled())
  {
    m_window.StartLocation();
    fw.StartTrackRecording();
    m_window.Toast(T("track_recording"));
    m_window.UpdateTrackRecordingState();
    return;
  }
  if (fw.IsTrackRecordingEmpty())
  {
    fw.StopTrackRecording();
    m_window.Toast(T("track_recording_toast_nothing_to_save"));
    m_window.UpdateTrackRecordingState();
    return;
  }
  AskText(GTK_WIDGET(m_window.Window()), T("track_recording_stop_and_save"),
          platform::GetLocalizedMyPositionBookmarkName(), [this](std::string const & name)
  {
    auto & fw = GetFramework();
    fw.SaveTrackRecordingWithName(name);
    fw.StopTrackRecording();
    m_window.UpdateTrackRecordingState();
    if (gtk_widget_get_mapped(m_listsToolbar))
      FillLists();
  });
}

void BookmarksPage::EditBookmark(MainWindow & window, kml::MarkId id)
{
  auto & bm = GetFramework().GetBookmarkManager();
  auto const * bookmark = bm.GetBookmark(id);
  if (!bookmark)
    return;

  struct State
  {
    kml::MarkId id;
    kml::PredefinedColor color;
    kml::MarkGroupId group;
    std::vector<kml::MarkGroupId> groups;
  };
  auto state = std::make_shared<State>();
  state->id = id;
  state->color = bookmark->GetColor();
  state->group = bookmark->GetGroupId();
  state->groups = bm.GetSortedBmGroupIdList();

  auto * dialog = adw_dialog_new();
  adw_dialog_set_title(dialog, T("placepage_edit_bookmark_button").c_str());
  adw_dialog_set_content_width(dialog, 420);
  auto * toolbar = adw_toolbar_view_new();
  auto * header = adw_header_bar_new();
  adw_header_bar_set_show_end_title_buttons(ADW_HEADER_BAR(header), FALSE);
  adw_header_bar_set_show_start_title_buttons(ADW_HEADER_BAR(header), FALSE);
  auto * cancel = gtk_button_new_with_label(T("cancel").c_str());
  OnClicked(cancel, [dialog]() { adw_dialog_close(dialog); });
  adw_header_bar_pack_start(ADW_HEADER_BAR(header), cancel);
  auto * save = gtk_button_new_with_label(T("save").c_str());
  gtk_widget_add_css_class(save, "suggested-action");
  adw_header_bar_pack_end(ADW_HEADER_BAR(header), save);
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar), header);

  auto * page = adw_preferences_page_new();
  auto * group = adw_preferences_group_new();
  auto * name = adw_entry_row_new();
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(name), T("placepage_bookmark_name_hint").c_str());
  gtk_editable_set_text(GTK_EDITABLE(name), bookmark->GetPreferredName().c_str());
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), name);

  // List selection.
  auto * names = gtk_string_list_new(nullptr);
  guint selected = 0;
  for (size_t i = 0; i < state->groups.size(); ++i)
  {
    gtk_string_list_append(names, bm.GetCategoryName(state->groups[i]).c_str());
    if (state->groups[i] == state->group)
      selected = static_cast<guint>(i);
  }
  auto * listRow = adw_combo_row_new();
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(listRow), T("list").c_str());
  adw_combo_row_set_model(ADW_COMBO_ROW(listRow), G_LIST_MODEL(names));
  adw_combo_row_set_selected(ADW_COMBO_ROW(listRow), selected);
  g_object_unref(names);
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), listRow);
  adw_preferences_page_add(ADW_PREFERENCES_PAGE(page), ADW_PREFERENCES_GROUP(group));

  // Colour palette.
  auto * colorGroup = adw_preferences_group_new();
  adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(colorGroup), T("choose_color").c_str());
  auto * flow = gtk_flow_box_new();
  gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flow), GTK_SELECTION_SINGLE);
  gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow), 8);
  gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(flow), 8);
  gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(flow), TRUE);
  for (auto const & ci : kColors)
  {
    auto * dot = ColorDot(ci.rgb, 30);
    gtk_widget_set_halign(dot, GTK_ALIGN_CENTER);
    gtk_widget_set_tooltip_text(dot, T(ci.key).c_str());
    gtk_flow_box_append(GTK_FLOW_BOX(flow), dot);
    if (ci.color == state->color)
      gtk_flow_box_select_child(GTK_FLOW_BOX(flow),
                                gtk_flow_box_get_child_at_index(GTK_FLOW_BOX(flow), static_cast<int>(&ci - kColors)));
  }
  Connect<void(GtkFlowBox *, GtkFlowBoxChild *)>(flow, "child-activated", [state](GtkFlowBox *, GtkFlowBoxChild * child)
  { state->color = kColors[gtk_flow_box_child_get_index(child)].color; });
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(colorGroup), flow);
  adw_preferences_page_add(ADW_PREFERENCES_PAGE(page), ADW_PREFERENCES_GROUP(colorGroup));

  // Notes.
  auto * notesGroup = adw_preferences_group_new();
  adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(notesGroup), T("description").c_str());
  auto * text = gtk_text_view_new();
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD_CHAR);
  gtk_widget_set_size_request(text, -1, 100);
  gtk_widget_add_css_class(text, "card");
  SetMarginAll(text, 0);
  gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text), 8);
  gtk_text_view_set_top_margin(GTK_TEXT_VIEW(text), 8);
  auto const desc = bookmark->GetDescription();
  gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(text)), desc.c_str(), -1);
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(notesGroup), text);
  adw_preferences_page_add(ADW_PREFERENCES_PAGE(page), ADW_PREFERENCES_GROUP(notesGroup));

  auto * deleteGroup = adw_preferences_group_new();
  auto * del = gtk_button_new_with_label(T("placepage_delete_bookmark_button").c_str());
  gtk_widget_add_css_class(del, "destructive-action");
  gtk_widget_add_css_class(del, "pill");
  gtk_widget_set_halign(del, GTK_ALIGN_CENTER);
  OnClicked(del, [dialog, id]()
  {
    auto & fw = GetFramework();
    fw.GetBookmarkManager().GetEditSession().DeleteBookmark(id);
    fw.DeactivateMapSelection();
    adw_dialog_close(dialog);
  });
  adw_preferences_group_add(ADW_PREFERENCES_GROUP(deleteGroup), del);
  adw_preferences_page_add(ADW_PREFERENCES_PAGE(page), ADW_PREFERENCES_GROUP(deleteGroup));

  OnClicked(save, [dialog, state, name, listRow, text]()
  {
    auto & fw = GetFramework();
    auto & bm = fw.GetBookmarkManager();
    auto const * b = bm.GetBookmark(state->id);
    if (!b)
      return;
    kml::BookmarkData data = b->GetData();
    kml::SetDefaultStr(data.m_customName, gtk_editable_get_text(GTK_EDITABLE(name)));
    data.m_color.m_predefinedColor = state->color;
    GtkTextIter start, end;
    auto * buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    char * notes = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    kml::SetDefaultStr(data.m_description, notes);
    g_free(notes);

    auto const newGroup = state->groups[adw_combo_row_get_selected(ADW_COMBO_ROW(listRow))];
    {
      auto session = bm.GetEditSession();
      session.UpdateBookmark(state->id, data);
      if (newGroup != b->GetGroupId())
        session.MoveBookmark(state->id, b->GetGroupId(), newGroup);
    }
    bm.SetLastEditedBmCategory(newGroup);
    bm.SetLastEditedBmColor(data.m_color.m_predefinedColor);

    if (fw.HasPlacePageInfo())
    {
      auto buildInfo = fw.GetCurrentPlacePageInfo().GetBuildInfo();
      buildInfo.m_match = place_page::BuildInfo::Match::Everything;
      buildInfo.m_userMarkId = state->id;
      fw.UpdatePlacePageInfoForCurrentSelection(buildInfo);
    }
    adw_dialog_close(dialog);
  });

  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar), page);
  adw_dialog_set_child(dialog, toolbar);
  adw_dialog_present(dialog, GTK_WIDGET(window.Window()));
}
}  // namespace lichen
