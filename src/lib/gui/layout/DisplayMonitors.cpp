/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "DisplayMonitors.h"

#include "common/PlatformInfo.h"
#include "platform/DisplayRects.h"

#include <QGuiApplication>
#include <QScreen>

namespace deskflow::gui {

namespace {

std::optional<QList<QRect>> &reported()
{
  static std::optional<QList<QRect>> monitors;
  return monitors;
}

ScreenGeometry geometryOf(const QScreen *screen)
{
  return {screen->geometry().topLeft(), screen->geometry().size(), screen->devicePixelRatio()};
}

} // namespace

DisplayMonitors::Units DisplayMonitors::platformUnits()
{
#if defined(Q_OS_MACOS)
  return Units::Logical;
#else
  return deskflow::platform::isWayland() ? Units::Logical : Units::Native;
#endif
}

QRect DisplayMonitors::toCoreUnits(const ScreenGeometry &screen, Units units)
{
  if (units == Units::Logical)
    return {screen.topLeft, screen.size};
  return {screen.topLeft, (QSizeF(screen.size) * screen.devicePixelRatio).toSize()};
}

QList<QRect> DisplayMonitors::fromScreens(const QList<ScreenGeometry> &screens, Units units)
{
  QList<QRect> monitors;
  for (const auto &screen : screens)
    monitors.append(toCoreUnits(screen, units));
  return deskflow::platform::sortedDisplayRects(monitors);
}

QList<QRect> DisplayMonitors::fromQt()
{
  QList<ScreenGeometry> screens;
  const auto all = QGuiApplication::screens();
  for (const auto *screen : all)
    screens.append(geometryOf(screen));
  return fromScreens(screens, platformUnits());
}

QList<QRect> DisplayMonitors::current()
{
  if (reported().has_value())
    return *reported();

  const auto monitors = deskflow::platform::displayRects();
  return monitors.isEmpty() ? fromQt() : monitors;
}

bool DisplayMonitors::freeEdges()
{
  return reported().has_value() || deskflow::platform::isWayland() || !deskflow::platform::displayRects().isEmpty();
}

std::optional<QRect> DisplayMonitors::primary()
{
  const auto *screen = QGuiApplication::primaryScreen();
  if (screen == nullptr)
    return std::nullopt;
  return toCoreUnits(geometryOf(screen), platformUnits());
}

void DisplayMonitors::setReported(const QList<QRect> &monitors)
{
  reported() = monitors;
}

void DisplayMonitors::clearReported()
{
  reported().reset();
}

} // namespace deskflow::gui
