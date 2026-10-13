#pragma once

#include "app.hpp"

#include "search/result.hpp"

#include <cstdint>
#include <string>

namespace omgtk
{
class MainWindow;

// Search page (categories, history, results) and the query bar shown over the map when the results
// are displayed there.
class SearchUi
{
public:
  explicit SearchUi(MainWindow & window);

  void ShowSearchPage(std::string const & query = {});
  void ClearOnMapSearch();
  void Shutdown();
  void SelectResult(size_t index);

private:
  void BuildPage();
  void BuildMapBar();
  void OnQueryChanged();
  void RunSearch(std::string const & query, bool isCategory);
  void OnResults(uint64_t timestamp, search::Results && results);
  void FillHome();
  void FillResults();
  void ShowOnMap();
  std::string InputLocale() const;
  GtkWidget * MakeResultRow(search::Result const & r);

  MainWindow & m_window;
  AdwNavigationPage * m_page = nullptr;
  GtkWidget * m_entry = nullptr;
  GtkWidget * m_stack = nullptr;
  GtkWidget * m_homeList = nullptr;
  GtkWidget * m_resultsList = nullptr;
  GtkWidget * m_spinner = nullptr;
  GtkWidget * m_showOnMapButton = nullptr;
  GtkWidget * m_mapBar = nullptr;
  GtkWidget * m_mapBarLabel = nullptr;

  search::Results m_results;
  std::string m_query;
  bool m_isCategory = false;
  bool m_settingText = false;
  uint64_t m_timestamp = 0;
};
}  // namespace omgtk
