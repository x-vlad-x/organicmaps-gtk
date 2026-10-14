#pragma once

#include "app.hpp"

#include "storage/storage_defines.hpp"

#include "platform/downloader_defines.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace lichen
{
class MainWindow;

// Map downloader: a navigable tree of countries and regions, like "Download maps" on Android.
class DownloaderPage
{
public:
  explicit DownloaderPage(MainWindow & window);
  ~DownloaderPage();

  void Show(storage::CountryId const & root = {});
  void Shutdown();

  void OnCountryChanged(storage::CountryId const & id);
  void OnProgress(storage::CountryId const & id, downloader::Progress const & progress);

private:
  struct Row
  {
    storage::CountryId m_id;
    GtkWidget * m_row = nullptr;
    GtkWidget * m_status = nullptr;  // box with the action button / progress
    std::string m_parents;           // shown for search results
  };
  struct Page
  {
    storage::CountryId m_root;
    AdwNavigationPage * m_page = nullptr;
    GtkWidget * m_content = nullptr;
    GtkWidget * m_updateButton = nullptr;
    GtkWidget * m_searchEntry = nullptr;
    std::vector<Row> m_rows;
  };

  void Fill(Page & page);
  void FillSearch(Page & page, std::vector<storage::CountryId> const & ids);
  GtkWidget * MakeGroup(Page & page, std::string const & title, std::vector<storage::CountryId> const & ids);
  GtkWidget * MakeRow(Page & page, storage::CountryId const & id);
  void UpdateRowStatus(Row & row);
  void UpdateHeader(Page & page);
  void ShowActions(storage::CountryId const & id);
  void RunSearch(Page & page, std::string const & query);

  MainWindow & m_window;
  std::vector<std::unique_ptr<Page>> m_pages;
  uint64_t m_searchTimestamp = 0;
};
}  // namespace lichen
