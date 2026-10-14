#pragma once

#include <cstdint>
#include <string>

namespace omgtk
{
inline constexpr char kAppCss[] = R"css(
.map-view { background-color: #ece9e2; }

button.map-button {
  min-width: 48px;
  min-height: 48px;
  padding: 0;
  border-radius: 999px;
  background-color: alpha(var(--window-bg-color), 0.96);
  color: var(--window-fg-color);
  box-shadow: 0 1px 4px alpha(black, 0.30);
}
button.map-button:hover { background-color: mix(var(--window-bg-color), var(--window-fg-color), 0.05); }
button.map-button:active { background-color: mix(var(--window-bg-color), var(--window-fg-color), 0.12); }
button.map-button.following { color: var(--accent-color); }
button.map-button.pending { color: alpha(var(--window-fg-color), 0.5); }

.map-bottom { margin-bottom: 0; }
.map-attribution {
  background-color: alpha(var(--window-bg-color), 0.96);
  color: var(--window-fg-color);
  padding: 2px 6px;
  border-radius: 6px 6px 0 0;
}

.bottom-menu {
  background-color: var(--window-bg-color);
  border-top: 1px solid alpha(var(--window-fg-color), 0.10);
  padding: 2px 4px;
}
.bottom-menu-button { padding: 6px 2px; min-height: 52px; border-radius: 12px; }

.download-card { padding: 16px; }

/* Top panels over the map. */
.map-top-panel {
  background-color: var(--window-bg-color);
  box-shadow: 0 1px 4px alpha(black, 0.25);
  padding: 6px 8px;
}
.map-bottom-panel {
  background-color: var(--window-bg-color);
  border-top: 1px solid alpha(var(--window-fg-color), 0.10);
  box-shadow: 0 -1px 4px alpha(black, 0.15);
  padding: 10px 12px;
}

/* Navigation: green turn panel like the Android app. */
.nav-turn-panel {
  background-color: #26803c;
  color: white;
  padding: 10px 14px;
  border-radius: 0 0 16px 16px;
  box-shadow: 0 2px 6px alpha(black, 0.35);
}
.nav-turn-distance { font-size: 30px; font-weight: 800; }
.nav-turn-street { font-size: 19px; font-weight: 600; }
.nav-next-turn {
  background-color: #1e6630;
  color: white;
  padding: 4px 10px;
  border-radius: 0 0 12px 12px;
  margin-left: 14px;
}
.nav-lane { color: alpha(white, 0.45); }
.nav-lane.recommended { color: white; }
.nav-stats-value { font-size: 20px; font-weight: 700; }
.nav-speed { font-size: 22px; font-weight: 800; }
.nav-speed.over { color: var(--error-color); }
.speed-limit {
  min-width: 44px; min-height: 44px;
  border-radius: 999px;
  border: 4px solid #e53935;
  background-color: white; color: black;
  font-weight: 800; font-size: 16px;
}

.route-type-button { min-height: 40px; min-width: 52px; border-radius: 999px; }
.route-type-button:checked { background-color: alpha(var(--accent-bg-color), 0.18); color: var(--accent-color); }
.route-point-dot { min-width: 10px; min-height: 10px; border-radius: 999px; }
.route-point-dot.start { background-color: #1e88e5; }
.route-point-dot.finish { background-color: #e53935; }
.route-point-dot.via { background-color: #fb8c00; }

/* Place page. */
.pp-title { font-size: 21px; font-weight: 700; }
.pp-subtitle { opacity: 0.75; }
.pp-actions button { min-height: 56px; border-radius: 12px; padding: 4px 6px; }
.pp-actions button.active { color: var(--accent-color); }
.pp-status-open { color: var(--success-color); }
.pp-status-closed { color: var(--error-color); }

.search-result-type { opacity: 0.7; font-size: 0.9em; }
.search-category-row { min-height: 48px; }
.search-on-map-bar { padding: 6px 8px; }

.color-dot { min-width: 18px; min-height: 18px; border-radius: 999px; }
)css";

std::string FormatSize(uint64_t bytes);
std::string FormatDistance(double meters);
std::string FormatDuration(double seconds);
std::string FormatArrivalTime(double secondsFromNow);
}  // namespace omgtk
