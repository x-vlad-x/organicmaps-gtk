#include "l10n.hpp"

#include "platform/localization.hpp"
#include "platform/platform.hpp"
#include "platform/preferred_languages.hpp"

#include "base/file_name_utils.hpp"
#include "base/logging.hpp"

#include <glib.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace lichen
{
namespace
{
std::unordered_map<std::string, std::string> g_strings;
std::string g_language = "en";
std::once_flag g_initOnce;

// English text for keys the CoMaps Android resources lack. The interface took these from the
// Organic Maps string set (shared with iOS), so they stay untranslated here.
// TODO: replace them with CoMaps keys or contribute the strings to CoMaps.
std::unordered_map<std::string, std::string> const kEnglishFallback = {
    {"cancel_download", "Cancel Download"},
    {"downloader_available_maps", "Available"},
    {"downloader_search_results", "Found"},
    {"downloader_status_outdated", "Update"},
    {"mode_cycling", "Cycling"},
    {"mode_driving", "Driving"},
    {"mode_public-transport", "Public Transport"},
    {"mode_walking", "Walking"},
    {"opens_dayoftheweek_at", "Opens %1$@ at %2$@"},
    {"phone", "Phone"},
    {"placepage_bookmark_name_hint", "Place Name"},
    {"placepage_delete_bookmark_button", "Delete Place"},
    {"route", "Route"},
    {"routing_arrive", "Arrival at %@"},
    {"routing_planning_error", "Route Planning Failed"},
    {"then_turn", "Then"},
    {"track_recording_stop_and_save", "Save the recorded track"},
};

// Android string resource escapes: \n, \t, \', \", \\, \uXXXX; an unescaped double quote only
// marks a quoted part of the string.
std::string UnescapeAndroid(std::string_view v)
{
  std::string out;
  out.reserve(v.size());
  for (size_t i = 0; i < v.size(); ++i)
  {
    char const c = v[i];
    if (c == '"')
      continue;
    if (c != '\\' || i + 1 == v.size())
    {
      out += c;
      continue;
    }
    char const n = v[++i];
    if (n == 'n')
      out += '\n';
    else if (n == 't')
      out += '\t';
    else if (n == 'u' && i + 4 < v.size())
    {
      char hex[5] = {v[i + 1], v[i + 2], v[i + 3], v[i + 4], 0};
      char utf8[6] = {0};
      g_unichar_to_utf8(static_cast<gunichar>(std::strtoul(hex, nullptr, 16)), utf8);
      out += utf8;
      i += 4;
    }
    else
      out += n;
  }
  return out;
}

// Reads <string name="key">value</string> entries of an Android resource file. Text of nested
// elements (<xliff:g>) belongs to the value.
void LoadAndroidXml(std::string const & path)
{
  gchar * content = nullptr;
  gsize length = 0;
  if (!g_file_get_contents(path.c_str(), &content, &length, nullptr))
    return;

  struct State
  {
    std::string key;
    std::string value;
    int depth = 0;
  } state;
  GMarkupParser parser = {};
  parser.start_element = [](GMarkupParseContext *, gchar const * element, gchar const ** names,
                            gchar const ** values, gpointer data, GError **)
  {
    auto & s = *static_cast<State *>(data);
    if (s.depth > 0)
    {
      ++s.depth;
      return;
    }
    if (std::strcmp(element, "string") != 0)
      return;
    for (int i = 0; names[i]; ++i)
      if (std::strcmp(names[i], "name") == 0)
        s.key = values[i];
    s.value.clear();
    s.depth = 1;
  };
  parser.end_element = [](GMarkupParseContext *, gchar const *, gpointer data, GError **)
  {
    auto & s = *static_cast<State *>(data);
    if (s.depth == 0 || --s.depth > 0)
      return;
    auto value = UnescapeAndroid(s.value);
    if (!s.key.empty() && !value.empty())
      g_strings[s.key] = std::move(value);
    s.key.clear();
  };
  parser.text = [](GMarkupParseContext *, gchar const * text, gsize size, gpointer data, GError **)
  {
    auto & s = *static_cast<State *>(data);
    if (s.depth > 0)
      s.value.append(text, size);
  };

  auto * context = g_markup_parse_context_new(&parser, GMarkupParseFlags(0), &state, nullptr);
  GError * error = nullptr;
  if (!g_markup_parse_context_parse(context, content, static_cast<gssize>(length), &error) ||
      !g_markup_parse_context_end_parse(context, &error))
  {
    LOG(LWARNING, ("Can't parse strings file", path, error ? error->message : ""));
    g_clear_error(&error);
  }
  g_markup_parse_context_free(context);
  g_free(content);
}

// Android resource folders for the user's language, from the most to the least specific:
// "pt_BR.UTF-8" -> values-pt-rBR, values-pt; "sr_RS@latin" -> values-b+sr+Latn, values-sr;
// Traditional Chinese -> values-zh-rTW (also rHK, rMO); Simplified Chinese -> values-zh.
std::vector<std::string> ResourceFolders(std::string locale, std::string const & twine)
{
  bool const latin = locale.find("@latin") != std::string::npos;
  locale = locale.substr(0, locale.find_first_of(".@"));
  std::replace(locale.begin(), locale.end(), '_', '-');
  std::string language = locale.substr(0, locale.find('-'));
  // Android keeps the legacy codes of these languages.
  if (language == "he")
    language = "iw";
  else if (language == "id")
    language = "in";
  else if (language == "yi")
    language = "ji";
  std::string const region = locale.find('-') == std::string::npos ? "" : locale.substr(locale.rfind('-') + 1);

  std::vector<std::string> folders;
  if (twine == "zh-Hant")
  {
    if (region == "HK" || region == "MO")
      folders.push_back("values-zh-r" + region);
    for (char const * r : {"TW", "HK", "MO"})
      folders.push_back(std::string("values-zh-r") + r);
    return folders;
  }
  if (latin)
    folders.push_back("values-b+" + language + "+Latn");
  if (region.size() == 2 && std::isupper(static_cast<unsigned char>(region[0])))
    folders.push_back("values-" + language + "-r" + region);
  else if (region.size() == 3 && std::isdigit(static_cast<unsigned char>(region[0])))
    folders.push_back("values-b+" + language + "+" + region);
  folders.push_back("values-" + language);
  return folders;
}
}  // namespace

void InitLocalization()
{
  std::call_once(g_initOnce, []()
  {
    std::string lang = languages::GetCurrentTwine();
    if (lang.empty())
      lang = "en";
    g_language = lang;

    // CoMaps keeps the UI translations in its Android resources: the app strings, the core
    // strings and the feature type names. Load the English defaults first and let each more
    // specific language folder override them.
    auto folders = ResourceFolders(languages::GetCurrentOrig(), lang);
    folders.push_back("values-en");
    folders.push_back("values");
    auto const dir = base::JoinPath(GetPlatform().ResourcesDir(), "..", "strings");
    for (auto it = folders.rbegin(); it != folders.rend(); ++it)
      for (char const * part : {"sdk", "app"})
        for (char const * file : {"strings.xml", "types_strings.xml"})
          LoadAndroidXml(base::JoinPath(dir, part, *it, file));
    if (g_strings.empty())
      LOG(LWARNING, ("No UI strings found in", dir));

    // "@string/other_key" values refer to another string.
    for (auto & [key, value] : g_strings)
    {
      for (int depth = 0; depth < 5 && value.starts_with("@string/"); ++depth)
      {
        auto const it = g_strings.find(value.substr(8));
        if (it == g_strings.end())
          break;
        value = it->second;
      }
    }
    LOG(LINFO, ("Loaded", g_strings.size(), "UI strings for", lang));
  });
}

std::string const & CurrentLanguage()
{
  InitLocalization();
  return g_language;
}

// A string the phone apps take from the OS (Android/iOS system resources): use GTK's translation.
std::string const * Fallback(std::string const & key)
{
  if (auto const it = kEnglishFallback.find(key); it != kEnglishFallback.end())
    return &it->second;
  if (key != "copy")
    return nullptr;
  static std::string const copy = []
  {
    std::string s = g_dgettext("gtk40", "_Copy");
    std::erase(s, '_');  // Mnemonic marker.
    return s;
  }();
  return &copy;
}

std::string T(std::string const & key)
{
  InitLocalization();
  auto const it = g_strings.find(key);
  if (it != g_strings.end())
    return it->second;
  if (auto const * f = Fallback(key))
    return *f;
  // Report each missing interface key once, so untranslated labels are easy to find (feature
  // types without a name are common and not reported).
  static std::mutex mutex;
  static std::unordered_set<std::string> reported;
  if (std::lock_guard lock(mutex); !key.starts_with("type.") && reported.insert(key).second)
    LOG(LWARNING, ("Missing UI string:", key));
  return key;
}

bool HasString(std::string const & key)
{
  InitLocalization();
  return g_strings.count(key) != 0;
}

std::string Format(std::string const & fmt, std::vector<std::string> const & args)
{
  std::string out;
  size_t next = 0;
  for (size_t i = 0; i < fmt.size(); ++i)
  {
    if (fmt[i] != '%' || i + 1 >= fmt.size())
    {
      out += fmt[i];
      continue;
    }
    size_t j = i + 1;
    if (fmt[j] == '%')
    {
      out += '%';
      i = j;
      continue;
    }
    // Optional positional index "N$".
    size_t index = next;
    size_t k = j;
    while (k < fmt.size() && isdigit(static_cast<unsigned char>(fmt[k])))
      ++k;
    if (k < fmt.size() && k > j && fmt[k] == '$')
    {
      index = std::stoul(fmt.substr(j, k - j)) - 1;
      j = k + 1;
    }
    // Skip flags/width/precision/length.
    while (j < fmt.size() && strchr("0123456789.-+ lhz", fmt[j]))
      ++j;
    if (j < fmt.size() && strchr("@sdfiu", fmt[j]))
    {
      if (index < args.size())
        out += args[index];
      next = index + 1;
      i = j;
    }
    else
      out += fmt[i];
  }
  return out;
}

std::string TF(std::string const & key, std::vector<std::string> const & args)
{
  return Format(T(key), args);
}
}  // namespace lichen

// Implementations of the platform localization hooks (replace localization_dummy.cpp).
namespace platform
{
std::string GetLocalizedTypeName(std::string const & type)
{
  std::string key = "type." + type;
  for (auto & c : key)
  {
    if (c == '-')
      c = '.';
    else if (c == ':')
      c = '_';
  }
  return lichen::T(key);
}

std::string GetLocalizedBrandName(std::string const & brand)
{
  return brand;
}

std::string GetLocalizedString(std::string const & key)
{
  return lichen::T(key);
}

std::string GetCurrencySymbol(std::string const & currencyCode)
{
  return currencyCode;
}

std::string GetLocalizedMyPositionBookmarkName()
{
  std::time_t t = std::time(nullptr);
  char buf[100] = {0};
  (void)std::strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M", std::localtime(&t));
  return buf;
}
}  // namespace platform
