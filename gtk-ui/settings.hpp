#pragma once

namespace lichen
{
class MainWindow;

// The CoMaps core only knows light and dark map styles, so the app keeps the night mode choice,
// including following the system style, as its own preference.
enum class NightMode : int
{
  Off = 0,
  On = 1,
  System = 2,
};

void ShowSettingsDialog(MainWindow & window);
void ShowLayersDialog(MainWindow & window);
void ShowAboutDialog(MainWindow & window);

// App-only preferences (not stored by the core).
bool ZoomButtonsEnabled();
bool KeepScreenOn();
NightMode GetNightMode();
void ApplyNightMode(NightMode mode);
void SyncAppColorScheme();
}  // namespace lichen
