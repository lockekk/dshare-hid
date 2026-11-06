/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "DisplayEdgesTests.h"

#include "server/DisplayEdges.h"

using namespace deskflow::server;
using enum Direction;

namespace {

// a low laptop on the left and a taller monitor on the right: a notch above the laptop
const QList<QRect> kOffset = {{0, 458, 1512, 982}, {1512, 0, 2560, 1440}};
const QRect kOffsetBox(0, 0, 4072, 1440);
// the same turned over: a notch below the laptop on the right
const QList<QRect> kFlipped = {{0, 0, 2560, 1440}, {2560, 0, 1512, 982}};
const QRect kFlippedBox(0, 0, 4072, 1440);

QList<QRect> s_displays;

QList<QRect> displays()
{
  return s_displays;
}

void use(const QList<QRect> &list)
{
  s_displays = list;
  DisplayEdges::setSource(&displays);
}

const auto kAlways = [](Direction, int32_t, int32_t) { return true; };
const auto kNever = [](Direction, int32_t, int32_t) { return false; };

} // namespace

DisplayEdgesTests::DisplayEdgesTests() = default;

void DisplayEdgesTests::initTestCase()
{
  m_arch.init();
}

void DisplayEdgesTests::cleanup()
{
  DisplayEdges::setRefreshInterval(0);
  DisplayEdges::setSource(nullptr);
}

void DisplayEdgesTests::displayMovedInsideTheBoxIsSeen()
{
  // the laptop moves up beside the monitor, and the box stays the same size
  use(kOffset);
  DisplayEdges::setRefreshInterval(1);
  s_displays = {{0, 0, 1512, 982}, {1512, 0, 2560, 1440}};
  QTest::qWait(5);

  // the old list has the laptop's top edge at y 458, which would throw this to the top of the box
  int32_t x = 700;
  int32_t y = 200;
  DisplayEdges::project(kOffsetBox, x, y, 1, kAlways);
  QCOMPARE(y, 200);
  QCOMPARE(DisplayEdges::displays(), s_displays);
}

void DisplayEdgesTests::refreshWaitsForItsInterval()
{
  use(kOffset);
  DisplayEdges::setRefreshInterval(60000);
  s_displays = {{0, 0, 1512, 982}, {1512, 0, 2560, 1440}};

  int32_t x = 700;
  int32_t y = 200;
  DisplayEdges::project(kOffsetBox, x, y, 1, kNever);
  QCOMPARE(DisplayEdges::displays(), kOffset);
}

void DisplayEdgesTests::refreshIntervalOffLeavesTheListAlone()
{
  use(kOffset);
  s_displays = {{0, 0, 1512, 982}, {1512, 0, 2560, 1440}};
  QTest::qWait(5);

  // macOS and X11 report every change, so they only read the list when told to
  int32_t x = 700;
  int32_t y = 200;
  DisplayEdges::project(kOffsetBox, x, y, 1, kAlways);
  QCOMPARE(y, 0);
  QCOMPARE(DisplayEdges::displays(), kOffset);
}

void DisplayEdgesTests::firstPixelFromEverySide()
{
  const QList<QRect> display = {{10, 20, 100, 50}};
  QCOMPARE(DisplayEdges::firstPixel(display, Left, 30), 10);
  QCOMPARE(DisplayEdges::firstPixel(display, Right, 30), 109);
  QCOMPARE(DisplayEdges::firstPixel(display, Top, 50), 20);
  QCOMPARE(DisplayEdges::firstPixel(display, Bottom, 50), 69);
}

void DisplayEdgesTests::firstPixelOverAGap()
{
  QVERIFY(!DisplayEdges::firstPixel({{0, 0, 100, 100}, {200, 0, 100, 100}}, Top, 150).has_value());
  // the far edge of a display is outside it
  QVERIFY(!DisplayEdges::firstPixel({{0, 0, 100, 100}}, Top, 100).has_value());
  QVERIFY(DisplayEdges::firstPixel({{0, 0, 100, 100}}, Top, 99).has_value());
}

void DisplayEdgesTests::firstPixelPicksTheOutermost()
{
  const QList<QRect> stacked = {{0, 0, 100, 100}, {0, 100, 100, 100}};
  QCOMPARE(DisplayEdges::firstPixel(stacked, Top, 50), 0);
  QCOMPARE(DisplayEdges::firstPixel(stacked, Bottom, 50), 199);

  const QList<QRect> beside = {{0, 0, 100, 100}, {100, 0, 100, 100}};
  QCOMPARE(DisplayEdges::firstPixel(beside, Left, 50), 0);
  QCOMPARE(DisplayEdges::firstPixel(beside, Right, 50), 199);
}

