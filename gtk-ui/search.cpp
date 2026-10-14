#include "search.hpp"

#include "l10n.hpp"
#include "main_window.hpp"
#include "style.hpp"

#include "map/everywhere_search_params.hpp"
#include "map/framework.hpp"
#include "map/viewport_search_params.hpp"

#include "search/displayed_categories.hpp"

#include "indexer/categories_holder.hpp"

#include "geometry/distance_on_sphere.hpp"
#include "geometry/mercator.hpp"

#include "base/string_utils.hpp"

#include <glib/gmarkup.h>

namespace lichen
{
namespace
{
std::string Escape(std::string const & s)
{
  char * e = g_markup_escape_text(s.c_str(), -1);
  std::string r(e);
  g_free(e);
  return r;
}

// Highlighted ranges are in UTF-16 code units of the result string.
std::string HighlightMarkup(search::Result const & r)
{
  strings::UniString const str = strings::MakeUniString(r.GetString());
  std::string out;
  size_t pos = 0;
  for (size_t i = 0; i < r.GetHighlightRangesCount(); ++i)
  {
    auto const & range = r.GetHighlightRange(i);
    if (range.first < pos || range.first + range.second > str.size())
      continue;
    out += Escape(strings::ToUtf8(strings::UniString(str.begin() + pos, str.begin() + range.first)));
    out += "<b>" +
           Escape(strings::ToUtf8(
               strings::UniString(str.begin() + range.first, str.begin() + range.first + range.second))) +
           "</b>";
    pos = range.first + range.second;
  }
  out += Escape(strings::ToUtf8(strings::UniString(str.begin() + pos, str.end())));
  return out;
}
}  // namespace

SearchUi::SearchUi(MainWindow & window) : m_window(window)
{
  BuildPage();
  BuildMapBar();
}

void SearchUi::Shutdown()
{
  GetFramework().GetSearchAPI().CancelAllSearches();
}

std::string SearchUi::InputLocale() const
{
  auto const & lang = CurrentLanguage();
  if (CategoriesHolder::MapLocaleToInteger(lang) < 0)
    return "en";
  return lang;
}

void SearchUi::BuildPage()
{
  auto * toolbar = adw_toolbar_view_new();
  auto * header = adw_header_bar_new();

  m_entry = gtk_search_entry_new();
  gtk_widget_set_hexpand(m_entry, TRUE);
  gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(m_entry), T("search_map").c_str());
  gtk_search_entry_set_search_delay(GTK_SEARCH_ENTRY(m_entry), 120);
  adw_header_bar_set_title_widget(ADW_HEADER_BAR(header), m_entry);
  Connect<void(GtkSearchEntry *)>(m_entry, "search-changed", [this](GtkSearchEntry *) { OnQueryChanged(); });
  Connect<void(GtkSearchEntry *)>(m_entry, "activate", [this](GtkSearchEntry *) { ShowOnMap(); });

  m_showOnMapButton = MakeIconButton("mark-location-symbolic", T("search_show_on_map").c_str());
  OnClicked(m_showOnMapButton, [this]() { ShowOnMap(); });
  gtk_widget_set_sensitive(m_showOnMapButton, FALSE);
  adw_header_bar_pack_end(ADW_HEADER_BAR(header), m_showOnMapButton);
  m_spinner = gtk_spinner_new();
  adw_header_bar_pack_end(ADW_HEADER_BAR(header), m_spinner);
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar), header);

  m_stack = gtk_stack_new();

  auto const makeList = [](GtkWidget *& list)
  {
    list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_NONE);
    gtk_widget_add_css_class(list, "navigation-sidebar");
    auto * scroller = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), list);
    gtk_widget_set_vexpand(scroller, TRUE);
    return scroller;
  };
  gtk_stack_add_named(GTK_STACK(m_stack), makeList(m_homeList), "home");
  gtk_stack_add_named(GTK_STACK(m_stack), makeList(m_resultsList), "results");

  auto * empty = adw_status_page_new();
  adw_status_page_set_icon_name(ADW_STATUS_PAGE(empty), "system-search-symbolic");
  adw_status_page_set_title(ADW_STATUS_PAGE(empty), T("search_not_found").c_str());
  adw_status_page_set_description(ADW_STATUS_PAGE(empty), T("search_not_found_query").c_str());
  gtk_stack_add_named(GTK_STACK(m_stack), empty, "empty");

  Connect<void(GtkListBox *, GtkListBoxRow *)>(m_resultsList, "row-activated", [this](GtkListBox *, GtkListBoxRow * row)
  { SelectResult(static_cast<size_t>(gtk_list_box_row_get_index(row))); });

  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar), m_stack);
  m_page = adw_navigation_page_new(toolbar, T("search").c_str());
  adw_navigation_page_set_tag(m_page, "search");
  g_object_ref_sink(m_page);
  Connect<void(AdwNavigationPage *)>(m_page, "shown", [this](AdwNavigationPage *) { gtk_widget_grab_focus(m_entry); });
}

