#include "location.hpp"

#include "app.hpp"

#include "base/logging.hpp"

#include <geoclue.h>

#include <cmath>

namespace lichen
{
LocationService::LocationService() = default;

LocationService::~LocationService()
{
  Stop();
}

void LocationService::Start()
{
  if (m_started)
    return;
  m_started = true;
  m_cancellable = g_cancellable_new();
  gclue_simple_new_with_thresholds(kAppId, GCLUE_ACCURACY_LEVEL_EXACT, 0 /* time */, 0 /* distance */,
                                   m_cancellable, &LocationService::OnSimpleReady, this);
  StartCompass();
}

void LocationService::Stop()
{
  if (!m_started)
    return;
  m_started = false;
  if (m_cancellable)
  {
    g_cancellable_cancel(m_cancellable);
    g_clear_object(&m_cancellable);
  }
  if (m_simple)
  {
    g_signal_handlers_disconnect_by_data(m_simple, this);
    g_clear_object(&m_simple);
  }
  StopCompass();
}

void LocationService::OnSimpleReady(GObject *, GAsyncResult * res, gpointer data)
{
  GError * error = nullptr;
  GClueSimple * simple = gclue_simple_new_finish(res, &error);
  if (!simple)
  {
    bool const cancelled = g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
    if (!cancelled)
    {
      auto * self = static_cast<LocationService *>(data);
      LOG(LWARNING, ("GeoClue is not available:", error->message));
      bool const denied = g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_ACCESS_DENIED);
      if (self->m_onError)
        self->m_onError(denied ? location::EDenied : location::ENotSupported);
    }
    g_clear_error(&error);
    return;
  }
  auto * self = static_cast<LocationService *>(data);
  self->m_simple = simple;
  g_signal_connect(simple, "notify::location", G_CALLBACK(&LocationService::OnLocationNotify), self);
  self->ReadLocation();
}

void LocationService::OnLocationNotify(GObject *, GParamSpec *, gpointer data)
{
  static_cast<LocationService *>(data)->ReadLocation();
}

void LocationService::ReadLocation()
{
  if (!m_simple)
    return;
  GClueLocation * loc = gclue_simple_get_location(m_simple);
  if (!loc)
    return;

  location::GpsInfo info;
  info.m_source = location::EGeoClue2;
  info.m_latitude = gclue_location_get_latitude(loc);
  info.m_longitude = gclue_location_get_longitude(loc);
  info.m_horizontalAccuracy = gclue_location_get_accuracy(loc);
  double const altitude = gclue_location_get_altitude(loc);
  if (altitude != -G_MAXDOUBLE)
  {
    info.m_altitude = altitude;
    info.m_verticalAccuracy = info.m_horizontalAccuracy;
  }
  double const speed = gclue_location_get_speed(loc);
  if (speed >= 0)
    info.m_speed = speed;
  double const heading = gclue_location_get_heading(loc);
  // GeoClue reports the compass heading when not moving; only trust it as a bearing while moving.
  if (heading >= 0 && speed > 0.5)
    info.m_bearing = heading;

  info.m_timestamp = static_cast<double>(g_get_real_time()) / 1e6;
  if (GVariant * ts = gclue_location_get_timestamp(loc))
  {
    guint64 sec = 0, usec = 0;
    g_variant_get(ts, "(tt)", &sec, &usec);
    if (sec > 0)
      info.m_timestamp = static_cast<double>(sec) + static_cast<double>(usec) / 1e6;
  }

  if (info.m_horizontalAccuracy <= 0)
    info.m_horizontalAccuracy = 100;

  m_last = info;
  if (m_onLocation)
    m_onLocation(info);
}

void LocationService::StartCompass()
{
  GError * error = nullptr;
  m_compass = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, nullptr,
                                            "net.hadess.SensorProxy", "/net/hadess/SensorProxy/Compass",
                                            "net.hadess.SensorProxy.Compass", nullptr, &error);
  if (!m_compass)
  {
    LOG(LINFO, ("No compass:", error ? error->message : ""));
    g_clear_error(&error);
    return;
  }
  GVariant * has = g_dbus_proxy_get_cached_property(m_compass, "HasCompass");
  bool const hasCompass = has && g_variant_get_boolean(has);
  if (has)
    g_variant_unref(has);
  if (!hasCompass)
  {
    g_clear_object(&m_compass);
    return;
  }
  GVariant * r = g_dbus_proxy_call_sync(m_compass, "ClaimCompass", nullptr, G_DBUS_CALL_FLAGS_NONE, 3000, nullptr,
                                        &error);
  if (!r)
  {
    LOG(LINFO, ("ClaimCompass failed:", error ? error->message : ""));
    g_clear_error(&error);
    g_clear_object(&m_compass);
    return;
  }
  g_variant_unref(r);
  g_signal_connect(m_compass, "g-properties-changed", G_CALLBACK(&LocationService::OnCompassProperties), this);
}

void LocationService::StopCompass()
{
  if (!m_compass)
    return;
  g_signal_handlers_disconnect_by_data(m_compass, this);
  g_dbus_proxy_call(m_compass, "ReleaseCompass", nullptr, G_DBUS_CALL_FLAGS_NONE, 1000, nullptr, nullptr, nullptr);
  g_clear_object(&m_compass);
}

void LocationService::OnCompassProperties(GDBusProxy *, GVariant * changed, GStrv, gpointer data)
{
  auto * self = static_cast<LocationService *>(data);
  double heading = -1;
  if (!g_variant_lookup(changed, "CompassHeading", "d", &heading) || heading < 0)
    return;
  if (self->m_onCompass)
    self->m_onCompass(heading * M_PI / 180.0);
}
}  // namespace lichen
