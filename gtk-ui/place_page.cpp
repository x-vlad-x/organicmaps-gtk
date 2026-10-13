#include "place_page.hpp"

#include "bookmarks.hpp"
#include "elevation_chart.hpp"
#include "l10n.hpp"
#include "main_window.hpp"
#include "routing.hpp"
#include "style.hpp"

#include "map/bookmark_helpers.hpp"
#include "map/framework.hpp"
#include "map/place_page_info.hpp"
#include "map/routing_manager.hpp"

#include "editor/opening_hours_ui.hpp"
#include "editor/ui2oh.hpp"

#include "indexer/validate_and_format_contacts.hpp"

#include "platform/distance.hpp"

#include "geometry/mercator.hpp"

#include "base/string_utils.hpp"

#include <opening_hours/opening_hours.hpp>

#include <ctime>

namespace omgtk
{
namespace
{
std::string TimeOfDay(time_t t)
{
  char buf[16] = {};
  std::strftime(buf, sizeof(buf), "%H:%M", std::localtime(&t));
  return buf;
}

std::string WeekdayName(osmoh::Weekday wd)
{
  std::tm tm{};
  tm.tm_wday = static_cast<int>(wd) - 1;  // osmoh: Sunday == 1
  char buf[32] = {};
  std::strftime(buf, sizeof(buf), "%a", &tm);
  return buf;
}

std::string FormatSpan(osmoh::Timespan const & span)
{
  auto const fmt = [](osmoh::Time const & t)
  {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", static_cast<int>(t.GetHoursCount()),
                  static_cast<int>(t.GetMinutesCount()));
    return std::string(buf);
  };
  return fmt(span.GetStart()) + "–" + fmt(span.GetEnd());
}

// Weekdays as ranges in Monday-first order: "Mon–Fri, Sun".
std::string FormatDays(editor::ui::OpeningDays const & days)
{
  static osmoh::Weekday const order[] = {osmoh::Weekday::Monday,   osmoh::Weekday::Tuesday, osmoh::Weekday::Wednesday,
                                         osmoh::Weekday::Thursday, osmoh::Weekday::Friday,  osmoh::Weekday::Saturday,
                                         osmoh::Weekday::Sunday};
  if (days.size() == 7)
    return T("daily");
  std::string out;
  for (size_t i = 0; i < 7;)
  {
    if (!days.count(order[i]))
    {
      ++i;
      continue;
    }
    size_t j = i;
    while (j + 1 < 7 && days.count(order[j + 1]))
      ++j;
    if (!out.empty())
      out += ", ";
    out += WeekdayName(order[i]);
    if (j > i)
      out += (j == i + 1 ? ", " : "–") + WeekdayName(order[j]);
    i = j + 1;
  }
  return out;
}

// Human readable schedule, one line per timetable, or the raw OSM value when it can't be simplified.
std::string FormatSchedule(std::string_view raw)
{
  osmoh::OpeningHours oh(raw);
  editor::ui::TimeTableSet tts;
  if (!oh.IsValid() || !editor::MakeTimeTableSet(oh, tts))
    return std::string(raw);
  if (tts.IsTwentyFourPerSeven())
    return T("twentyfour_seven");
  std::string out;
  for (auto const & tt : tts)
  {
    if (!out.empty())
      out += "\n";
    out += FormatDays(tt.GetOpeningDays()) + ": ";
    if (tt.IsTwentyFourHours())
      out += T("editor_time_allday");
    else
      out += FormatSpan(tt.GetOpeningTime());
    for (auto const & ex : tt.GetExcludeTime())
      out += " (" + T("editor_hours_closed") + " " + FormatSpan(ex) + ")";
  }
  auto const closed = tts.GetUnhandledDays();
  if (!closed.empty())
    out += "\n" + FormatDays(closed) + ": " + T("day_off");
  return out;
}

// "Open • closes at 22:00" like the Android place page.
std::string FormatOpenStatus(std::string_view raw, bool & isOpen, bool & known)
{
  known = false;
  osmoh::OpeningHours oh(raw);
  if (!oh.IsValid())
    return {};
  time_t const now = std::time(nullptr);
  auto const info = oh.GetInfo(now);
  if (info.state == osmoh::RuleState::Unknown)
    return {};
  known = true;
  isOpen = info.state == osmoh::RuleState::Open;
  if (isOpen)
  {
    if (oh.IsTwentyFourHours())
      return T("twentyfour_seven");
    auto const left = info.nextTimeClosed - now;
    std::string s = T("editor_time_open");
    if (left > 0 && left <= 60 * 60)
      s += " • " + TF("closes_in", {std::to_string((left + 59) / 60) + " " + T("minute")});
    else if (left > 0 && left < 24 * 60 * 60)
      s += " • " + TF("closes_at", {TimeOfDay(info.nextTimeClosed)});
    return s;
  }
  std::string s = T("closed_now");
  auto const until = info.nextTimeOpen - now;
  if (until <= 0)
    return s;
  std::tm nowTm = *std::localtime(&now);
  std::tm openTm = *std::localtime(&info.nextTimeOpen);
  if (until <= 60 * 60)
    s += " • " + TF("opens_in", {std::to_string((until + 59) / 60) + " " + T("minute")});
  else if (openTm.tm_yday == nowTm.tm_yday && openTm.tm_year == nowTm.tm_year)
    s += " • " + TF("opens_at", {TimeOfDay(info.nextTimeOpen)});
  else if (until < 48 * 60 * 60 && (openTm.tm_yday == nowTm.tm_yday + 1 || openTm.tm_yday == 0))
    s += " • " + TF("opens_tomorrow_at", {TimeOfDay(info.nextTimeOpen)});
  else if (until < 7 * 24 * 60 * 60)
    s += " • " + TF("opens_dayoftheweek_at", {WeekdayName(static_cast<osmoh::Weekday>(openTm.tm_wday + 1)),
                                               TimeOfDay(info.nextTimeOpen)});
  return s;
}

GtkWidget * MakeLabel(char const * css, bool wrap = true)
{
  auto * l = gtk_label_new("");
  gtk_label_set_xalign(GTK_LABEL(l), 0);
  gtk_label_set_wrap(GTK_LABEL(l), wrap);
  gtk_label_set_wrap_mode(GTK_LABEL(l), PANGO_WRAP_WORD_CHAR);
  gtk_label_set_selectable(GTK_LABEL(l), FALSE);
  if (css)
    gtk_widget_add_css_class(l, css);
  return l;
}

// Wikipedia descriptions are HTML: plain text with entities decoded, paragraphs kept.
std::string HtmlToText(std::string const & html)
{
  static std::pair<char const *, char const *> const kEntities[] = {
      {"nbsp", "\u00a0"}, {"amp", "&"},      {"lt", "<"},       {"gt", ">"},       {"quot", "\""},
      {"apos", "'"},       {"mdash", "—"},    {"ndash", "–"},    {"laquo", "«"},    {"raquo", "»"},
      {"hellip", "…"},     {"deg", "°"},      {"middot", "·"},   {"thinsp", "\u2009"}, {"shy", ""}};
  std::string out;
  bool inTag = false;
  std::string tag;
  for (size_t i = 0; i < html.size(); ++i)
  {
    char const c = html[i];
    if (inTag)
    {
      if (c == '>')
      {
        inTag = false;
        if ((tag.rfind("/p", 0) == 0 || tag.rfind("br", 0) == 0) && !out.empty() && out.back() != '\n')
          out += '\n';
      }
      else
        tag += c;
      continue;
    }
    if (c == '<')
    {
      inTag = true;
      tag.clear();
      continue;
    }
    if (c == '&')
    {
      auto const semi = html.find(';', i);
      if (semi != std::string::npos && semi - i <= 8)
      {
        std::string const name = html.substr(i + 1, semi - i - 1);
        bool done = false;
        if (!name.empty() && name[0] == '#')
        {
          gunichar const code = static_cast<gunichar>(
              name.size() > 1 && (name[1] == 'x' || name[1] == 'X') ? std::strtoul(name.c_str() + 2, nullptr, 16)
                                                                    : std::strtoul(name.c_str() + 1, nullptr, 10));
          char buf[8] = {};
          g_unichar_to_utf8(code, buf);
          out += buf;
          done = true;
        }
        for (auto const & [n, v] : kEntities)
        {
          if (!done && name == n)
          {
            out += v;
            done = true;
          }
        }
        if (done)
        {
          i = semi;
          continue;
        }
      }
    }
    out += c;
  }
  while (!out.empty() && (out.back() == '\n' || out.back() == ' '))
    out.pop_back();
  return out;
}

void SetLabelOrHide(GtkWidget * label, std::string const & text)
{
  gtk_label_set_text(GTK_LABEL(label), text.c_str());
  gtk_widget_set_visible(label, !text.empty());
}
}  // namespace

