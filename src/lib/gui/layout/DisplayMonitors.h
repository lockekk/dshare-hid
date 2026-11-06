/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#pragma once

#include <QList>
#include <QPoint>
#include <QRect>
#include <QSize>

#include <optional>

namespace deskflow::gui {

struct ScreenGeometry
{
  QPoint topLeft;
  QSize size;
  qreal devicePixelRatio = 1;
};

/**
 * @brief The server's displays, in the units the core measures its desktop in
 *
 * These come from the same calls the core uses. Where those say nothing, which is Wayland, they come
 * from Qt: it keeps a screen's position in native pixels on Windows and X11 but scales its size, so
 * a display there has to be converted back, while on macOS and Wayland both are already logical.
 */
class DisplayMonitors
{
public:
  enum class Units
  {
    Logical,
    Native
  };

  static Units platformUnits();
  static QRect toCoreUnits(const ScreenGeometry &screen, Units units);

  /// in core units, sorted, with mirrored displays counted once
  static QList<QRect> fromScreens(const QList<ScreenGeometry> &screens, Units units);

  /// the displays now
  static QList<QRect> current();

  /// the displays as Qt sees them, in core units
  static QList<QRect> fromQt();

  /// true where the core can cross from a free edge inside the desktop box, which is everywhere the
  /// displays can be listed: on Wayland that is the portal's job, and its barriers do it
  static bool freeEdges();

  /// the primary display in core units, when Qt knows it
  static std::optional<QRect> primary();

  /// uses this list instead, until cleared
  static void setReported(const QList<QRect> &monitors);
  static void clearReported();
};

} // namespace deskflow::gui
