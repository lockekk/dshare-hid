/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#pragma once

#include "base/DirectionTypes.h"

#include <QList>
#include <QRect>

#include <functional>
#include <optional>

namespace deskflow::server {

/**
 * @brief Lets a free display edge inside the desktop box cross to a client
 *
 * The core sees the server as one box and switches only at the edge of it, so a link to the top of a
 * short display beside a taller one never fires: the cursor stops at that display and can't reach
 * the top of the box. Moving the cursor's position to the box edge makes the core's own link logic
 * apply, and coming back it lands on a real display just inside that edge.
 *
 * All of this is off unless the displays add up to the box exactly, so a list in other units can't
 * make the cursor cross in the wrong place.
 */
class DisplayEdges
{
public:
  using Source = QList<QRect> (*)();
  using Linked = std::function<bool(Direction side, int32_t x, int32_t y)>;

  /// where the displays come from, which the core sets because this library can't reach the platform
  static void setSource(Source source);

  /// reads the displays again, for when they change
  static void refresh();

  /// how often project() looks for itself, 0 to rely on refresh(). Windows only reports a change of
  /// the whole box, so a display moved inside the same box would go unseen
  static void setRefreshInterval(int milliseconds);

  static const QList<QRect> &displays();

  /// the first display pixel seen from outside at a row or column, none over a gap
  static std::optional<int32_t> firstPixel(const QList<QRect> &displays, Direction side, int32_t along);

  /// moves a position on a free edge with a link to the box edge
  static void project(const QRect &box, int32_t &x, int32_t &y, int32_t zone, const Linked &linked);

  /// moves a position coming in from a client onto a display, past the jump zone of a free edge
  static void land(const QRect &box, Direction side, int32_t zone, int32_t &x, int32_t &y);
};

} // namespace deskflow::server