PlacePage::PlacePage(MainWindow & window) : m_window(window)
{
  m_root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_margin_bottom(m_root, 6);

  auto * header = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  gtk_widget_set_margin_start(header, 18);
  gtk_widget_set_margin_end(header, 18);
  gtk_widget_set_margin_top(header, 20);

  auto * titleRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  m_title = MakeLabel("pp-title");
  gtk_widget_set_hexpand(m_title, TRUE);
  m_distance = MakeLabel("dim-label", false);
  gtk_widget_set_valign(m_distance, GTK_ALIGN_START);
  gtk_widget_set_margin_top(m_distance, 6);
  gtk_box_append(GTK_BOX(titleRow), m_title);
  gtk_box_append(GTK_BOX(titleRow), m_distance);
  gtk_box_append(GTK_BOX(header), titleRow);

  m_secondaryTitle = MakeLabel(nullptr);
  m_subtitle = MakeLabel("pp-subtitle");
  m_address = MakeLabel("pp-subtitle");
  m_hoursStatus = MakeLabel(nullptr);
  gtk_box_append(GTK_BOX(header), m_secondaryTitle);
  gtk_box_append(GTK_BOX(header), m_subtitle);
  gtk_box_append(GTK_BOX(header), m_address);
  gtk_box_append(GTK_BOX(header), m_hoursStatus);
  gtk_box_append(GTK_BOX(m_root), header);

  m_actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_box_set_homogeneous(GTK_BOX(m_actions), TRUE);
  gtk_widget_add_css_class(m_actions, "pp-actions");
  gtk_widget_set_margin_start(m_actions, 8);
  gtk_widget_set_margin_end(m_actions, 8);
  gtk_widget_set_margin_top(m_actions, 8);
  gtk_box_append(GTK_BOX(m_root), m_actions);

  m_details = gtk_list_box_new();
  gtk_list_box_set_selection_mode(GTK_LIST_BOX(m_details), GTK_SELECTION_NONE);
  gtk_widget_add_css_class(m_details, "boxed-list");
  SetMarginAll(m_details, 12);

  m_scroller = gtk_scrolled_window_new();
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(m_scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(m_scroller), TRUE);
  gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(m_scroller), 380);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(m_scroller), m_details);
  gtk_box_append(GTK_BOX(m_root), m_scroller);
}

