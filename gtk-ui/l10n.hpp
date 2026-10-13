#pragma once

#include <string>
#include <vector>

namespace omgtk
{
// UI strings come from data/strings/strings.txt (the same twine file Android and iOS use),
// so every label is translated exactly like in the phone app.
void InitLocalization();
std::string const & CurrentLanguage();

// Returns the translation for |key| or |key| itself when missing.
std::string T(std::string const & key);
bool HasString(std::string const & key);

// Substitutes iOS-style placeholders (%@, %1$@, %d, %s, %1$s ...) in order.
std::string Format(std::string const & fmt, std::vector<std::string> const & args);
std::string TF(std::string const & key, std::vector<std::string> const & args);
}  // namespace omgtk
