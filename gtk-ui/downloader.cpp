#include "downloader.hpp"

#include "l10n.hpp"
#include "main_window.hpp"
#include "style.hpp"

#include "map/framework.hpp"

#include "storage/downloader_search_params.hpp"
#include "storage/storage.hpp"

#include "defines.hpp"

#include "indexer/categories_holder.hpp"

#include <set>

namespace lichen
{
namespace
{
storage::Storage & GetStorage()
{
  return GetFramework().GetStorage();
}

std::string SubtitleFor(storage::CountryId const & id, storage::NodeAttrs const & attrs)
{
  auto & storage = GetStorage();
  using storage::NodeStatus;
  if (!storage.IsLeaf(id))
  {
    std::string s = T("downloader_status_maps") + ": " + std::to_string(attrs.m_mwmCounter);
    if (attrs.m_localMwmCounter > 0)
      s = TF("downloader_of", {std::to_string(attrs.m_localMwmCounter), std::to_string(attrs.m_mwmCounter)}) +
          " • " + FormatSize(attrs.m_localMwmSize);
    else
      s += " • " + FormatSize(attrs.m_mwmSize);
    return s;
  }
  switch (attrs.m_status)
  {
  case NodeStatus::OnDisk: return FormatSize(attrs.m_localMwmSize);
  case NodeStatus::OnDiskOutOfDate:
    return T("downloader_status_outdated") + " • " + FormatSize(attrs.m_mwmSize);
  default: return FormatSize(attrs.m_mwmSize);
  }
}
}  // namespace

DownloaderPage::DownloaderPage(MainWindow & window) : m_window(window) {}

DownloaderPage::~DownloaderPage()
{
  for (auto & p : m_pages)
    g_object_unref(p->m_page);
}

void DownloaderPage::Shutdown()
{
  GetFramework().GetSearchAPI().CancelAllSearches();
}

void DownloaderPage::Show(storage::CountryId const & rootArg)
{
  auto & storage = GetStorage();
  bool const isTop = rootArg.empty() || rootArg == storage.GetRootId();
  if (isTop)
  {
    for (auto & p : m_pages)
      g_object_unref(p->m_page);
    m_pages.clear();
  }

  auto page = std::make_unique<Page>();
  page->m_root = isTop ? storage.GetRootId() : rootArg;

  storage::NodeAttrs attrs;
  storage.GetNodeAttrs(page->m_root, attrs);
  std::string const title = isTop ? T("download_maps") : attrs.m_nodeLocalName;

  auto * toolbar = adw_toolbar_view_new();
  auto * header = adw_header_bar_new();
  page->m_updateButton = gtk_button_new_with_label(T("downloader_update_all_button").c_str());
  gtk_widget_add_css_class(page->m_updateButton, "suggested-action");
  auto * pagePtr = page.get();
  OnClicked(page->m_updateButton, [this, pagePtr]()
  {
    GetStorage().UpdateNode(pagePtr->m_root);
    UpdateHeader(*pagePtr);
  });
  adw_header_bar_pack_end(ADW_HEADER_BAR(header), page->m_updateButton);
  adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar), header);

  if (isTop)
  {
    page->m_searchEntry = gtk_search_entry_new();
    gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(page->m_searchEntry),
                                          T("downloader_search_field_hint").c_str());
    auto * clamp = adw_clamp_new();
    adw_clamp_set_child(ADW_CLAMP(clamp), page->m_searchEntry);
    SetMarginAll(page->m_searchEntry, 6);
    adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar), clamp);
    Connect<void(GtkSearchEntry *)>(page->m_searchEntry, "search-changed", [this, pagePtr](GtkSearchEntry * e)
    { RunSearch(*pagePtr, gtk_editable_get_text(GTK_EDITABLE(e))); });
  }

  page->m_content = adw_preferences_page_new();
  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar), page->m_content);

  page->m_page = adw_navigation_page_new(toolbar, title.c_str());
  g_object_ref_sink(page->m_page);
  Fill(*page);
  UpdateHeader(*page);
  m_window.PushPage(page->m_page);
  m_pages.push_back(std::move(page));
}

