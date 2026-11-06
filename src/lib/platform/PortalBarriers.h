/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#pragma once

#include "base/DirectionTypes.h"

#include <QList>
#include <QRect>

#include <cstdint>

namespace deskflow::platform {

/**
 * @brief A pointer barrier on a free edge inside the desktop's bounding box
 *
 * The portal's rule is the outside of the union of all zones, not of their bounding box, so a notch
 * has an edge a barrier can sit on. The coordinates are inclusive, as the portal counts them.
 */
struct PortalBarrier
{
  Direction side = Direction::Left;
  QRect zone;
  int x1 = 0;
  int y1 = 0;
  int x2 = 0;
  int y2 = 0;
  bool wholeEdge = false; ///< KDE refuses anything less than a display's whole edge

  bool operator==(const PortalBarrier &) const = default;
};

/**
 * @brief The barriers for every free edge that's on an active side
 *
 * Edges on the bounding box are left out, as the portal code already has those. So are seams between
 * zones and edges no pointer can reach from outside, which mirrors what the layout editor offers.
 *
 * @param activeSides The DirectionMask bits of the sides that have a link
 */
QList<PortalBarrier> freeEdgeBarriers(const QList<QRect> &zones, uint32_t activeSides);

/// the desktop the portal reports, which a barrier list can't give: not every display has a barrier
QRect allZones(const QList<QRect> &zones);

} // namespace deskflow::platform
