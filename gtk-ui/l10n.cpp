#include "l10n.hpp"

#include "platform/localization.hpp"
#include "platform/platform.hpp"
#include "platform/preferred_languages.hpp"

#include "base/file_name_utils.hpp"
#include "base/logging.hpp"

#include <glib.h>

#include <cctype>
#include <cstring>
#include <ctime>
#include <fstream>
#include <mutex>
#include <unordered_map>

namespace omgtk
{
namespace
{
std::unordered_map<std::string, std::string> g_strings;
// Twine aliases: "ref = other.key" (e.g. type.place.city.capital.2 -> type.place.city.capital).
std::unordered_map<std::string, std::string> g_refs;
std::string g_language = "en";
std::once_flag g_initOnce;

std::string Unescape(std::string_view v)
{
  std::string out;
  out.reserve(v.size());
  for (size_t i = 0; i < v.size(); ++i)
  {
    if (v[i] == '\\' && i + 1 < v.size())
    {
      char const n = v[++i];
      if (n == 'n')
        out += '\n';
      else if (n == 't')
        out += '\t';
      else
        out += n;
    }
    else
      out += v[i];
  }
  return out;
}

std::string_view Trim(std::string_view s)
{
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r'))
    s.remove_prefix(1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
    s.remove_suffix(1);
  return s;
}

// Twine format: "[key]" starts a section, "lang = value" lines follow.
void LoadTwine(std::string const & path, std::vector<std::string> const & langs)
{
  std::ifstream in(path);
  if (!in)
  {
    LOG(LWARNING, ("Can't open strings file", path));
    return;
  }

  std::string line, key;
  // Index of the best language found for the current key (lower is better).
  size_t bestRank = langs.size();
  std::string best;
  auto const flush = [&]()
  {
    if (!key.empty() && !best.empty())
      g_strings[key] = std::move(best);
    best.clear();
    bestRank = langs.size();
  };

  while (std::getline(in, line))
  {
    std::string_view l = Trim(line);
    if (l.empty())
      continue;
    if (l.front() == '[')
    {
      flush();
      if (l.size() > 2 && l[1] == '[')
        key.clear();  // [[Section]]
      else
        key = std::string(l.substr(1, l.find(']') - 1));
      continue;
    }
    if (key.empty())
      continue;
    auto const eq = l.find('=');
    if (eq == std::string_view::npos)
      continue;
    auto const lang = Trim(l.substr(0, eq));
    if (lang == "ref")
    {
      g_refs[key] = std::string(Trim(l.substr(eq + 1)));
      continue;
    }
    for (size_t i = 0; i < bestRank; ++i)
    {
      if (lang == langs[i])
      {
        best = Unescape(Trim(l.substr(eq + 1)));
        bestRank = i;
        break;
      }
    }
  }
  flush();
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

    std::vector<std::string> langs{lang};
    if (auto const dash = lang.find('-'); dash != std::string::npos)
      langs.push_back(lang.substr(0, dash));
    if (lang != "en")
      langs.push_back("en");

    auto const dir = base::JoinPath(GetPlatform().ResourcesDir(), "strings");
    LoadTwine(base::JoinPath(dir, "strings.txt"), langs);
    LoadTwine(base::JoinPath(dir, "types_strings.txt"), langs);
    for (auto const & [key, target] : g_refs)
    {
      if (g_strings.count(key))
        continue;
      std::string t = target;
      for (int depth = 0; depth < 5; ++depth)
      {
        if (auto const it = g_strings.find(t); it != g_strings.end())
        {
          g_strings[key] = it->second;
          break;
        }
        auto const r = g_refs.find(t);
        if (r == g_refs.end())
          break;
        t = r->second;
      }
    }
    g_refs.clear();
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
}  // namespace omgtk

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
  return omgtk::T(key);
}

std::string GetLocalizedBrandName(std::string const & brand)
{
  return brand;
}

std::string GetLocalizedString(std::string const & key)
{
  return omgtk::T(key);
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
