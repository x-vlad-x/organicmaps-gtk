#pragma once

#include "map/elevation_info.hpp"

#include <adwaita.h>

namespace lichen
{
// Altitude profile of a route or a track (like the Android elevation chart).
GtkWidget * MakeElevationChart(ElevationInfo const & info, int height = 110);
}  // namespace lichen