void DisplayEdgesTests::firstPixelEnclosedHoleStaysClosed()
{
  const QList<QRect> cShape = {{0, 0, 3000, 500}, {0, 500, 1000, 500}, {0, 1000, 3000, 500}};
  QCOMPARE(DisplayEdges::firstPixel(cShape, Top, 2000), 0);
  QCOMPARE(DisplayEdges::firstPixel(cShape, Bottom, 2000), 1499);
}

void DisplayEdgesTests::firstPixelEmptyList()
{
  QVERIFY(!DisplayEdges::firstPixel({}, Top, 0).has_value());
}

void DisplayEdgesTests::firstPixelNegativeOrigin()
{
  const QList<QRect> displays = {{-1920, -200, 1920, 1080}, {0, 0, 2560, 1440}};
  QCOMPARE(DisplayEdges::firstPixel(displays, Top, -500), -200);
  QCOMPARE(DisplayEdges::firstPixel(displays, Top, 500), 0);
  QCOMPARE(DisplayEdges::firstPixel(displays, Left, -100), -1920);
}

void DisplayEdgesTests::projectsFromEveryFreeEdge_data()
{
  QTest::addColumn<QList<QRect>>("list");
  QTest::addColumn<QRect>("box");
  QTest::addColumn<QPoint>("from");
  QTest::addColumn<QPoint>("to");
  QTest::newRow("top of the short display") << kOffset << kOffsetBox << QPoint(350, 458) << QPoint(350, 0);
  QTest::newRow("left of the tall display") << kOffset << kOffsetBox << QPoint(1512, 200) << QPoint(0, 200);
  QTest::newRow("bottom of the short display") << kFlipped << kFlippedBox << QPoint(3000, 981) << QPoint(3000, 1439);
  QTest::newRow("right of the tall display") << kFlipped << kFlippedBox << QPoint(2559, 1200) << QPoint(4071, 1200);
}

void DisplayEdgesTests::projectsFromEveryFreeEdge()
{
  QFETCH(QList<QRect>, list);
  QFETCH(QRect, box);
  QFETCH(QPoint, from);
  QFETCH(QPoint, to);
  use(list);
  int32_t x = from.x();
  int32_t y = from.y();
  DisplayEdges::project(box, x, y, 1, kAlways);
  QCOMPARE(QPoint(x, y), to);
}

void DisplayEdgesTests::projectNeedsALink()
{
  use(kOffset);
  int32_t x = 350;
  int32_t y = 458;
  DisplayEdges::project(kOffsetBox, x, y, 1, kNever);
  QCOMPARE(QPoint(x, y), QPoint(350, 458));
}

void DisplayEdgesTests::projectLeavesTheOuterEdge()
{
  use(kOffset);
  // already at the box edge, which the core handles itself
  int32_t x = 4071;
  int32_t y = 700;
  int calls = 0;
  DisplayEdges::project(kOffsetBox, x, y, 1, [&calls](Direction, int32_t, int32_t) {
    ++calls;
    return true;
  });
  QCOMPARE(QPoint(x, y), QPoint(4071, 700));
  QCOMPARE(calls, 0);
}

void DisplayEdgesTests::projectLeavesTheMiddleOfADisplay()
{
  use(kOffset);
  int32_t x = 350;
  int32_t y = 700;
  DisplayEdges::project(kOffsetBox, x, y, 1, kAlways);
  QCOMPARE(QPoint(x, y), QPoint(350, 700));
}

void DisplayEdgesTests::projectUsesTheZone()
{
  use(kOffset);
  int32_t x = 350;
  int32_t y = 460;
  DisplayEdges::project(kOffsetBox, x, y, 3, kAlways);
  QCOMPARE(y, 0);

  y = 461;
  DisplayEdges::project(kOffsetBox, x, y, 3, kAlways);
  QCOMPARE(y, 461);
}

void DisplayEdgesTests::projectBothEdgesInACorner()
{
  use(kOffset);
  // the step between the displays: the top of the short one and the left of the tall one meet here
  int32_t x = 1512;
  int32_t y = 457;
  DisplayEdges::project(kOffsetBox, x, y, 1, kAlways);
  QCOMPARE(QPoint(x, y), QPoint(0, 457));
}

void DisplayEdgesTests::projectLeavesAGap()
{
  use({{0, 0, 100, 100}, {200, 0, 100, 100}});
  int32_t x = 150;
  int32_t y = 50;
  DisplayEdges::project(QRect(0, 0, 300, 100), x, y, 1, kAlways);
  QCOMPARE(QPoint(x, y), QPoint(150, 50));
}

