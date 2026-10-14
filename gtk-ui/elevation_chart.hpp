#pragma once

#include "map/elevation_info.hpp"

#include <adwaita.h>

#include <utility>
#include <vector>

namespace lichen
{
// Polylines of (distance from start, altitude) points; the lines of a multi-line track follow
// one another.
using ElevationLines = std::vector<std::vector<std::pair<double, double>>>;

// Splits a track's elevation profile into its lines.
ElevationLines GetElevationLines(ElevationInfo const & info);

// Altitude profile of a route or a track (like the Android elevation chart).
GtkWidget * MakeElevationChart(ElevationLines lines, int height = 110);
}  // namespace lichen