void DownloaderPage::Fill(Page & page)
{
  auto & storage = GetStorage();
  page.m_rows.clear();
  // AdwPreferencesPage has no "remove all": rebuild the page content.
  auto * content = adw_preferences_page_new();
  auto * toolbar = gtk_widget_get_ancestor(page.m_content, ADW_TYPE_TOOLBAR_VIEW);
  page.m_content = content;
  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar), content);

  storage::CountriesVec downloaded, available;
  storage.GetChildrenInGroups(page.m_root, downloaded, available);
  if (page.m_root == storage.GetRootId())
  {
    if (!downloaded.empty())
      adw_preferences_page_add(ADW_PREFERENCES_PAGE(content),
                               ADW_PREFERENCES_GROUP(MakeGroup(page, T("downloader_downloaded_subtitle"), downloaded)));
    storage::CountriesVec all;
    storage.GetChildren(page.m_root, all);
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(content),
                             ADW_PREFERENCES_GROUP(MakeGroup(page, T("downloader_available_maps"), all)));
  }
  else
  {
    storage::CountriesVec all;
    storage.GetChildren(page.m_root, all);
    adw_preferences_page_add(ADW_PREFERENCES_PAGE(content), ADW_PREFERENCES_GROUP(MakeGroup(page, {}, all)));
  }
}

void DownloaderPage::FillSearch(Page & page, std::vector<storage::CountryId> const & ids)
{
  page.m_rows.clear();
  auto * content = adw_preferences_page_new();
  auto * toolbar = gtk_widget_get_ancestor(page.m_content, ADW_TYPE_TOOLBAR_VIEW);
  page.m_content = content;
  adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar), content);
  if (ids.empty())
  {
    auto * status = adw_status_page_new();
    adw_status_page_set_title(ADW_STATUS_PAGE(status), T("search_not_found").c_str());
    adw_status_page_set_icon_name(ADW_STATUS_PAGE(status), "system-search-symbolic");
    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar), status);
    page.m_content = status;
    return;
  }
  adw_preferences_page_add(ADW_PREFERENCES_PAGE(content),
                           ADW_PREFERENCES_GROUP(MakeGroup(page, T("downloader_search_results"), ids)));
}

GtkWidget * DownloaderPage::MakeGroup(Page & page, std::string const & title, std::vector<storage::CountryId> const & ids)
{
  auto * group = adw_preferences_group_new();
  if (!title.empty())
    adw_preferences_group_set_title(ADW_PREFERENCES_GROUP(group), title.c_str());

  // Sort by the localized name.
  std::vector<std::pair<std::string, storage::CountryId>> sorted;
  auto & storage = GetStorage();
  for (auto const & id : ids)
  {
    // World maps come with the app (as on Android, they are not shown).
    if (id == WORLD_FILE_NAME || id == WORLD_COASTS_FILE_NAME)
      continue;
    storage::NodeAttrs attrs;
    storage.GetNodeAttrs(id, attrs);
    sorted.emplace_back(attrs.m_nodeLocalName, id);
  }
  std::sort(sorted.begin(), sorted.end(), [](auto const & a, auto const & b)
  { return g_utf8_collate(a.first.c_str(), b.first.c_str()) < 0; });

  for (auto const & [name, id] : sorted)
    adw_preferences_group_add(ADW_PREFERENCES_GROUP(group), MakeRow(page, id));
  return group;
}

GtkWidget * DownloaderPage::MakeRow(Page & page, storage::CountryId const & id)
{
  auto & storage = GetStorage();
  storage::NodeAttrs attrs;
  storage.GetNodeAttrs(id, attrs);

  auto * row = adw_action_row_new();
  adw_preferences_row_set_use_markup(ADW_PREFERENCES_ROW(row), FALSE);
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), attrs.m_nodeLocalName.c_str());
  std::string parents;
  if (page.m_searchEntry && *gtk_editable_get_text(GTK_EDITABLE(page.m_searchEntry)))
    for (auto const & p : attrs.m_topmostParentInfo)
      parents += (parents.empty() ? "" : ", ") + p.m_localName;
  gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), TRUE);

  auto * status = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_set_valign(status, GTK_ALIGN_CENTER);
  adw_action_row_add_suffix(ADW_ACTION_ROW(row), status);
  bool const leaf = storage.IsLeaf(id);
  if (!leaf)
    adw_action_row_add_suffix(ADW_ACTION_ROW(row), gtk_image_new_from_icon_name("go-next-symbolic"));

  Connect<void(AdwActionRow *)>(row, "activated", [this, id, leaf](AdwActionRow *)
  {
    if (leaf)
      ShowActions(id);
    else
      Show(id);
  });

  page.m_rows.push_back({id, row, status, parents});
  UpdateRowStatus(page.m_rows.back());
  return row;
}

