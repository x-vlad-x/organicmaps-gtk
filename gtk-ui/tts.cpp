#include "tts.hpp"

#include "l10n.hpp"

#include "platform/settings.hpp"

#include "base/logging.hpp"

#include <glib.h>

#include <unistd.h>

namespace omgtk::tts
{
namespace
{
char const kEnabledKey[] = "GtkTtsEnabled";

std::string ExeDir()
{
  char path[4096] = {};
  ssize_t const n = ::readlink("/proc/self/exe", path, sizeof(path) - 1);
  std::string s(path, n > 0 ? static_cast<size_t>(n) : 0);
  return s.substr(0, s.rfind('/'));
}

struct Engine
{
  std::string spdSay;
  std::string espeak;      // bundled espeak-ng binary
  std::string espeakRoot;  // its prefix (lib + data)
};

Engine const & GetEngine()
{
  static Engine const engine = []()
  {
    Engine e;
    if (char * p = g_find_program_in_path("spd-say"))
    {
      e.spdSay = p;
      g_free(p);
    }
    auto const root = ExeDir() + "/../tts";
    if (g_file_test((root + "/usr/bin/espeak-ng").c_str(), G_FILE_TEST_IS_EXECUTABLE))
    {
      e.espeak = root + "/usr/bin/espeak-ng";
      e.espeakRoot = root;
    }
    else if (char * p = g_find_program_in_path("espeak-ng"))
    {
      e.espeak = p;
      g_free(p);
    }
    LOG(LINFO, ("TTS engine:", !e.spdSay.empty() ? e.spdSay : (e.espeak.empty() ? "none" : e.espeak)));
    return e;
  }();
  return engine;
}

// The language code understood by both speech-dispatcher and espeak-ng.
std::string Voice()
{
  auto lang = CurrentLanguage();
  if (auto const dash = lang.find('-'); dash != std::string::npos)
    lang = lang.substr(0, dash);
  return lang;
}
}  // namespace

bool IsAvailable()
{
  auto const & e = GetEngine();
  return !e.spdSay.empty() || !e.espeak.empty();
}

bool IsEnabled()
{
  bool enabled = true;
  settings::TryGet(kEnabledKey, enabled);
  return enabled && IsAvailable();
}

void SetEnabled(bool enabled)
{
  settings::Set(kEnabledKey, enabled);
}

void Speak(std::string const & phrase)
{
  if (phrase.empty() || !IsAvailable())
    return;
  auto const & e = GetEngine();
  std::vector<std::string> args;
  std::vector<std::string> env;
  if (!e.spdSay.empty())
    args = {e.spdSay, "-l", Voice(), "-r", "10", phrase};
  else
  {
    args = {e.espeak, "-v", Voice(), "-s", "165", phrase};
    if (!e.espeakRoot.empty())
    {
      auto const multiarch = e.espeakRoot + "/usr/lib/aarch64-linux-gnu";
      env.push_back("LD_LIBRARY_PATH=" + multiarch);
      env.push_back("ESPEAK_DATA_PATH=" + multiarch + "/espeak-ng-data");
    }
  }

  std::vector<char *> argv;
  for (auto & a : args)
    argv.push_back(a.data());
  argv.push_back(nullptr);

  char ** envp = g_get_environ();
  for (auto const & kv : env)
  {
    auto const eq = kv.find('=');
    envp = g_environ_setenv(envp, kv.substr(0, eq).c_str(), kv.substr(eq + 1).c_str(), TRUE);
  }
  GError * error = nullptr;
  if (!g_spawn_async(nullptr, argv.data(), envp, G_SPAWN_DEFAULT, nullptr, nullptr, nullptr, &error))
  {
    LOG(LWARNING, ("TTS failed:", error->message));
    g_clear_error(&error);
  }
  g_strfreev(envp);
}

void Speak(std::vector<std::string> const & phrases)
{
  std::string text;
  for (auto const & p : phrases)
  {
    if (!text.empty())
      text += ". ";
    text += p;
  }
  Speak(text);
}
}  // namespace omgtk::tts