void PlacePage::Open()
{
  if (!GetFramework().HasPlacePageInfo())
    return;
  m_isOpen = true;
  Rebuild();
  m_window.SetSheetOpen(true);
}

void PlacePage::Close()
{
  m_isOpen = false;
  m_window.SetSheetOpen(false);
}

void PlacePage::Update()
{
  if (m_isOpen && GetFramework().HasPlacePageInfo())
    Rebuild();
}

void PlacePage::OnSheetClosed()
{
  if (!m_isOpen)
    return;
  m_isOpen = false;
  auto & fw = GetFramework();
  if (fw.HasPlacePageInfo())
    fw.DeactivateMapSelection();
}

void PlacePage::OnLocationUpdate()
{
  if (m_isOpen)
    UpdateDistance();
}

void PlacePage::UpdateDistance()
{
  auto & fw = GetFramework();
  auto const & last = m_window.Location().LastLocation();
  if (!fw.HasPlacePageInfo() || !last)
  {
    gtk_widget_set_visible(m_distance, FALSE);
    return;
  }
  auto const & info = fw.GetCurrentPlacePageInfo();
  if (info.IsMyPosition())
  {
    gtk_widget_set_visible(m_distance, FALSE);
    return;
  }
  platform::Distance distance;
  double azimut = 0;
  fw.GetDistanceAndAzimut(info.GetMercator(), last->m_latitude, last->m_longitude, -1.0, distance, azimut);
  SetLabelOrHide(m_distance, distance.ToString());
}