void DownloaderPage::UpdateRowStatus(Row & row)
{
  auto & storage = GetStorage();
  storage::NodeAttrs attrs;
  storage.GetNodeAttrs(row.m_id, attrs);
  auto subtitle = SubtitleFor(row.m_id, attrs);
  if (!row.m_parents.empty())
    subtitle = row.m_parents + " • " + subtitle;
  adw_action_row_set_subtitle(ADW_ACTION_ROW(row.m_row), subtitle.c_str());

  ClearBox(row.m_status);
  auto const id = row.m_id;
  auto const addButton = [&](char const * icon, char const * label, std::function<void()> fn)
  {
    GtkWidget * b = label ? gtk_button_new_with_label(label) : gtk_button_new_from_icon_name(icon);
    gtk_widget_add_css_class(b, "flat");
    if (!label)
      gtk_widget_add_css_class(b, "circular");
    gtk_widget_set_valign(b, GTK_ALIGN_CENTER);
    OnClicked(b, std::move(fn));
    gtk_box_append(GTK_BOX(row.m_status), b);
  };

  using storage::NodeStatus;
  switch (attrs.m_status)
  {
  case NodeStatus::NotDownloaded:
  case NodeStatus::Partly:
    addButton("folder-download-symbolic", nullptr, [id]() { GetStorage().DownloadNode(id); });
    break;
  case NodeStatus::Downloading:
  case NodeStatus::Applying:
  {
    auto const & p = attrs.m_downloadingProgress;
    int const percent = p.m_bytesTotal > 0 ? static_cast<int>(100.0 * p.m_bytesDownloaded / p.m_bytesTotal) : 0;
    auto * l = gtk_label_new((std::to_string(percent) + "%").c_str());
    gtk_widget_add_css_class(l, "numeric");
    gtk_box_append(GTK_BOX(row.m_status), l);
    addButton("window-close-symbolic", nullptr, [id]() { GetStorage().CancelDownloadNode(id); });
    break;
  }
  case NodeStatus::InQueue:
  {
    auto * l = gtk_label_new(T("downloader_queued").c_str());
    gtk_widget_add_css_class(l, "dim-label");
    gtk_box_append(GTK_BOX(row.m_status), l);
    addButton("window-close-symbolic", nullptr, [id]() { GetStorage().CancelDownloadNode(id); });
    break;
  }
  case NodeStatus::OnDisk:
    gtk_box_append(GTK_BOX(row.m_status), gtk_image_new_from_icon_name("object-select-symbolic"));
    break;
  case NodeStatus::OnDiskOutOfDate:
    addButton(nullptr, T("downloader_status_outdated").c_str(), [id]() { GetStorage().UpdateNode(id); });
    break;
  case NodeStatus::Error:
    addButton(nullptr, T("downloader_retry").c_str(), [id]() { GetStorage().RetryDownloadNode(id); });
    break;
  case NodeStatus::Undefined: break;
  }
}

void DownloaderPage::UpdateHeader(Page & page)
{
  storage::Storage::UpdateInfo info;
  bool const hasUpdates = GetStorage().GetUpdateInfo(page.m_root, info) && info.m_numberOfMwmFilesToUpdate > 0;
  if (hasUpdates)
    gtk_button_set_label(GTK_BUTTON(page.m_updateButton),
                         (T("downloader_update_all_button") + " (" + FormatSize(info.m_totalDownloadSizeInBytes) + ")").c_str());
  gtk_widget_set_visible(page.m_updateButton, hasUpdates);
}

