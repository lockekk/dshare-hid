/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "PortalBarrierTests.h"

#include "platform/PortalBarriers.h"

using namespace deskflow::platform;
using enum Direction;

namespace {

// a low laptop on the left and a taller monitor on the right, leaving a notch above the laptop
const QRect kLaptop(0, 458, 1512, 982);
const QRect kMonitor(1512, 0, 2560, 1440);
const QList<QRect> kOffset = {kLaptop, kMonitor};

constexpr auto mask(DirectionMask side)
{
  return static_cast<uint32_t>(side);
}
constexpr uint32_t kAll = mask(DirectionMask::LeftMask) | mask(DirectionMask::RightMask) |
                          mask(DirectionMask::TopMask) | mask(DirectionMask::BottomMask);

QList<PortalBarrier> onSide(const QList<PortalBarrier> &barriers, Direction side)
{
  QList<PortalBarrier> found;
  for (const auto &barrier : barriers) {
    if (barrier.side == side)
      found.append(barrier);
  }
  return found;
}

} // namespace

void PortalBarrierTests::noZones()
{
  QVERIFY(freeEdgeBarriers({}, kAll).isEmpty());
}

void PortalBarrierTests::oneZoneHasNoFreeEdges()
{
  QVERIFY(freeEdgeBarriers({{0, 0, 1920, 1080}}, kAll).isEmpty());
}

void PortalBarrierTests::sideBySideHaveNoFreeEdges()
{
  QVERIFY(freeEdgeBarriers({{0, 0, 1920, 1080}, {1920, 0, 1920, 1080}}, kAll).isEmpty());
}

void PortalBarrierTests::topOfAShortDisplay()
{
  const auto barriers = onSide(freeEdgeBarriers(kOffset, kAll), Top);
  QCOMPARE(barriers.size(), 1);
  QCOMPARE(barriers.first().zone, kLaptop);
  QCOMPARE(barriers.first().y1, 458);
  QCOMPARE(barriers.first().y2, 458);
  QCOMPARE(barriers.first().x1, 0);
  QCOMPARE(barriers.first().x2, 1511);
  QVERIFY(barriers.first().wholeEdge);
}

void PortalBarrierTests::partOfATallDisplaysSide()
{
  const auto barriers = onSide(freeEdgeBarriers(kOffset, kAll), Left);
  QCOMPARE(barriers.size(), 1);
  QCOMPARE(barriers.first().zone, kMonitor);
  QCOMPARE(barriers.first().x1, 1512);
  QCOMPARE(barriers.first().x2, 1512);
  QCOMPARE(barriers.first().y1, 0);
  QCOMPARE(barriers.first().y2, 457);
  QVERIFY(!barriers.first().wholeEdge);
}

void PortalBarrierTests::everySide()
{
  // a small display centred on top of a wide one has a free stretch either side of it, and its own
  // sides are on the box only at the top
  const QList<QRect> zones = {{1000, 0, 1000, 500}, {0, 500, 3000, 500}};
  const auto barriers = freeEdgeBarriers(zones, kAll);
  QCOMPARE(onSide(barriers, Left).size(), 1);
  QCOMPARE(onSide(barriers, Right).size(), 1);
  QCOMPARE(onSide(barriers, Top).size(), 2);
  QCOMPARE(onSide(barriers, Bottom).size(), 0);

  const auto flipped = freeEdgeBarriers({{1000, 500, 1000, 500}, {0, 0, 3000, 500}}, kAll);
  QCOMPARE(onSide(flipped, Bottom).size(), 2);
  QCOMPARE(onSide(flipped, Top).size(), 0);
}

void PortalBarrierTests::inclusiveCoordinates()
{
  const QList<QRect> zones = {{1000, 0, 1000, 500}, {0, 500, 3000, 500}};
  const auto top = onSide(freeEdgeBarriers(zones, kAll), Top);
  QCOMPARE(top.at(0).x1, 0);
  QCOMPARE(top.at(0).x2, 999);
  QCOMPARE(top.at(1).x1, 2000);
  QCOMPARE(top.at(1).x2, 2999);

  // right and bottom barriers sit one past the zone's last pixel
  const auto right = onSide(freeEdgeBarriers(zones, kAll), Right);
  QCOMPARE(right.first().x1, 2000);
  QCOMPARE(right.first().y2, 499);

  const auto bottom = onSide(freeEdgeBarriers({{1000, 500, 1000, 500}, {0, 0, 3000, 500}}, kAll), Bottom);
  QCOMPARE(bottom.first().y1, 500);
}