void PlacePage::Rebuild()
{
  auto & fw = GetFramework();
  auto const & info = fw.GetCurrentPlacePageInfo();

  std::string title = info.GetTitle();
  if (title.empty() && info.IsMyPosition())
    title = T("p2p_your_location");
  SetLabelOrHide(m_title, title);
  SetLabelOrHide(m_secondaryTitle, info.GetSecondaryTitle());
  SetLabelOrHide(m_subtitle, info.GetSubtitle());
  SetLabelOrHide(m_address, info.GetSecondarySubtitle());

  bool open = false, known = false;
  auto const status = FormatOpenStatus(info.GetOpeningHours(), open, known);
  SetLabelOrHide(m_hoursStatus, status);
  gtk_widget_remove_css_class(m_hoursStatus, "pp-status-open");
  gtk_widget_remove_css_class(m_hoursStatus, "pp-status-closed");
  if (known)
    gtk_widget_add_css_class(m_hoursStatus, open ? "pp-status-open" : "pp-status-closed");

  UpdateDistance();
  BuildActions();
  BuildDetails();
  m_coordFormat = 0;
}

GtkWidget * PlacePage::AddAction(char const * icon, std::string const & label, std::function<void()> action,
                                 bool active)
{
  auto * button = gtk_button_new();
  auto * box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  auto * image = gtk_image_new_from_icon_name(icon);
  gtk_image_set_icon_size(GTK_IMAGE(image), GTK_ICON_SIZE_LARGE);
  auto * text = gtk_label_new(label.c_str());
  gtk_label_set_ellipsize(GTK_LABEL(text), PANGO_ELLIPSIZE_END);
  gtk_widget_add_css_class(text, "caption");
  gtk_box_append(GTK_BOX(box), image);
  gtk_box_append(GTK_BOX(box), text);
  gtk_button_set_child(GTK_BUTTON(button), box);
  gtk_widget_add_css_class(button, "flat");
  if (active)
    gtk_widget_add_css_class(button, "active");
  OnClicked(button, std::move(action));
  gtk_box_append(GTK_BOX(m_actions), button);
  return button;
}

void PlacePage::BuildActions()
{
  ClearBox(m_actions);
  auto & fw = GetFramework();
  auto const & info = fw.GetCurrentPlacePageInfo();
  auto & rm = fw.GetRoutingManager();
  bool const planning = rm.IsRoutingActive() && !rm.IsRoutingFollowing();

  if (info.IsRoutePoint())
  {
    AddAction("om-route-remove-symbolic", T("placepage_remove_stop"), [this]() { RemoveStop(); });
    return;
  }

  if (!info.IsMyPosition() && !info.IsTrack())
    AddAction(info.IsBookmark() ? "starred-symbolic" : "non-starred-symbolic", info.IsBookmark() ? T("delete") : T("save"),
              [this]() { ToggleBookmark(); }, info.IsBookmark());

  if (!rm.IsRoutingFollowing())
  {
    AddAction("om-flag-symbolic", T("p2p_from_here"), [this]() { RouteFrom(); });
    if (!info.IsMyPosition())
      AddAction("om-route-symbolic", T("p2p_to_here"), [this]() { RouteTo(); });
    if (planning && rm.CouldAddIntermediatePoint() && !info.IsMyPosition())
      AddAction("list-add-symbolic", T("placepage_add_stop"), [this]() { AddStop(); });
  }

  AddAction("send-to-symbolic", T("share"), [this]() { Share(); });
}