void DownloaderPage::ShowActions(storage::CountryId const & id)
{
  auto & storage = GetStorage();
  storage::NodeAttrs attrs;
  storage.GetNodeAttrs(id, attrs);

  auto * dialog = adw_alert_dialog_new(attrs.m_nodeLocalName.c_str(), SubtitleFor(id, attrs).c_str());
  using storage::NodeStatus;
  bool const onDisk = attrs.m_status == NodeStatus::OnDisk || attrs.m_status == NodeStatus::OnDiskOutOfDate;
  adw_alert_dialog_add_response(ADW_ALERT_DIALOG(dialog), "cancel", T("cancel").c_str());
  if (onDisk)
  {
    adw_alert_dialog_add_response(ADW_ALERT_DIALOG(dialog), "delete", T("downloader_delete_map").c_str());
    adw_alert_dialog_set_response_appearance(ADW_ALERT_DIALOG(dialog), "delete", ADW_RESPONSE_DESTRUCTIVE);
  }
  if (attrs.m_status == NodeStatus::OnDiskOutOfDate)
    adw_alert_dialog_add_response(ADW_ALERT_DIALOG(dialog), "update", T("downloader_update_map").c_str());
  if (attrs.m_status == NodeStatus::NotDownloaded || attrs.m_status == NodeStatus::Error)
  {
    adw_alert_dialog_add_response(ADW_ALERT_DIALOG(dialog), "download", T("downloader_download_map").c_str());
    adw_alert_dialog_set_response_appearance(ADW_ALERT_DIALOG(dialog), "download", ADW_RESPONSE_SUGGESTED);
  }
  if (attrs.m_status == NodeStatus::Downloading || attrs.m_status == NodeStatus::InQueue)
    adw_alert_dialog_add_response(ADW_ALERT_DIALOG(dialog), "stop", T("cancel_download").c_str());
  adw_alert_dialog_add_response(ADW_ALERT_DIALOG(dialog), "show", T("zoom_to_country").c_str());
  adw_alert_dialog_set_prefer_wide_layout(ADW_ALERT_DIALOG(dialog), FALSE);

  Connect<void(AdwAlertDialog *, char const *)>(dialog, "response", [this, id](AdwAlertDialog *, char const * r)
  {
    std::string_view const resp = r;
    auto & storage = GetStorage();
    if (resp == "delete")
    {
      auto & rm = GetFramework().GetRoutingManager();
      if (rm.IsRoutingActive())
        rm.CloseRouting(false);
      storage.DeleteNode(id);
    }
    else if (resp == "update")
      storage.UpdateNode(id);
    else if (resp == "download")
      storage.DownloadNode(id);
    else if (resp == "stop")
      storage.CancelDownloadNode(id);
    else if (resp == "show")
    {
      m_window.PopToMap();
      GetFramework().ShowNode(id);
    }
  });
  adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(m_window.Window()));
}

void DownloaderPage::RunSearch(Page & page, std::string const & query)
{
  auto const ts = ++m_searchTimestamp;
  if (query.empty())
  {
    GetFramework().GetSearchAPI().CancelSearch(search::Mode::Downloader);
    Fill(page);
    return;
  }
  storage::DownloaderSearchParams params;
  params.m_query = query;
  auto locale = CurrentLanguage();
  if (CategoriesHolder::MapLocaleToInteger(locale) < 0)
    locale = "en";
  params.m_inputLocale = locale;
  auto * pagePtr = &page;
  params.m_onResults = [this, pagePtr, ts](storage::DownloaderSearchResults results)
  {
    if (ts != m_searchTimestamp)
      return;
    std::vector<storage::CountryId> ids;
    std::set<storage::CountryId> seen;
    for (auto const & r : results.m_results)
      if (seen.insert(r.m_countryId).second)
        ids.push_back(r.m_countryId);
    FillSearch(*pagePtr, ids);
  };
  GetFramework().GetSearchAPI().SearchInDownloader(std::move(params));
}

void DownloaderPage::OnCountryChanged(storage::CountryId const & id)
{
  auto & storage = GetStorage();
  for (auto & page : m_pages)
  {
    for (auto & row : page->m_rows)
    {
      // A change of a leaf affects all its parents' rows too.
      if (row.m_id == id || !storage.IsLeaf(row.m_id))
        UpdateRowStatus(row);
    }
    UpdateHeader(*page);
  }
}

void DownloaderPage::OnProgress(storage::CountryId const & id, downloader::Progress const &)
{
  OnCountryChanged(id);
}
}  // namespace lichen
