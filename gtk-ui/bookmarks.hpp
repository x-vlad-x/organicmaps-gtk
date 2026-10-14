#pragma once

#include "app.hpp"

#include "kml/type_utils.hpp"

#include <string>

namespace lichen
{
class MainWindow;

// Bookmarks and tracks: lists, list contents, bookmark editor, import and track recording.
class BookmarksPage
{
public:
  explicit BookmarksPage(MainWindow & window);
  ~BookmarksPage();

  void Show();
  void ImportFile(std::string const & path);
  void ToggleTrackRecording();

  static void EditBookmark(MainWindow & window, kml::MarkId id);

private:
  void FillLists();
  void ShowCategory(kml::MarkGroupId id);
  void FillCategory(kml::MarkGroupId id, GtkWidget * toolbar);
  void ChooseFileToImport();
  void CreateList(std::function<void(kml::MarkGroupId)> onCreated = {});
  void RenameList(kml::MarkGroupId id, std::function<void()> onDone);

  MainWindow & m_window;
  AdwNavigationPage * m_listsPage = nullptr;
  GtkWidget * m_listsToolbar = nullptr;
};

// Asks for a text (list name, track name) in a small dialog.
void AskText(GtkWidget * parent, std::string const & title, std::string const & initial,
             std::function<void(std::string const &)> onOk);
}  // namespace lichen