void PortalBarrierTests::onlyActiveSides()
{
  QVERIFY(freeEdgeBarriers(kOffset, 0).isEmpty());
  const auto barriers = freeEdgeBarriers(kOffset, mask(DirectionMask::TopMask));
  QCOMPARE(barriers.size(), 1);
  QCOMPARE(barriers.first().side, Top);
}

void PortalBarrierTests::gapsAreLeftOut()
{
  // a gap in the union isn't a zone, so the barrier stops where the zone does
  const QList<QRect> zones = {{0, 200, 500, 300}, {1000, 0, 500, 500}};
  const auto top = onSide(freeEdgeBarriers(zones, kAll), Top);
  QCOMPARE(top.size(), 1);
  QCOMPARE(top.first().x2, 499);
}

void PortalBarrierTests::seamsAreLeftOut()
{
  // the laptop's right edge is a seam with the monitor all the way along
  const auto barriers = freeEdgeBarriers(kOffset, kAll);
  QVERIFY(onSide(barriers, Right).isEmpty());
  for (const auto &barrier : barriers) {
    if (barrier.side == Left) {
      QVERIFY(barrier.y2 < kLaptop.y());
    }
  }
}

void PortalBarrierTests::enclosedHoleStaysClosed()
{
  const QList<QRect> cShape = {{0, 0, 3000, 500}, {0, 500, 1000, 500}, {0, 1000, 3000, 500}};
  const auto barriers = freeEdgeBarriers(cShape, kAll);
  QVERIFY(onSide(barriers, Top).isEmpty());
  QVERIFY(onSide(barriers, Bottom).isEmpty());
  // the hole opens to the right, so the spine has a free edge there
  QCOMPARE(onSide(barriers, Right).size(), 1);
  QCOMPARE(onSide(barriers, Right).first().x1, 1000);
}

void PortalBarrierTests::aZoneBelowDoesntSplitAnEdge()
{
  // the display under this one marks out part of the edge, which stays one barrier
  const QList<QRect> zones = {{0, 100, 1000, 100}, {200, 200, 200, 100}, {1000, 0, 100, 300}};
  const auto top = onSide(freeEdgeBarriers(zones, kAll), Top);
  QCOMPARE(top.size(), 1);
  QCOMPARE(top.first().x1, 0);
  QCOMPARE(top.first().x2, 999);
  QVERIFY(top.first().wholeEdge);
}

void PortalBarrierTests::negativeOrigins()
{
  const QList<QRect> zones = {{-1000, 300, 1000, 500}, {0, 0, 2000, 1000}};
  const auto top = onSide(freeEdgeBarriers(zones, kAll), Top);
  QCOMPARE(top.size(), 1);
  QCOMPARE(top.first().x1, -1000);
  QCOMPARE(top.first().x2, -1);
  QCOMPARE(top.first().y1, 300);
}

void PortalBarrierTests::kdeOnlyTakesWholeEdges()
{
  // KDE refuses a part of an edge, and the portal code drops just that barrier
  const auto barriers = freeEdgeBarriers(kOffset, kAll);
  QCOMPARE(onSide(barriers, Top).first().wholeEdge, true);
  QCOMPARE(onSide(barriers, Left).first().wholeEdge, false);
}

void PortalBarrierTests::fitsGnomesRules()
{
  // mutter: a horizontal barrier may not reach the left edge of the next monitor, so it has to stop
  // one short of it, which inclusive coordinates do
  const auto barrier = onSide(freeEdgeBarriers(kOffset, kAll), Top).first();
  QVERIFY(barrier.x2 < kMonitor.x());
}

void PortalBarrierTests::allZonesIsTheWholeDesktop()
{
  QCOMPARE(allZones({}), QRect());
  QCOMPARE(allZones({{0, 0, 2560, 1440}}), QRect(0, 0, 2560, 1440));

  // a short display beside a tall one, which is all the barriers would show if the tall one had none
  QCOMPARE(allZones({{0, 0, 2560, 1440}, {2560, 0, 1512, 982}}), QRect(0, 0, 4072, 1440));
  QCOMPARE(allZones({{-1920, 0, 1920, 1080}, {0, 0, 3840, 2160}}), QRect(-1920, 0, 5760, 2160));
}

QTEST_GUILESS_MAIN(PortalBarrierTests)
