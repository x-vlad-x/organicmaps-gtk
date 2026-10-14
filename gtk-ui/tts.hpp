#pragma once

#include <string>
#include <vector>

namespace lichen
{
// Voice prompts: speech-dispatcher (spd-say) when installed, otherwise the espeak-ng bundled with
// the app. Phrases come from the core (data/sound-strings), already in the right language.
namespace tts
{
bool IsAvailable();
bool IsEnabled();
void SetEnabled(bool enabled);
void Speak(std::vector<std::string> const & phrases);
void Speak(std::string const & phrase);
}  // namespace tts
}  // namespace lichen