GtkWidget * PlacePage::AddRow(char const * icon, std::string const & title, std::string const & subtitle,
                              std::function<void()> onActivate, bool copyable)
{
  auto * row = adw_action_row_new();
  adw_preferences_row_set_use_markup(ADW_PREFERENCES_ROW(row), FALSE);
  adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), title.c_str());
  if (!subtitle.empty())
    adw_action_row_set_subtitle(ADW_ACTION_ROW(row), subtitle.c_str());
  adw_action_row_set_title_lines(ADW_ACTION_ROW(row), 0);
  if (icon)
    adw_action_row_add_prefix(ADW_ACTION_ROW(row), gtk_image_new_from_icon_name(icon));
  if (copyable)
  {
    auto * copy = MakeIconButton("edit-copy-symbolic", T("copy").c_str());
    gtk_widget_add_css_class(copy, "flat");
    gtk_widget_set_valign(copy, GTK_ALIGN_CENTER);
    std::string const value = title;
    OnClicked(copy, [value]()
    {
      CopyToClipboard(value);
      ShowToast(TF("copied_to_clipboard", {value}));
    });
    adw_action_row_add_suffix(ADW_ACTION_ROW(row), copy);
  }
  if (onActivate)
  {
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), TRUE);
    auto fn = std::move(onActivate);
    Connect<void(AdwActionRow *)>(row, "activated", [fn](AdwActionRow *) { fn(); });
  }
  gtk_list_box_append(GTK_LIST_BOX(m_details), row);
  return row;
}

