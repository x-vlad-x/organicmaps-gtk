#include "elevation_chart.hpp"

#include "style.hpp"

#include "platform/distance.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace lichen
{
namespace
{
struct ChartData
{
  // Polylines (distance from start, altitude), lines of a multi-line track are drawn one after another.
  std::vector<std::vector<std::pair<double, double>>> m_lines;
  double m_maxDist = 0;
  double m_minAlt = 0;
  double m_maxAlt = 0;
};

void Draw(GtkDrawingArea * area, cairo_t * cr, int width, int height, gpointer data)
{
  auto const & d = *static_cast<ChartData *>(data);
  if (d.m_lines.empty() || d.m_maxDist <= 0)
    return;

  GdkRGBA fg;
  gtk_widget_get_color(GTK_WIDGET(area), &fg);
  GdkRGBA * accentPtr = adw_style_manager_get_accent_color_rgba(adw_style_manager_get_default());
  GdkRGBA const accent = *accentPtr;
  gdk_rgba_free(accentPtr);

  double const labelW = 52, padTop = 8, padBottom = 18;
  double const plotW = width - labelW - 4;
  double const plotH = height - padTop - padBottom;
  double range = d.m_maxAlt - d.m_minAlt;
  double minAlt = d.m_minAlt;
  if (range < 20)
  {
    minAlt -= (20 - range) / 2;
    range = 20;
  }
  auto const X = [&](double dist) { return labelW + dist / d.m_maxDist * plotW; };
  auto const Y = [&](double alt) { return padTop + (1.0 - (alt - minAlt) / range) * plotH; };

  // Grid and altitude labels.
  cairo_set_line_width(cr, 1);
  cairo_set_source_rgba(cr, fg.red, fg.green, fg.blue, 0.15);
  for (int i = 0; i <= 2; ++i)
  {
    double const y = padTop + plotH * i / 2.0;
    cairo_move_to(cr, labelW, y);
    cairo_line_to(cr, labelW + plotW, y);
  }
  cairo_stroke(cr);

  auto * layout = gtk_widget_create_pango_layout(GTK_WIDGET(area), nullptr);
  auto * font = pango_font_description_from_string("Sans 8");
  pango_layout_set_font_description(layout, font);
  pango_font_description_free(font);
  auto const text = [&](std::string const & s, double x, double y, bool right)
  {
    pango_layout_set_text(layout, s.c_str(), -1);
    int tw = 0, th = 0;
    pango_layout_get_pixel_size(layout, &tw, &th);
    cairo_move_to(cr, right ? x - tw : x, y - th / 2.0);
    pango_cairo_show_layout(cr, layout);
  };
  cairo_set_source_rgba(cr, fg.red, fg.green, fg.blue, 0.7);
  text(platform::Distance::FormatAltitude(minAlt + range), labelW - 6, padTop, true);
  text(platform::Distance::FormatAltitude(minAlt), labelW - 6, padTop + plotH, true);
  text("0", labelW, height - padBottom / 2.0 + 2, false);
  text(FormatDistance(d.m_maxDist), labelW + plotW, height - padBottom / 2.0 + 2, true);
  g_object_unref(layout);

  // Filled profile.
  for (auto const & line : d.m_lines)
  {
    if (line.size() < 2)
      continue;
    cairo_move_to(cr, X(line.front().first), padTop + plotH);
    for (auto const & [dist, alt] : line)
      cairo_line_to(cr, X(dist), Y(alt));
    cairo_line_to(cr, X(line.back().first), padTop + plotH);
    cairo_close_path(cr);
    cairo_set_source_rgba(cr, accent.red, accent.green, accent.blue, 0.25);
    cairo_fill(cr);

    cairo_move_to(cr, X(line.front().first), Y(line.front().second));
    for (auto const & [dist, alt] : line)
      cairo_line_to(cr, X(dist), Y(alt));
    cairo_set_source_rgba(cr, accent.red, accent.green, accent.blue, 1.0);
    cairo_set_line_width(cr, 2);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_stroke(cr);
  }
}
}  // namespace

GtkWidget * MakeElevationChart(ElevationInfo const & info, int height)
{
  auto * data = new ChartData;
  data->m_minAlt = std::numeric_limits<double>::max();
  data->m_maxAlt = std::numeric_limits<double>::lowest();
  for (auto const & line : info.GetLines())
  {
    std::vector<std::pair<double, double>> pts;
    pts.reserve(line.size());
    for (auto const & p : line)
    {
      pts.emplace_back(p.m_distance, p.m_altitude);
      data->m_maxDist = std::max(data->m_maxDist, p.m_distance);
      data->m_minAlt = std::min<double>(data->m_minAlt, p.m_altitude);
      data->m_maxAlt = std::max<double>(data->m_maxAlt, p.m_altitude);
    }
    data->m_lines.push_back(std::move(pts));
  }

  auto * area = gtk_drawing_area_new();
  gtk_widget_set_size_request(area, -1, height);
  gtk_widget_set_hexpand(area, TRUE);
  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(area), &Draw, data,
                                 [](gpointer d) { delete static_cast<ChartData *>(d); });
  return area;
}
}  // namespace lichen
