#pragma once

#include "platform/location.hpp"

#include <gio/gio.h>

#include <functional>
#include <optional>

typedef struct _GClueSimple GClueSimple;

namespace lichen
{
// GPS through GeoClue (on FuriOS it uses the hybris GNSS source) and the magnetometer through
// iio-sensor-proxy. Updates are delivered to the Framework on the main thread.
class LocationService
{
public:
  LocationService();
  ~LocationService();

  void Start();
  void Stop();
  bool IsStarted() const { return m_started; }

  std::optional<location::GpsInfo> const & LastLocation() const { return m_last; }
  // Feeds a location as if it came from GeoClue (debug/testing).
  void InjectForTests(location::GpsInfo const & info)
  {
    m_last = info;
    if (m_onLocation)
      m_onLocation(info);
  }

  std::function<void(location::GpsInfo const &)> m_onLocation;
  std::function<void(location::TLocationError)> m_onError;
  std::function<void(double bearingRad)> m_onCompass;

private:
  static void OnSimpleReady(GObject * source, GAsyncResult * res, gpointer data);
  static void OnLocationNotify(GObject * obj, GParamSpec * pspec, gpointer data);
  void ReadLocation();
  void StartCompass();
  void StopCompass();
  static void OnCompassProperties(GDBusProxy * proxy, GVariant * changed, GStrv invalidated, gpointer data);

  bool m_started = false;
  GCancellable * m_cancellable = nullptr;
  GClueSimple * m_simple = nullptr;
  GDBusProxy * m_compass = nullptr;
  std::optional<location::GpsInfo> m_last;
};
}  // namespace lichen
