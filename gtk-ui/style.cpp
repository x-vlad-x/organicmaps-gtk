#include "style.hpp"

#include "l10n.hpp"

#include "platform/distance.hpp"

#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace omgtk
{
std::string FormatSize(uint64_t bytes)
{
  double const mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
  std::ostringstream ss;
  if (mb >= 1024.0)
    ss << std::fixed << std::setprecision(1) << mb / 1024.0 << " " << T("gb");
  else if (mb >= 10.0)
    ss << static_cast<int>(std::round(mb)) << " " << T("mb");
  else
    ss << std::fixed << std::setprecision(1) << std::max(mb, 0.1) << " " << T("mb");
  return ss.str();
}

std::string FormatDistance(double meters)
{
  return platform::Distance::CreateFormatted(meters).ToString();
}

std::string FormatDuration(double seconds)
{
  long const total = std::lround(seconds / 60.0);
  long const days = total / (60 * 24);
  long const hours = (total / 60) % 24;
  long const minutes = total % 60;
  std::ostringstream ss;
  if (days > 0)
    ss << days << " " << T("day") << " ";
  if (hours > 0 || days > 0)
    ss << hours << " " << T("hour") << " ";
  if (days == 0)
    ss << minutes << " " << T("minute");
  auto s = ss.str();
  while (!s.empty() && s.back() == ' ')
    s.pop_back();
  return s;
}

std::string FormatArrivalTime(double secondsFromNow)
{
  std::time_t const t = std::time(nullptr) + static_cast<std::time_t>(secondsFromNow);
  char buf[32] = {};
  std::strftime(buf, sizeof(buf), "%H:%M", std::localtime(&t));
  return buf;
}
}  // namespace omgtk