void PlacePage::BuildDetails()
{
  ClearListBox(GTK_LIST_BOX(m_details));
  auto & fw = GetFramework();
  auto const & info = fw.GetCurrentPlacePageInfo();
  using MD = osm::MapObject::MetadataID;

  // Bookmark description first, like on Android.
  if (info.IsBookmark())
  {
    auto const & bm = info.GetBookmarkData();
    auto const desc = kml::GetDefaultStr(bm.m_description);
    if (!desc.empty())
      AddRow("text-x-generic-symbolic", desc, {}, {}, false);
    AddRow("document-edit-symbolic", T("placepage_edit_bookmark_button"), info.GetBookmarkCategoryName(),
           [this, id = info.GetBookmarkId()]() { BookmarksPage::EditBookmark(m_window, id); }, false);
  }

  if (info.IsTrack() && !info.IsRelationTrack())
  {
    if (auto const * track = fw.GetBookmarkManager().GetTrack(info.GetTrackId()))
    {
      auto const stats = track->GetStatistics();
      std::string summary = stats.GetFormattedLength();
      if (stats.m_duration > 0)
        summary += " • " + stats.GetFormattedDuration();
      AddRow("om-ruler-symbolic", summary, {}, {}, false);
      if (auto const * ei = track->GetElevationInfo(); ei && !ei->IsEmpty())
      {
        AddRow("om-terrain-symbolic",
               "↗ " + stats.GetFormattedAscent() + "   ↘ " + stats.GetFormattedDescent() + "   " +
                   stats.GetFormattedMinElevation() + " – " + stats.GetFormattedMaxElevation(),
               {}, {}, false);
        auto * chartRow = gtk_list_box_row_new();
        gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(chartRow), FALSE);
        auto * chart = MakeElevationChart(*ei);
        SetMarginAll(chart, 10);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(chartRow), chart);
        gtk_list_box_append(GTK_LIST_BOX(m_details), chartRow);
      }
    }
  }

  if (auto const oh = info.GetOpeningHours(); !oh.empty())
    AddRow("om-clock-symbolic", FormatSchedule(oh), T("editor_time_title"), {}, false);

  if (auto const & address = info.GetAddress(); !address.empty())
    AddRow("om-pin-symbolic", address, T("address"));

  for (auto const & phoneRaw : strings::Tokenize(info.GetMetadata(MD::FMD_PHONE_NUMBER), ";"))
  {
    std::string phone = Trim(std::string(phoneRaw));
    AddRow("om-phone-symbolic", phone, T("phone"), [phone]()
    {
      std::string uri = "tel:";
      for (char c : phone)
        if (isdigit(static_cast<unsigned char>(c)) || c == '+')
          uri += c;
      OpenUri(uri);
    });
  }

  auto const addLink = [this](char const * icon, std::string_view value, std::string const & label)
  {
    if (value.empty())
      return;
    std::string url(value);
    if (url.find("://") == std::string::npos)
      url = "https://" + url;
    AddRow(icon, std::string(value), label, [url]() { OpenUri(url); });
  };
  addLink("web-browser-symbolic", info.GetMetadata(MD::FMD_WEBSITE), T("website"));
  addLink("web-browser-symbolic", info.GetMetadata(MD::FMD_WEBSITE_MENU), T("menu"));
  if (auto const email = info.GetMetadata(MD::FMD_EMAIL); !email.empty())
    AddRow("mail-unread-symbolic", std::string(email), T("email"),
           [e = std::string(email)]() { OpenUri("mailto:" + e); });

  for (auto const id : {MD::FMD_CONTACT_FACEBOOK, MD::FMD_CONTACT_INSTAGRAM, MD::FMD_CONTACT_TWITTER,
                        MD::FMD_CONTACT_VK, MD::FMD_CONTACT_LINE})
  {
    auto const value = info.GetMetadata(id);
    if (value.empty())
      continue;
    auto const url = osm::socialContactToURL(id, value);
    AddRow("web-browser-symbolic", std::string(value), {}, [url]() { OpenUri(url); });
  }

  if (auto const wiki = info.GetMetadata(MD::FMD_WIKIPEDIA); !wiki.empty())
  {
    auto const url = feature::Metadata::ToWikiURL(std::string(wiki));
    AddRow("help-browser-symbolic", T("read_in_wikipedia"), {}, [url]() { OpenUri(url); }, false);
  }
  if (auto const & wikiDesc = info.GetWikiDescription(); !wikiDesc.empty())
  {
    std::string const full = HtmlToText(wikiDesc);
    std::string text = full;
    std::function<void()> more;
    if (text.size() > 600)
    {
      text = text.substr(0, text.rfind(' ', 600)) + "…";
      // "More": the whole article summary in a dialog.
      more = [this, full, title = info.GetTitle()]()
      {
        auto * dialog = adw_dialog_new();
        adw_dialog_set_title(dialog, title.c_str());
        adw_dialog_set_content_height(dialog, 600);
        auto * toolbar = adw_toolbar_view_new();
        adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar), adw_header_bar_new());
        auto * label = gtk_label_new(full.c_str());
        gtk_label_set_wrap(GTK_LABEL(label), TRUE);
        gtk_label_set_xalign(GTK_LABEL(label), 0);
        gtk_label_set_selectable(GTK_LABEL(label), TRUE);
        SetMarginAll(label, 18);
        auto * scroller = gtk_scrolled_window_new();
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), label);
        gtk_widget_set_vexpand(scroller, TRUE);
        adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar), scroller);
        adw_dialog_set_child(dialog, toolbar);
        adw_dialog_present(dialog, GTK_WIDGET(m_window.Window()));
      };
    }
    auto * row = AddRow(nullptr, text, more ? T("placepage_more_button") : std::string(), more, false);
    UNUSED_VALUE(row);
  }

  if (auto const cuisines = info.FormatCuisines(); !cuisines.empty())
    AddRow("om-cuisine-symbolic", cuisines, T("cuisine"), {}, false);
  if (auto const op = info.GetMetadata(MD::FMD_OPERATOR); !op.empty())
    AddRow("system-users-symbolic", std::string(op), T("editor_operator"), {}, false);
  if (info.HasWifi())
    AddRow("om-wifi-symbolic", T("category_wifi"), {}, {}, false);
  if (auto const level = info.GetMetadata(MD::FMD_LEVEL); !level.empty())
    AddRow("go-up-symbolic", std::string(level), T("level"), {}, false);
  if (auto const ele = info.GetMetadata(MD::FMD_ELE); !ele.empty())
  {
    double meters = 0;
    if (strings::to_double(ele, meters))
      AddRow("om-terrain-symbolic", platform::Distance::FormatAltitude(meters), {}, {}, false);
  }

  // Coordinates: tap to switch the format, copy button copies the bare value.
  auto const entries = place_page::GetAvailableCoordinateFormats(info.GetLatLon(), info.GetCountryId());
  if (!entries.empty())
  {
    auto * row = adw_action_row_new();
    adw_action_row_add_prefix(ADW_ACTION_ROW(row), gtk_image_new_from_icon_name("find-location-symbolic"));
    auto const show = [row, entries](size_t i)
    {
      auto const & e = entries[i % entries.size()];
      adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), e.m_display.c_str());
    };
    show(0);
    adw_preferences_row_set_use_markup(ADW_PREFERENCES_ROW(row), FALSE);
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), TRUE);
    Connect<void(AdwActionRow *)>(row, "activated", [this, show](AdwActionRow *) { show(++m_coordFormat); });
    auto * copy = MakeIconButton("edit-copy-symbolic", T("copy").c_str());
    gtk_widget_add_css_class(copy, "flat");
    gtk_widget_set_valign(copy, GTK_ALIGN_CENTER);
    OnClicked(copy, [this, entries]()
    {
      auto const & value = entries[m_coordFormat % entries.size()].m_value;
      CopyToClipboard(value);
      ShowToast(TF("copied_to_clipboard", {value}));
    });
    adw_action_row_add_suffix(ADW_ACTION_ROW(row), copy);
    gtk_list_box_append(GTK_LIST_BOX(m_details), row);
  }

  gtk_widget_set_visible(m_scroller, gtk_widget_get_first_child(m_details) != nullptr);
}