void DisplayEdgesTests::landsOnEveryFreeEdge_data()
{
  QTest::addColumn<QList<QRect>>("list");
  QTest::addColumn<QRect>("box");
  QTest::addColumn<int>("direction");
  QTest::addColumn<QPoint>("from");
  QTest::addColumn<QPoint>("to");
  QTest::newRow("down onto the short display")
      << kOffset << kOffsetBox << static_cast<int>(Bottom) << QPoint(350, 0) << QPoint(350, 459);
  QTest::newRow("right onto the tall display")
      << kOffset << kOffsetBox << static_cast<int>(Right) << QPoint(0, 200) << QPoint(1513, 200);
  QTest::newRow("up onto the short display")
      << kFlipped << kFlippedBox << static_cast<int>(Top) << QPoint(3000, 1439) << QPoint(3000, 980);
  QTest::newRow("left onto the tall display")
      << kFlipped << kFlippedBox << static_cast<int>(Left) << QPoint(4071, 1200) << QPoint(2558, 1200);
}

void DisplayEdgesTests::landsOnEveryFreeEdge()
{
  QFETCH(QList<QRect>, list);
  QFETCH(QRect, box);
  QFETCH(int, direction);
  QFETCH(QPoint, from);
  QFETCH(QPoint, to);
  use(list);
  int32_t x = from.x();
  int32_t y = from.y();
  DisplayEdges::land(box, static_cast<Direction>(direction), 1, x, y);
  QCOMPARE(QPoint(x, y), to);
}

void DisplayEdgesTests::landLeavesTheOuterEdge()
{
  use(kOffset);
  int32_t x = 3000;
  int32_t y = 0;
  DisplayEdges::land(kOffsetBox, Bottom, 1, x, y);
  QCOMPARE(QPoint(x, y), QPoint(3000, 0));
}

void DisplayEdgesTests::landLeavesAGap()
{
  use({{0, 0, 100, 100}, {200, 0, 100, 100}});
  int32_t x = 150;
  int32_t y = 0;
  DisplayEdges::land(QRect(0, 0, 300, 100), Bottom, 1, x, y);
  QCOMPARE(QPoint(x, y), QPoint(150, 0));
}

void DisplayEdgesTests::landLeavesNoDirection()
{
  use(kOffset);
  int32_t x = 350;
  int32_t y = 0;
  DisplayEdges::land(kOffsetBox, NoDirection, 1, x, y);
  QCOMPARE(QPoint(x, y), QPoint(350, 0));
}

void DisplayEdgesTests::landKeepsAPositionAlreadyInside()
{
  use(kOffset);
  int32_t x = 350;
  int32_t y = 900;
  DisplayEdges::land(kOffsetBox, Bottom, 1, x, y);
  QCOMPARE(y, 900);
}

void DisplayEdgesTests::offWithOneDisplay()
{
  use({{0, 0, 1000, 1000}});
  int32_t x = 0;
  int32_t y = 0;
  DisplayEdges::project(QRect(0, 0, 1000, 1000), x, y, 1, kAlways);
  QCOMPARE(QPoint(x, y), QPoint(0, 0));
}

void DisplayEdgesTests::offWithoutASource()
{
  DisplayEdges::setSource(nullptr);
  QVERIFY(DisplayEdges::displays().isEmpty());
  int32_t x = 350;
  int32_t y = 458;
  DisplayEdges::project(kOffsetBox, x, y, 1, kAlways);
  DisplayEdges::land(kOffsetBox, Bottom, 1, x, y);
  QCOMPARE(QPoint(x, y), QPoint(350, 458));
}

void DisplayEdgesTests::offWhenTheDisplaysDontMatchTheBox()
{
  // as a list in other units would be: crossing on it would land in the wrong place
  use({{0, 458, 756, 491}, {756, 0, 1280, 720}});
  int32_t x = 350;
  int32_t y = 458;
  DisplayEdges::project(kOffsetBox, x, y, 1, kAlways);
  QCOMPARE(QPoint(x, y), QPoint(350, 458));
  // it says so once
  DisplayEdges::project(kOffsetBox, x, y, 1, kAlways);
  DisplayEdges::land(kOffsetBox, Bottom, 1, x, y);
  QCOMPARE(QPoint(x, y), QPoint(350, 458));
}

void DisplayEdgesTests::refreshReadsAgain()
{
  use(kOffset);
  QCOMPARE(DisplayEdges::displays(), kOffset);
  s_displays = kFlipped;
  QCOMPARE(DisplayEdges::displays(), kOffset);
  DisplayEdges::refresh();
  QCOMPARE(DisplayEdges::displays(), kFlipped);
}

QTEST_GUILESS_MAIN(DisplayEdgesTests)
