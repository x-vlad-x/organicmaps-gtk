#pragma once

#include "app.hpp"

#include <string>

namespace omgtk
{
class MainWindow;

// The place page shown in the bottom sheet when an object is selected on the map.
class PlacePage
{
public:
  explicit PlacePage(MainWindow & window);

  GtkWidget * Widget() const { return m_root; }

  // Framework listeners.
  void Open();
  void Close();
  void Update();

  void OnSheetClosed();
  void OnLocationUpdate();

private:
  void Rebuild();
  void BuildActions();
  void BuildDetails();
  void UpdateDistance();
  GtkWidget * AddRow(char const * icon, std::string const & title, std::string const & subtitle = {},
                     std::function<void()> onActivate = {}, bool copyable = true);
  GtkWidget * AddAction(char const * icon, std::string const & label, std::function<void()> action,
                        bool active = false);

  void ToggleBookmark();
  void RouteFrom();
  void RouteTo();
  void AddStop();
  void RemoveStop();
  void Share();

  MainWindow & m_window;
  GtkWidget * m_root = nullptr;
  GtkWidget * m_title = nullptr;
  GtkWidget * m_secondaryTitle = nullptr;
  GtkWidget * m_subtitle = nullptr;
  GtkWidget * m_address = nullptr;
  GtkWidget * m_distance = nullptr;
  GtkWidget * m_hoursStatus = nullptr;
  GtkWidget * m_actions = nullptr;
  GtkWidget * m_scroller = nullptr;
  GtkWidget * m_details = nullptr;
  bool m_isOpen = false;
  size_t m_coordFormat = 0;
};
}  // namespace omgtk
