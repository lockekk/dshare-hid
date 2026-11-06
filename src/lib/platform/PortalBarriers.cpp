/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "PortalBarriers.h"

#include <algorithm>
#include <array>

namespace deskflow::platform {

namespace {

using enum Direction;

struct Side
{
  Direction direction;
  DirectionMask mask;
};

constexpr std::array kSides = {
    Side{Left, DirectionMask::LeftMask}, Side{Right, DirectionMask::RightMask}, Side{Top, DirectionMask::TopMask},
    Side{Bottom, DirectionMask::BottomMask}
};

bool runsDown(Direction side)
{
  return side == Left || side == Right;
}

int spanStart(const QRect &zone, Direction side)
{
  return runsDown(side) ? zone.y() : zone.x();
}

int spanEnd(const QRect &zone, Direction side)
{
  return spanStart(zone, side) + (runsDown(side) ? zone.height() : zone.width());
}

/// where a side of a zone lies
int edgeOf(const QRect &zone, Direction side)
{
  const int start = runsDown(side) ? zone.x() : zone.y();
  const int length = runsDown(side) ? zone.width() : zone.height();
  return side == Left || side == Top ? start : start + length;
}

/// a side's position as a number that grows outward
int reachOf(const QRect &zone, Direction side)
{
  return side == Left || side == Top ? -edgeOf(zone, side) : edgeOf(zone, side);
}

/// the stretches of a zone's side with no other zone further out
QList<std::pair<int, int>> freeStretches(const QList<QRect> &zones, const QRect &zone, Direction side)
{
  QList<int> cuts = {spanStart(zone, side), spanEnd(zone, side)};
  for (const auto &other : zones) {
    cuts << std::clamp(spanStart(other, side), cuts.at(0), cuts.at(1))
         << std::clamp(spanEnd(other, side), cuts.at(0), cuts.at(1));
  }
  std::ranges::sort(cuts);
  cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());

  QList<std::pair<int, int>> stretches;
  for (qsizetype i = 1; i < cuts.size(); ++i) {
    const int start = cuts.at(i - 1);
    const int end = cuts.at(i);
    const bool covered = std::ranges::any_of(zones, [&](const QRect &other) {
      return other != zone && spanStart(other, side) <= start && spanEnd(other, side) >= end &&
             reachOf(other, side) > reachOf(zone, side);
    });
    if (covered)
      continue;
    if (!stretches.isEmpty() && stretches.last().second == start)
      stretches.last().second = end;
    else
      stretches.append({start, end});
  }
  return stretches;
}

PortalBarrier makeBarrier(const QRect &zone, Direction side, int start, int end)
{
  PortalBarrier barrier;
  barrier.side = side;
  barrier.zone = zone;
  barrier.wholeEdge = start == spanStart(zone, side) && end == spanEnd(zone, side);

  // a barrier sits on the top or left edge of its pixels, so right and bottom are one past the zone
  const int at = edgeOf(zone, side);
  if (runsDown(side)) {
    barrier.x1 = at;
    barrier.x2 = at;
    barrier.y1 = start;
    barrier.y2 = end - 1;
  } else {
    barrier.x1 = start;
    barrier.x2 = end - 1;
    barrier.y1 = at;
    barrier.y2 = at;
  }
  return barrier;
}

} // namespace

QList<PortalBarrier> freeEdgeBarriers(const QList<QRect> &zones, uint32_t activeSides)
{
  QRect box;
  for (const auto &zone : zones)
    box = box.isNull() ? zone : box.united(zone);

  QList<PortalBarrier> barriers;
  for (const auto &side : kSides) {
    if ((activeSides & static_cast<uint32_t>(side.mask)) == 0)
      continue;
    for (const auto &zone : zones) {
      // the portal code already covers the edges on the bounding box
      if (reachOf(zone, side.direction) == reachOf(box, side.direction))
        continue;
      const auto stretches = freeStretches(zones, zone, side.direction);
      for (const auto &[start, end] : stretches)
        barriers.append(makeBarrier(zone, side.direction, start, end));
    }
  }
  return barriers;
}

QRect allZones(const QList<QRect> &zones)
{
  QRect united;
  for (const auto &zone : zones)
    united = united.isNull() ? zone : united.united(zone);
  return united;
}

} // namespace deskflow::platform