void SearchUi::BuildMapBar()
{
  m_mapBar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_add_css_class(m_mapBar, "map-top-panel");
  gtk_widget_add_css_class(m_mapBar, "search-on-map-bar");

  auto * back = MakeIconButton("go-previous-symbolic", T("back").c_str(), "flat");
  OnClicked(back, [this]() { ShowSearchPage(m_query); });
  gtk_box_append(GTK_BOX(m_mapBar), back);

  auto * openSearch = gtk_button_new();
  gtk_widget_add_css_class(openSearch, "flat");
  gtk_widget_set_hexpand(openSearch, TRUE);
  m_mapBarLabel = gtk_label_new("");
  gtk_label_set_xalign(GTK_LABEL(m_mapBarLabel), 0);
  gtk_label_set_ellipsize(GTK_LABEL(m_mapBarLabel), PANGO_ELLIPSIZE_END);
  gtk_button_set_child(GTK_BUTTON(openSearch), m_mapBarLabel);
  OnClicked(openSearch, [this]() { ShowSearchPage(m_query); });
  gtk_box_append(GTK_BOX(m_mapBar), openSearch);

  auto * close = MakeIconButton("window-close-symbolic", T("close").c_str(), "flat");
  OnClicked(close, [this]() { ClearOnMapSearch(); });
  gtk_box_append(GTK_BOX(m_mapBar), close);

  gtk_widget_set_visible(m_mapBar, FALSE);
  gtk_box_append(GTK_BOX(m_window.TopSlot()), m_mapBar);
}

void SearchUi::ShowSearchPage(std::string const & query)
{
  if (m_window.GetMode() == MainWindow::Mode::Navigation)
    return;
  m_window.PushPage(m_page);
  m_settingText = true;
  gtk_editable_set_text(GTK_EDITABLE(m_entry), query.c_str());
  m_settingText = false;
  if (query.empty())
  {
    m_query.clear();
    FillHome();
    gtk_stack_set_visible_child_name(GTK_STACK(m_stack), "home");
  }
  else
    FillResults();
  gtk_editable_set_position(GTK_EDITABLE(m_entry), -1);
}