void PlacePage::ToggleBookmark()
{
  auto & fw = GetFramework();
  if (!fw.HasPlacePageInfo())
    return;
  auto const & info = fw.GetCurrentPlacePageInfo();
  auto & bmManager = fw.GetBookmarkManager();
  auto buildInfo = info.GetBuildInfo();

  if (info.IsBookmark())
  {
    bmManager.GetEditSession().DeleteBookmark(info.GetBookmarkId());
    buildInfo.m_match = place_page::BuildInfo::Match::FeatureOnly;
    buildInfo.m_userMarkId = kml::kInvalidMarkId;
    buildInfo.m_source = place_page::BuildInfo::Source::Other;
  }
  else
  {
    kml::BookmarkData bmData;
    bmData.m_name = info.FormatNewBookmarkName();
    bmData.m_point = info.GetMercator();
    bmData.m_color = fw.LastEditedBMColor();
    if (info.IsFeature())
      SaveFeatureTypes(info.GetTypes(), bmData);
    auto const * bm = bmManager.GetEditSession().CreateBookmark(std::move(bmData), fw.LastEditedBMCategory());
    buildInfo.m_match = place_page::BuildInfo::Match::Everything;
    buildInfo.m_userMarkId = bm->GetId();
  }
  fw.UpdatePlacePageInfoForCurrentSelection(buildInfo);
  Rebuild();
}

void PlacePage::RouteFrom()
{
  m_window.Routing().SetPointFromPlacePage(RouteMarkType::Start);
}

void PlacePage::RouteTo()
{
  m_window.Routing().SetPointFromPlacePage(RouteMarkType::Finish);
}

void PlacePage::AddStop()
{
  m_window.Routing().SetPointFromPlacePage(RouteMarkType::Intermediate);
}

void PlacePage::RemoveStop()
{
  auto & fw = GetFramework();
  auto const & info = fw.GetCurrentPlacePageInfo();
  m_window.Routing().RemovePoint(info.GetRouteMarkType(), info.GetIntermediateIndex());
}

void PlacePage::Share()
{
  auto & fw = GetFramework();
  if (!fw.HasPlacePageInfo())
    return;
  auto const data = fw.GetShareData(fw.GetCurrentPlacePageInfo());
  CopyToClipboard(data.m_text.empty() ? data.m_url : data.m_text);
  ShowToast(TF("copied_to_clipboard", {data.m_url}));
}
}  // namespace omgtk
