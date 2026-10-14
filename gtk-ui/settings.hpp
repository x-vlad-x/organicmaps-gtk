#pragma once

#include "platform/style_utils.hpp"

namespace lichen
{
class MainWindow;

void ShowSettingsDialog(MainWindow & window);
void ShowLayersDialog(MainWindow & window);
void ShowAboutDialog(MainWindow & window);

// App-only preferences (not stored by the core).
bool ZoomButtonsEnabled();
bool KeepScreenOn();
void ApplyNightMode(style_utils::NightMode mode);
void SyncAppColorScheme();
}  // namespace lichen