void SearchUi::FillHome()
{
  ClearListBox(GTK_LIST_BOX(m_homeList));
  auto & fw = GetFramework();

  auto const addRow = [this](GtkWidget * prefix, std::string const & title, std::function<void()> action)
  {
    auto * row = adw_action_row_new();
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), title.c_str());
    adw_preferences_row_set_use_markup(ADW_PREFERENCES_ROW(row), FALSE);
    if (prefix)
      adw_action_row_add_prefix(ADW_ACTION_ROW(row), prefix);
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), TRUE);
    gtk_widget_add_css_class(row, "search-category-row");
    Connect<void(AdwActionRow *)>(row, "activated", [action](AdwActionRow *) { action(); });
    gtk_list_box_append(GTK_LIST_BOX(m_homeList), row);
  };

  auto const & history = fw.GetSearchAPI().GetLastSearchQueries();
  for (auto const & q : history)
  {
    std::string const query = q.second;
    addRow(gtk_image_new_from_icon_name("document-open-recent-symbolic"), query, [this, query]()
    {
      gtk_editable_set_text(GTK_EDITABLE(m_entry), query.c_str());
    });
  }

  for (auto const & key : fw.GetDisplayedCategories().GetKeys())
  {
    std::string const name = T(key);
    auto * icon = gtk_image_new_from_icon_name(("om-" + std::string(key).replace(0, 9, "category-")).c_str());
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 32);
    addRow(icon, name, [this, name]()
    {
      // Category search, like tapping a category on Android: show the results on the map.
      m_settingText = true;
      gtk_editable_set_text(GTK_EDITABLE(m_entry), (name + " ").c_str());
      m_settingText = false;
      RunSearch(name + " ", true);
      ShowOnMap();
    });
  }
}

void SearchUi::OnQueryChanged()
{
  if (m_settingText)
    return;
  std::string const query = gtk_editable_get_text(GTK_EDITABLE(m_entry));
  if (Trim(query).empty())
  {
    m_query.clear();
    m_results.Clear();
    GetFramework().GetSearchAPI().CancelAllSearches();
    gtk_spinner_stop(GTK_SPINNER(m_spinner));
    gtk_widget_set_sensitive(m_showOnMapButton, FALSE);
    FillHome();
    gtk_stack_set_visible_child_name(GTK_STACK(m_stack), "home");
    return;
  }
  RunSearch(query, false);
}

void SearchUi::RunSearch(std::string const & query, bool isCategory)
{
  m_query = query;
  m_isCategory = isCategory;
  auto const timestamp = ++m_timestamp;
  search::EverywhereSearchParams params;
  params.m_query = query;
  params.m_inputLocale = InputLocale();
  params.m_isCategory = isCategory;
  params.m_onResults = [this, timestamp](search::Results results)
  {
    // Results arrive on the GUI thread already.
    OnResults(timestamp, std::move(results));
  };
  if (GetFramework().GetSearchAPI().SearchEverywhere(std::move(params)))
    gtk_spinner_start(GTK_SPINNER(m_spinner));
  gtk_widget_set_sensitive(m_showOnMapButton, TRUE);
}

void SearchUi::OnResults(uint64_t timestamp, search::Results && results)
{
  if (timestamp != m_timestamp)
    return;
  m_results = std::move(results);
  if (m_results.IsEndMarker())
    gtk_spinner_stop(GTK_SPINNER(m_spinner));
  FillResults();
}

GtkWidget * SearchUi::MakeResultRow(search::Result const & r)
{
  auto * row = gtk_list_box_row_new();
  auto * box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
  SetMarginAll(box, 8);

  auto * texts = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  gtk_widget_set_hexpand(texts, TRUE);
  auto * title = gtk_label_new(nullptr);
  gtk_label_set_markup(GTK_LABEL(title), HighlightMarkup(r).c_str());
  gtk_label_set_xalign(GTK_LABEL(title), 0);
  gtk_label_set_wrap(GTK_LABEL(title), TRUE);
  gtk_box_append(GTK_BOX(texts), title);

  if (r.IsSuggest())
  {
    gtk_box_prepend(GTK_BOX(box), gtk_image_new_from_icon_name("system-search-symbolic"));
  }
  else
  {
    std::string type = r.GetLocalizedFeatureType();
    auto desc = r.GetFeatureDescription(type);
    if (!desc.empty())
    {
      auto * l = gtk_label_new(desc.c_str());
      gtk_label_set_xalign(GTK_LABEL(l), 0);
      gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
      gtk_widget_add_css_class(l, "search-result-type");
      gtk_box_append(GTK_BOX(texts), l);
    }
    if (!r.GetAddress().empty())
    {
      auto * l = gtk_label_new(r.GetAddress().c_str());
      gtk_label_set_xalign(GTK_LABEL(l), 0);
      gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
      gtk_widget_add_css_class(l, "dim-label");
      gtk_widget_add_css_class(l, "caption");
      gtk_box_append(GTK_BOX(texts), l);
    }
    if (r.IsOpenNow() != osm::Unknown)
    {
      bool const open = r.IsOpenNow() == osm::Yes;
      auto * l = gtk_label_new((open ? T("editor_time_open") : T("closed_now")).c_str());
      gtk_label_set_xalign(GTK_LABEL(l), 0);
      gtk_widget_add_css_class(l, open ? "pp-status-open" : "pp-status-closed");
      gtk_widget_add_css_class(l, "caption");
      gtk_box_append(GTK_BOX(texts), l);
    }
  }
  gtk_box_append(GTK_BOX(box), texts);

  if (!r.IsSuggest() && r.HasPoint())
  {
    if (auto const & last = m_window.Location().LastLocation())
    {
      auto const dist = ms::DistanceOnEarth(last->GetLatLon(), mercator::ToLatLon(r.GetFeatureCenter()));
      auto * l = gtk_label_new(FormatDistance(dist).c_str());
      gtk_widget_add_css_class(l, "dim-label");
      gtk_widget_set_valign(l, GTK_ALIGN_START);
      gtk_box_append(GTK_BOX(box), l);
    }
  }
  gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
  return row;
}

