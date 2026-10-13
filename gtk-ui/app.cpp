#include "app.hpp"

#include "main_window.hpp"

#include "base/assert.hpp"

#include <algorithm>
#include <cctype>

namespace omgtk
{
App & GetApp()
{
  static App app;
  return app;
}

Framework & GetFramework()
{
  CHECK(GetApp().framework, ());
  return *GetApp().framework;
}

void RunOnUiThread(std::function<void()> fn)
{
  auto * heap = new std::function<void()>(std::move(fn));
  g_main_context_invoke_full(nullptr, G_PRIORITY_DEFAULT, [](gpointer data) -> gboolean
  {
    (*static_cast<std::function<void()> *>(data))();
    return G_SOURCE_REMOVE;
  }, heap, [](gpointer data) { delete static_cast<std::function<void()> *>(data); });
}

GtkWidget * MakeIconButton(char const * iconName, char const * tooltip, char const * cssClass)
{
  GtkWidget * b = gtk_button_new_from_icon_name(iconName);
  if (tooltip)
  {
    gtk_widget_set_tooltip_text(b, tooltip);
    gtk_accessible_update_property(GTK_ACCESSIBLE(b), GTK_ACCESSIBLE_PROPERTY_LABEL, tooltip, -1);
  }
  if (cssClass)
    gtk_widget_add_css_class(b, cssClass);
  return b;
}

void SetMarginAll(GtkWidget * w, int margin)
{
  gtk_widget_set_margin_top(w, margin);
  gtk_widget_set_margin_bottom(w, margin);
  gtk_widget_set_margin_start(w, margin);
  gtk_widget_set_margin_end(w, margin);
}

void ClearBox(GtkWidget * box)
{
  while (GtkWidget * child = gtk_widget_get_first_child(box))
  {
    if (GTK_IS_BOX(box))
      gtk_box_remove(GTK_BOX(box), child);
    else
      gtk_widget_unparent(child);
  }
}

void ClearListBox(GtkListBox * list)
{
  gtk_list_box_remove_all(list);
}

void ShowToast(std::string const & text)
{
  if (GetApp().window)
    GetApp().window->Toast(text);
}

void OpenUri(std::string const & uri)
{
  auto * launcher = gtk_uri_launcher_new(uri.c_str());
  GtkWindow * parent = GetApp().window ? GetApp().window->Window() : nullptr;
  gtk_uri_launcher_launch(launcher, parent, nullptr, nullptr, nullptr);
  g_object_unref(launcher);
}

std::string Trim(std::string s)
{
  auto const notSpace = [](unsigned char c) { return !std::isspace(c); };
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
  s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
  return s;
}

void CopyToClipboard(std::string const & text)
{
  GdkClipboard * cb = gdk_display_get_clipboard(gdk_display_get_default());
  gdk_clipboard_set_text(cb, text.c_str());
}
}  // namespace omgtk