void SearchUi::FillResults()
{
  ClearListBox(GTK_LIST_BOX(m_resultsList));
  for (auto const & r : m_results)
    gtk_list_box_append(GTK_LIST_BOX(m_resultsList), MakeResultRow(r));
  if (m_results.GetCount() == 0)
    gtk_stack_set_visible_child_name(GTK_STACK(m_stack), m_results.IsEndMarker() ? "empty" : "results");
  else
    gtk_stack_set_visible_child_name(GTK_STACK(m_stack), "results");
}

void SearchUi::SelectResult(size_t index)
{
  if (index >= m_results.GetCount())
    return;
  auto const & r = m_results[index];
  if (r.IsSuggest())
  {
    gtk_editable_set_text(GTK_EDITABLE(m_entry), r.GetSuggestionString().c_str());
    gtk_editable_set_position(GTK_EDITABLE(m_entry), -1);
    return;
  }
  auto & fw = GetFramework();
  fw.GetSearchAPI().SaveSearchQuery({InputLocale(), m_query});
  fw.FillSearchResultsMarks(true /* clear */, m_results);
  m_window.PopToMap();
  m_window.SetMode(MainWindow::Mode::SearchOnMap);
  gtk_label_set_text(GTK_LABEL(m_mapBarLabel), m_query.c_str());
  gtk_widget_set_visible(m_mapBar, TRUE);
  fw.SelectSearchResult(r, true /* animation */);
}

void SearchUi::ShowOnMap()
{
  if (m_query.empty())
    return;
  auto & fw = GetFramework();
  fw.GetSearchAPI().SaveSearchQuery({InputLocale(), m_query});

  search::ViewportSearchParams params;
  params.m_query = m_query;
  params.m_inputLocale = InputLocale();
  params.m_isCategory = m_isCategory;
  params.m_onCompleted = [](search::Results results)
  { GetFramework().FillSearchResultsMarks(true /* clear */, results); };
  fw.GetSearchAPI().SearchInViewport(std::move(params));

  if (m_results.GetCount() > 0)
    fw.UpdateViewport(m_results);

  m_window.PopToMap();
  m_window.SetMode(MainWindow::Mode::SearchOnMap);
  gtk_label_set_text(GTK_LABEL(m_mapBarLabel), m_query.c_str());
  gtk_widget_set_visible(m_mapBar, TRUE);
}

void SearchUi::ClearOnMapSearch()
{
  auto & fw = GetFramework();
  fw.GetSearchAPI().CancelAllSearches();
  fw.ClearViewportSearchResults();
  fw.DeactivateMapSelection();
  gtk_widget_set_visible(m_mapBar, FALSE);
  m_window.SetMode(MainWindow::Mode::Default);
}
}  // namespace lichen
