/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "DisplayLayoutTests.h"

#include "gui/layout/DisplayLayout.h"

#include <QRandomGenerator>

#include <clocale>

using namespace deskflow::gui;
using enum LayoutSide;

namespace {

// a low laptop on the left and a taller monitor on the right, leaving a notch above the laptop
const QRect kLaptop(0, 458, 1512, 982);
const QRect kMonitor(1512, 0, 2560, 1440);
const QList<QRect> kOffset = {kLaptop, kMonitor};
const QList<QRect> kSingle = {{0, 0, 1000, 1000}};
const QList<QRect> kThree = {{0, 0, 1080, 1920}, {1080, 240, 2560, 1440}, {3640, 420, 1920, 1080}};
// two wide displays joined at the left, which closes a hole on the right
const QList<QRect> kCShape = {{0, 0, 3000, 500}, {0, 500, 1000, 500}, {0, 1000, 3000, 500}};

LayoutClient client(const QString &name, int width, int height, std::optional<LayoutPlacement> placement = std::nullopt)
{
  return {name, QSize(width, height), true, false, placement};
}

LayoutPlacement on(LayoutSide side, int offset = 0, const QString &anchor = {}, bool whole = false)
{
  return {anchor, side, offset, whole};
}

LayoutState state(const QList<QRect> &monitors, const QList<LayoutClient> &clients, bool freeEdges = false)
{
  return {QStringLiteral("srv"), monitors, clients, freeEdges};
}

QList<LayoutLink> linksOf(const LayoutState &layout, const QString &name)
{
  const auto solved = DisplayLayout::solve(layout);
  const auto blocks = DisplayLayout::links(layout, solved);
  for (const auto &block : blocks) {
    if (block.source == name)
      return block.links;
  }
  return {};
}

/// what the core needs of any layout: it refuses the whole config otherwise
bool valid(const LayoutState &layout, QString &problem)
{
  const auto solved = DisplayLayout::solve(layout);
  const auto blocks = DisplayLayout::links(layout, solved);
  QHash<QString, QList<LayoutLink>> bySource;
  for (const auto &block : blocks)
    bySource.insert(block.source, block.links);

  for (const auto &block : blocks) {
    QList<LayoutRange> perSide[4];
    for (const auto &link : block.links) {
      for (const auto &range : {link.range, link.targetRange}) {
        if (range.start < 0 || range.end > 100 || range.start >= range.end) {
          problem = QStringLiteral("%1 has a bad range").arg(block.source);
          return false;
        }
      }
      perSide[static_cast<int>(link.side)].append(link.range);
      const auto back = bySource.value(link.target);
      if (std::ranges::none_of(back, [&block](const LayoutLink &other) { return other.target == block.source; })) {
        problem = QStringLiteral("%1 has no way back from %2").arg(block.source, link.target);
        return false;
      }
    }
    for (const auto &ranges : perSide) {
      for (qsizetype i = 1; i < ranges.size(); ++i) {
        if (ranges.at(i).start < ranges.at(i - 1).end) {
          problem = QStringLiteral("%1 has overlapping ranges").arg(block.source);
          return false;
        }
      }
    }
  }

  const auto rects = solved.rects.values();
  for (qsizetype i = 0; i < rects.size(); ++i) {
    for (qsizetype j = i + 1; j < rects.size(); ++j) {
      if (rects.at(i).intersects(rects.at(j))) {
        problem = QStringLiteral("two clients overlap");
        return false;
      }
    }
    const auto blocked = layout.freeEdges ? layout.monitors : QList<QRect>{solved.box};
    if (std::ranges::any_of(blocked, [&](const QRect &rect) { return rect.intersects(rects.at(i)); })) {
      problem = QStringLiteral("a client overlaps a display");
      return false;
    }
  }
  return true;
}

} // namespace

void DisplayLayoutTests::reachSingleDisplay()
{
  const auto box = DisplayLayout::boundingBox(kSingle);
  QCOMPARE(box, QRect(0, 0, 1000, 1000));
  QCOMPARE(DisplayLayout::reach(kSingle, box, Left, false), QList<LayoutSegment>({{0, 1000, 0}}));
  QCOMPARE(DisplayLayout::reach(kSingle, box, Right, false), QList<LayoutSegment>({{0, 1000, 1000}}));
  QCOMPARE(DisplayLayout::reach(kSingle, box, Up, false), QList<LayoutSegment>({{0, 1000, 0}}));
  QCOMPARE(DisplayLayout::reach(kSingle, box, Down, false), QList<LayoutSegment>({{0, 1000, 1000}}));
}

void DisplayLayoutTests::reachSideBySide()
{
  const QList<QRect> monitors = {{0, 0, 1000, 500}, {1000, 0, 1000, 500}};
  const auto box = DisplayLayout::boundingBox(monitors);
  // the top runs across both, and the seam between them isn't an edge
  QCOMPARE(DisplayLayout::reach(monitors, box, Up, true), QList<LayoutSegment>({{0, 2000, 0}}));
  QCOMPARE(DisplayLayout::reach(monitors, box, Left, true), QList<LayoutSegment>({{0, 500, 0}}));
  QCOMPARE(DisplayLayout::reach(monitors, box, Right, true), QList<LayoutSegment>({{0, 500, 2000}}));
}

void DisplayLayoutTests::reachOffsetOuterEdgesOnly()
{
  const auto box = DisplayLayout::boundingBox(kOffset);
  QCOMPARE(box, QRect(0, 0, 4072, 1440));
  QCOMPARE(DisplayLayout::reach(kOffset, box, Up, false), QList<LayoutSegment>({{1512, 4072, 0}}));
  QCOMPARE(DisplayLayout::reach(kOffset, box, Left, false), QList<LayoutSegment>({{458, 1440, 0}}));
  QCOMPARE(DisplayLayout::reach(kOffset, box, Down, false), QList<LayoutSegment>({{0, 4072, 1440}}));
}

void DisplayLayoutTests::reachOffsetFreeEdges()
{
  const auto box = DisplayLayout::boundingBox(kOffset);
  QCOMPARE(DisplayLayout::reach(kOffset, box, Up, true), QList<LayoutSegment>({{0, 1512, 458}, {1512, 4072, 0}}));
  QCOMPARE(DisplayLayout::reach(kOffset, box, Left, true), QList<LayoutSegment>({{0, 458, 1512}, {458, 1440, 0}}));
}

void DisplayLayoutTests::reachStacked()
{
  const QList<QRect> monitors = {{0, 0, 2000, 500}, {500, 500, 1000, 500}};
  const auto box = DisplayLayout::boundingBox(monitors);
  QCOMPARE(
      DisplayLayout::reach(monitors, box, Down, true),
      QList<LayoutSegment>({{0, 500, 500}, {500, 1500, 1000}, {1500, 2000, 500}})
  );
  QCOMPARE(DisplayLayout::reach(monitors, box, Down, false), QList<LayoutSegment>({{500, 1500, 1000}}));
}

void DisplayLayoutTests::reachThreeDisplays()
{
  const auto box = DisplayLayout::boundingBox(kThree);
  QCOMPARE(DisplayLayout::reach(kThree, box, Right, false), QList<LayoutSegment>({{420, 1500, 5560}}));
  QCOMPARE(DisplayLayout::reach(kThree, box, Up, false), QList<LayoutSegment>({{0, 1080, 0}}));
  QCOMPARE(
      DisplayLayout::reach(kThree, box, Up, true),
      QList<LayoutSegment>({{0, 1080, 0}, {1080, 3640, 240}, {3640, 5560, 420}})
  );
}

void DisplayLayoutTests::reachMirrored()
{
  const QList<QRect> monitors = {{0, 0, 1000, 500}, {0, 0, 1000, 500}};
  const auto box = DisplayLayout::boundingBox(monitors);
  QCOMPARE(DisplayLayout::reach(monitors, box, Left, true), QList<LayoutSegment>({{0, 500, 0}}));
}

void DisplayLayoutTests::reachNegativeOrigin()
{
  const QList<QRect> monitors = {{-1920, -200, 1920, 1080}, {0, 0, 2560, 1440}};
  const auto box = DisplayLayout::boundingBox(monitors);
  QCOMPARE(box, QRect(-1920, -200, 4480, 1640));
  QCOMPARE(DisplayLayout::reach(monitors, box, Left, false), QList<LayoutSegment>({{-200, 880, -1920}}));
  QCOMPARE(DisplayLayout::reach(monitors, box, Up, true), QList<LayoutSegment>({{-1920, 0, -200}, {0, 2560, 0}}));
}

void DisplayLayoutTests::reachEnclosedHoleStaysClosed()
{
  const auto box = DisplayLayout::boundingBox(kCShape);
  // each column belongs to the first display seen from outside, so the hole has no edge
  QCOMPARE(DisplayLayout::reach(kCShape, box, Down, true), QList<LayoutSegment>({{0, 3000, 1500}}));
  QCOMPARE(DisplayLayout::reach(kCShape, box, Up, true), QList<LayoutSegment>({{0, 3000, 0}}));
  QCOMPARE(
      DisplayLayout::reach(kCShape, box, Right, true),
      QList<LayoutSegment>({{0, 500, 3000}, {500, 1000, 1000}, {1000, 1500, 3000}})
  );
}

void DisplayLayoutTests::reachGapBetweenDisplays()
{
  const QList<QRect> monitors = {{0, 0, 1000, 500}, {2000, 0, 1000, 500}};
  const auto box = DisplayLayout::boundingBox(monitors);
  QCOMPARE(DisplayLayout::reach(monitors, box, Up, false), QList<LayoutSegment>({{0, 1000, 0}, {2000, 3000, 0}}));

  // a client over the gap crosses to each display, and not across the gap
  const auto layout = state(monitors, {client("a", 2000, 300, on(Up, 500))});
  QCOMPARE(DisplayLayout::solve(layout).rects.value("a"), QRect(500, -300, 2000, 300));
  QCOMPARE(linksOf(layout, "srv").size(), 2);
  QString problem;
  QVERIFY2(valid(layout, problem), qPrintable(problem));
}

void DisplayLayoutTests::reachEmptyList()
{
  QCOMPARE(DisplayLayout::boundingBox({}), QRect());
  QVERIFY(DisplayLayout::reach({}, QRect(), Left, true).isEmpty());
  const auto solved = DisplayLayout::solve(state({}, {client("a", 100, 100, on(Left))}));
  QVERIFY(solved.rects.isEmpty());
  QCOMPARE(solved.parked, QStringList({"a"}));
}

void DisplayLayoutTests::solveAllFourSides()
{
  const auto solved = DisplayLayout::solve(state(
      kSingle, {client("l", 500, 300, on(Left, 10)), client("r", 500, 300, on(Right, 20)),
                client("u", 400, 200, on(Up, 30)), client("d", 400, 200, on(Down, 40))}
  ));
  QCOMPARE(solved.rects.value("l"), QRect(-500, 10, 500, 300));
  QCOMPARE(solved.rects.value("r"), QRect(1000, 20, 500, 300));
  QCOMPARE(solved.rects.value("u"), QRect(30, -200, 400, 200));
  QCOMPARE(solved.rects.value("d"), QRect(40, 1000, 400, 200));
  QVERIFY(solved.parked.isEmpty());
}

void DisplayLayoutTests::solveChainThreeDeep()
{
  // listed backwards, so a client's anchor may come later in the list
  const auto solved = DisplayLayout::solve(state(
      kSingle, {client("c", 100, 100, on(Right, 0, "b")), client("b", 100, 100, on(Right, 0, "a")),
                client("a", 100, 100, on(Right))}
  ));
  QCOMPARE(solved.rects.value("a").x(), 1000);
  QCOMPARE(solved.rects.value("b").x(), 1100);
  QCOMPARE(solved.rects.value("c").x(), 1200);
}

void DisplayLayoutTests::solveMissingAnchorParks()
{
  const auto solved = DisplayLayout::solve(state(kSingle, {client("a", 100, 100, on(Right, 0, "gone"))}));
  QVERIFY(solved.rects.isEmpty());
  QCOMPARE(solved.parked, QStringList({"a"}));
}

void DisplayLayoutTests::solveAnchorLoopParks()
{
  const auto solved = DisplayLayout::solve(
      state(kSingle, {client("a", 100, 100, on(Right, 0, "b")), client("b", 100, 100, on(Right, 0, "a"))})
  );
  QVERIFY(solved.rects.isEmpty());
  QCOMPARE(solved.parked.size(), 2);
}

void DisplayLayoutTests::solveInvalidSizeParks()
{
  const auto solved = DisplayLayout::solve(state(
      kSingle,
      {client("zero", 0, 100, on(Right)), client("bad", -5, -5, on(Left)), client("child", 100, 100, on(Up, 0, "zero"))}
  ));
  QVERIFY(solved.rects.isEmpty());
  QCOMPARE(solved.parked.size(), 3);
}

void DisplayLayoutTests::solveNoRoomParks()
{
  const auto solved = DisplayLayout::solve(state(
      kSingle, {client("a", 500, 950, on(Right)), client("b", 500, 950, on(Right)), client("c", 500, 950, on(Right))}
  ));
  QCOMPARE(solved.rects.size(), 2);
  QCOMPARE(solved.parked, QStringList({"c"}));
}

void DisplayLayoutTests::solveReturnsWhenRoomComesBack()
{
  auto layout = state(
      kSingle, {client("a", 500, 950, on(Right)), client("b", 500, 950, on(Right)), client("c", 500, 950, on(Right))}
  );
  QCOMPARE(DisplayLayout::solve(layout).parked, QStringList({"c"}));
  layout.monitors = {{0, 0, 1000, 5000}};
  QVERIFY(DisplayLayout::solve(layout).parked.isEmpty());
}

void DisplayLayoutTests::solveKeepsTheEdgeWhenAResolutionChanges()
{
  auto layout = state(kSingle, {client("a", 500, 300, on(Right, 100))});
  layout.clients[0].size = QSize(800, 600);
  QCOMPARE(DisplayLayout::solve(layout).rects.value("a"), QRect(1000, 100, 800, 600));

  layout.monitors = {{0, 0, 1920, 1080}};
  QCOMPARE(DisplayLayout::solve(layout).rects.value("a"), QRect(1920, 100, 800, 600));
  QCOMPARE(linksOf(layout, "a").size(), 1);
}

void DisplayLayoutTests::solveSlidesWhenADisplayGoesAndComesBack()
{
  // sits low on the monitor's right edge, and the laptop on its own is short
  auto layout = state(kOffset, {client("a", 500, 300, on(Right, 1100))});
  QCOMPARE(DisplayLayout::solve(layout).rects.value("a").y(), 1100);

  layout.monitors = {{0, 0, 1512, 982}};
  const auto rect = DisplayLayout::solve(layout).rects.value("a");
  QCOMPARE(rect.x(), 1512);
  QVERIFY(rect.y() + DisplayLayout::kMinCrossing <= 982);

  layout.monitors = kOffset;
  QCOMPARE(DisplayLayout::solve(layout).rects.value("a").y(), 1100);
}

void DisplayLayoutTests::solveVeryWideDesktop()
{
  const auto layout = state({{0, 0, 65535, 2000}}, {client("a", 500, 300, on(Up, 65000))});
  const auto links = linksOf(layout, "srv");
  QCOMPARE(links.size(), 1);
  QVERIFY(links.first().range.end <= 100);
  QString problem;
  QVERIFY2(valid(layout, problem), qPrintable(problem));
}

void DisplayLayoutTests::solveFifteenComputers()
{
  QList<LayoutClient> clients;
  QString previous;
  for (int i = 0; i < 15; ++i) {
    const auto name = QStringLiteral("c%1").arg(i);
    clients.append(client(name, 1920, 1080, on(Right, 0, previous)));
    previous = name;
  }
  const auto layout = state(kThree, clients);
  const auto solved = DisplayLayout::solve(layout);
  QCOMPARE(solved.rects.size(), 15);
  QString problem;
  QVERIFY2(valid(layout, problem), qPrintable(problem));
}

void DisplayLayoutTests::solveNeverEntersTheBoxWithoutFreeEdges()
{
  // above the laptop there is no edge, so this stays above the box rather than in the notch
  const auto solved = DisplayLayout::solve(state(kOffset, {client("a", 700, 400, on(Up, 100))}));
  const auto rect = solved.rects.value("a");
  QCOMPARE(rect.y() + rect.height(), 0);
  QVERIFY(rect.x() + rect.width() > 1512);
}

void DisplayLayoutTests::linksPartialEdge()
{
  const auto layout = state(kOffset, {client("hp", 1920, 1080, on(Left, 360))});
  const auto links = linksOf(layout, "srv");
  QCOMPARE(links.size(), 1);
  QCOMPARE(links.first().side, Left);
  QCOMPARE(links.first().target, QStringLiteral("hp"));
  // the laptop starts at 458 of 1440, the client at 360, so 98 of its 1080 are above the laptop
  QVERIFY(qAbs(links.first().range.start - 31.8056) < 0.001);
  QCOMPARE(links.first().range.end, 100.0);
  QVERIFY(qAbs(links.first().targetRange.start - 9.0741) < 0.001);

  const auto back = linksOf(layout, "hp");
  QCOMPARE(back.size(), 1);
  QCOMPARE(back.first().side, Right);
  QCOMPARE(back.first().range, links.first().targetRange);
  QCOMPARE(back.first().targetRange, links.first().range);
}

void DisplayLayoutTests::linksTwoOnOneSide()
{
  const auto layout = state(kSingle, {client("a", 500, 400, on(Right, 0)), client("b", 500, 400, on(Right, 500))});
  const auto links = linksOf(layout, "srv");
  QCOMPARE(links.size(), 2);
  QCOMPARE(links.at(0).target, QStringLiteral("a"));
  QCOMPARE(links.at(1).target, QStringLiteral("b"));
  QVERIFY(links.at(0).range.end <= links.at(1).range.start);
}

void DisplayLayoutTests::linksRangesOnlyTouch()
{
  const auto links =
      linksOf(state(kSingle, {client("a", 500, 500, on(Right, 0)), client("b", 500, 500, on(Right, 500))}), "srv");
  QCOMPARE(links.at(0).range.end, links.at(1).range.start);
}

void DisplayLayoutTests::linksOverlapTooShortIsSkipped()
{
  const auto layout = state(kSingle, {client("a", 100, 100, on(Right)), client("b", 100, 100, on(Right))});
  SolvedLayout solved;
  solved.box = kSingle.first();
  solved.rects.insert("a", QRect(1200, 0, 100, 100));
  solved.rects.insert("b", QRect(1300, 100 - DisplayLayout::kMinCrossing + 1, 100, 100));
  QVERIFY(DisplayLayout::links(layout, solved).isEmpty());

  solved.rects["b"].moveTop(100 - DisplayLayout::kMinCrossing);
  QCOMPARE(DisplayLayout::links(layout, solved).size(), 2);
}

void DisplayLayoutTests::linksSmallClientStillLinks()
{
  const auto layout = state(kSingle, {client("tiny", 30, 30, on(Right, 100))});
  QCOMPARE(linksOf(layout, "srv").size(), 1);
}

void DisplayLayoutTests::linksChainBetweenClients()
{
  const auto layout = state(kSingle, {client("a", 500, 400, on(Right, 0)), client("b", 500, 200, on(Right, 100, "a"))});
  const auto links = linksOf(layout, "a");
  QCOMPARE(links.size(), 2);
  QCOMPARE(links.at(0).target, QStringLiteral("srv"));
  QCOMPARE(links.at(1).target, QStringLiteral("b"));
  QCOMPARE(links.at(1).range, LayoutRange(25, 75));
  QCOMPARE(links.at(1).targetRange, LayoutRange());
}

void DisplayLayoutTests::linksWholeEdgeAlone()
{
  const auto layout = state(kOffset, {client("hp", 1920, 1080, on(Left, 360, {}, true))});
  const auto links = linksOf(layout, "srv");
  QCOMPARE(links.size(), 1);
  QCOMPARE(links.first().range, LayoutRange());
  QCOMPARE(links.first().targetRange, LayoutRange());
}

void DisplayLayoutTests::linksWholeEdgeOfAClient()
{
  const auto layout =
      state(kSingle, {client("a", 500, 400, on(Right)), client("b", 500, 200, on(Right, 100, "a", true))});
  const auto links = linksOf(layout, "a");
  QCOMPARE(links.at(1).target, QStringLiteral("b"));
  QCOMPARE(links.at(1).range, LayoutRange());
}

void DisplayLayoutTests::linksWholeEdgeThenASecondClient()
{
  // whole-edge ranges would overlap the second client's, which stops the server starting
  const auto layout =
      state(kSingle, {client("a", 500, 400, on(Left, 0, {}, true)), client("b", 500, 400, on(Left, 500))});
  const auto solved = DisplayLayout::solve(layout);
  QVERIFY(!DisplayLayout::wholeEdgeAllowed(layout, solved, layout.clients.at(0)));
  QVERIFY(DisplayLayout::sideShared(layout, solved, layout.clients.at(0)));
  QString problem;
  QVERIFY2(valid(layout, problem), qPrintable(problem));
  QCOMPARE(linksOf(layout, "srv").size(), 2);
}

void DisplayLayoutTests::linksWholeEdgeIsIgnoredBesideANotch()
{
  const auto layout =
      state(kOffset, {client("a", 1920, 900, on(Left, 500, {}, true)), client("b", 900, 300, on(Left, 60))}, true);
  QString problem;
  QVERIFY2(valid(layout, problem), qPrintable(problem));
  const auto solved = DisplayLayout::solve(layout);
  QVERIFY(!DisplayLayout::wholeEdgeAllowed(layout, solved, layout.clients.at(0)));
}

void DisplayLayoutTests::linksStayOffTheGapInTheBox()
{
  // the client spans the gap above the laptop, but only the laptop's edge can cross
  const auto layout = state(kOffset, {client("hp", 1000, 1440, on(Left, 0))});
  const auto links = linksOf(layout, "srv");
  QCOMPARE(links.size(), 1);
  QVERIFY(qAbs(links.first().range.start - 31.8056) < 0.001);
}

void DisplayLayoutTests::wholeEdgeNeedsTheFacingSideToo()
{
  // `a` overhangs the server, and `b` touches the overhang: a whole-edge link from `a` would
  // overlap its link to `b`, and the core would refuse the config
  const auto layout =
      state(kSingle, {client("a", 1400, 300, on(Up, 0, {}, true)), client("b", 300, 900, on(Right, 0))});
  const auto solved = DisplayLayout::solve(layout);
  QCOMPARE(solved.rects.value("a"), QRect(0, -300, 1400, 300));
  QCOMPARE(solved.rects.value("b"), QRect(1000, 0, 300, 900));
  QVERIFY(!DisplayLayout::wholeEdgeAllowed(layout, solved, layout.clients.at(0)));
  QString problem;
  QVERIFY2(valid(layout, problem), qPrintable(problem));
}

void DisplayLayoutTests::wholeEdgeOfAClientTouchingTheServer()
{
  // `b` sits on top of `a` and also touches the server, so it has two neighbours on one face
  const auto layout =
      state(kSingle, {client("a", 300, 500, on(Right, 0)), client("b", 800, 200, on(Up, -500, "a", true))});
  const auto solved = DisplayLayout::solve(layout);
  QCOMPARE(solved.rects.value("b"), QRect(500, -200, 800, 200));
  QVERIFY(!DisplayLayout::wholeEdgeAllowed(layout, solved, layout.clients.at(1)));
  QString problem;
  QVERIFY2(valid(layout, problem), qPrintable(problem));
}

void DisplayLayoutTests::freeEdgeNotchTop()
{
  const auto layout = state(kOffset, {client("book", 700, 400, on(Up, 0))}, true);
  const auto solved = DisplayLayout::solve(layout);
  QCOMPARE(solved.rects.value("book"), QRect(0, 58, 700, 400));
  const auto links = linksOf(layout, "srv");
  QCOMPARE(links.size(), 1);
  QCOMPARE(links.first().side, Up);
  QVERIFY(qAbs(links.first().range.end - 17.1906) < 0.001);
  QCOMPARE(links.first().targetRange, LayoutRange());
}

void DisplayLayoutTests::freeEdgeNotchCornerLinksBothSides()
{
  const auto layout = state(kOffset, {client("book", 600, 300, on(Up, 912))}, true);
  QCOMPARE(DisplayLayout::solve(layout).rects.value("book"), QRect(912, 158, 600, 300));
  const auto links = linksOf(layout, "srv");
  QCOMPARE(links.size(), 2);
  QCOMPARE(links.at(0).side, Left);
  QCOMPARE(links.at(1).side, Up);
  QString problem;
  QVERIFY2(valid(layout, problem), qPrintable(problem));
}

void DisplayLayoutTests::freeEdgeTooWideForTheNotch()
{
  const auto layout = state(kOffset, {client("book", 1800, 600, on(Up, 100))}, true);
  const auto rect = DisplayLayout::solve(layout).rects.value("book");
  QCOMPARE(rect.y() + rect.height(), 0);
  const auto links = linksOf(layout, "srv");
  QCOMPARE(links.size(), 1);
  QVERIFY(links.first().range.start >= 37.1);
}

void DisplayLayoutTests::formatPercentHasNoDecimalPoint()
{
  QCOMPARE(DisplayLayout::formatPercent(0), QStringLiteral("0"));
  QCOMPARE(DisplayLayout::formatPercent(100), QStringLiteral("100"));
  QCOMPARE(DisplayLayout::formatPercent(50), QStringLiteral("50"));
  QCOMPARE(DisplayLayout::formatPercent(31.8056), QStringLiteral("318056e-4"));
  QCOMPARE(DisplayLayout::formatPercent(0.0015), QStringLiteral("15e-4"));
}

void DisplayLayoutTests::formatPercentIsTheSameUnderACommaLocale()
{
  const auto *previous = std::setlocale(LC_NUMERIC, nullptr);
  const QByteArray saved(previous);
  if (std::setlocale(LC_NUMERIC, "de_DE.UTF-8") == nullptr)
    QSKIP("no comma locale on this system");

  const auto text = DisplayLayout::formatPercent(31.8056);
  char *end = nullptr;
  const double parsed = std::strtod(text.toLatin1().constData(), &end);
  const bool whole = end != nullptr && *end == '\0';
  std::setlocale(LC_NUMERIC, saved.constData());

  QCOMPARE(text, QStringLiteral("318056e-4"));
  QVERIFY(whole);
  QVERIFY(qAbs(parsed - 31.8056) < 0.0001);
}

void DisplayLayoutTests::linkTextGolden()
{
  const auto layout = state(kOffset, {client("hp", 1920, 1080, on(Left, 360)), client("pc", 2560, 1440, on(Right, 0))});
  QCOMPARE(
      DisplayLayout::linkText(layout, DisplayLayout::solve(layout)),
      QStringLiteral(
          "\tsrv:\n"
          "\t\tleft(318056e-4,100) = hp(90741e-4,100)\n"
          "\t\tright = pc\n"
          "\thp:\n"
          "\t\tright(90741e-4,100) = srv(318056e-4,100)\n"
          "\tpc:\n"
          "\t\tleft = srv\n"
      )
  );
}

void DisplayLayoutTests::linkTextLeavesOutTheFullRange()
{
  const auto layout = state(kSingle, {client("a", 500, 1000, on(Up, 0, {}, true)), client("b", 1000, 1000, on(Right))});
  const auto text = DisplayLayout::linkText(layout, DisplayLayout::solve(layout));
  QVERIFY(text.contains(QStringLiteral("\t\tup = a\n")));
  QVERIFY(text.contains(QStringLiteral("\t\tright = b\n")));
}

void DisplayLayoutTests::linkTextLeavesOutParkedComputers()
{
  const auto layout = state(kSingle, {client("a", 500, 500, on(Right, 0, "gone")), client("b", 500, 500)});
  QCOMPARE(DisplayLayout::linkText(layout, DisplayLayout::solve(layout)), QString());
}

void DisplayLayoutTests::snapNearestEdge()
{
  const auto layout = state(kSingle, {client("a", 300, 200)});
  const auto solved = DisplayLayout::solve(layout);
  const auto snap = DisplayLayout::snap(layout, solved, "a", QRect(1100, 400, 300, 200), 0);
  QVERIFY(snap.has_value());
  QCOMPARE(snap->side, Right);
  QCOMPARE(snap->anchor, QString());
  QCOMPARE(snap->rect, QRect(1000, 400, 300, 200));
  QCOMPARE(snap->offset, 400);
}

void DisplayLayoutTests::snapMagnets()
{
  const auto layout = state(kSingle, {client("a", 300, 200)});
  const auto solved = DisplayLayout::solve(layout);

  QCOMPARE(DisplayLayout::snap(layout, solved, "a", QRect(1100, 10, 300, 200), 30)->rect.y(), 0);
  QCOMPARE(DisplayLayout::snap(layout, solved, "a", QRect(1100, 785, 300, 200), 30)->rect.y(), 800);
  QCOMPARE(DisplayLayout::snap(layout, solved, "a", QRect(1100, 395, 300, 200), 30)->rect.y(), 400);
  QCOMPARE(DisplayLayout::snap(layout, solved, "a", QRect(1100, 300, 300, 200), 30)->rect.y(), 300);
}

void DisplayLayoutTests::snapSkipsItsOwnSubtree()
{
  const auto layout = state(kSingle, {client("a", 300, 200, on(Right)), client("b", 300, 200, on(Right, 0, "a"))});
  const auto solved = DisplayLayout::solve(layout);
  const auto snap = DisplayLayout::snap(layout, solved, "a", QRect(1350, 0, 300, 200), 0);
  QVERIFY(snap.has_value());
  QVERIFY(snap->anchor != QStringLiteral("b"));
}

void DisplayLayoutTests::snapCanDockOnAClient()
{
  const auto layout = state(kSingle, {client("a", 300, 200, on(Right)), client("b", 300, 200)});
  const auto solved = DisplayLayout::solve(layout);
  const auto snap = DisplayLayout::snap(layout, solved, "b", QRect(1320, 0, 300, 200), 0);
  QVERIFY(snap.has_value());
  QCOMPARE(snap->anchor, QStringLiteral("a"));
  QCOMPARE(snap->rect.x(), 1300);
}

void DisplayLayoutTests::snapFindsNoFreeSpot()
{
  const auto layout = state({}, {client("a", 300, 200)});
  QVERIFY(!DisplayLayout::snap(layout, DisplayLayout::solve(layout), "a", QRect(0, 0, 300, 200), 0).has_value());
}

void DisplayLayoutTests::subtreeFollowsAnchors()
{
  const auto layout = state(
      kSingle, {client("a", 1, 1, on(Right)), client("b", 1, 1, on(Right, 0, "a")), client("c", 1, 1, on(Up, 0, "b")),
                client("other", 1, 1, on(Left))}
  );
  QCOMPARE(DisplayLayout::subtree(layout, "a"), QSet<QString>({"a", "b", "c"}));
  QCOMPARE(DisplayLayout::subtree(layout, "c"), QSet<QString>({"c"}));
}

void DisplayLayoutTests::unlinkTakesWhatHangsOffIt()
{
  auto layout = state(
      kSingle, {client("a", 1, 1, on(Right)), client("b", 1, 1, on(Right, 0, "a")), client("other", 1, 1, on(Left))}
  );
  DisplayLayout::unlink(layout, "a");
  QVERIFY(!layout.clients.at(0).placement.has_value());
  QVERIFY(!layout.clients.at(1).placement.has_value());
  QVERIFY(layout.clients.at(2).placement.has_value());
}

void DisplayLayoutTests::removeClientLeavesTheRestPlaced()
{
  auto layout = state(
      kSingle, {client("a", 1, 1, on(Right)), client("b", 1, 1, on(Right, 0, "a")), client("other", 1, 1, on(Left))}
  );
  DisplayLayout::removeClient(layout, "a");
  QCOMPARE(layout.clients.size(), 2);
  QCOMPARE(layout.clients.at(0).name, QStringLiteral("b"));
  QVERIFY(!layout.clients.at(0).placement.has_value());
  QVERIFY(layout.clients.at(1).placement.has_value());
}

void DisplayLayoutTests::renameClientUpdatesAnchors()
{
  auto layout =
      state(kSingle, {client("a", 1, 1, on(Right)), client("b", 1, 1, on(Right, 0, "a")), client("shelf", 1, 1)});
  DisplayLayout::renameClient(layout, "a", "z");
  QCOMPARE(layout.clients.at(0).name, QStringLiteral("z"));
  QCOMPARE(layout.clients.at(1).placement->anchor, QStringLiteral("z"));
  QVERIFY(!layout.clients.at(2).placement.has_value());
}

void DisplayLayoutTests::defaultPlacementFirstFreeSide()
{
  auto layout = state(kSingle, {});
  const auto newcomer = client("new", 500, 400);
  QCOMPARE(DisplayLayout::defaultPlacement(layout, newcomer)->side, Left);

  layout.clients = {client("a", 500, 400, on(Left))};
  QCOMPARE(DisplayLayout::defaultPlacement(layout, newcomer)->side, Right);

  layout.clients << client("b", 500, 400, on(Right));
  QCOMPARE(DisplayLayout::defaultPlacement(layout, newcomer)->side, Up);

  layout.clients << client("c", 500, 400, on(Up));
  QCOMPARE(DisplayLayout::defaultPlacement(layout, newcomer)->side, Down);
}

void DisplayLayoutTests::defaultPlacementSharesASideWhenAllAreTaken()
{
  const auto layout = state(
      kSingle, {client("a", 500, 100, on(Left)), client("b", 500, 100, on(Right)), client("c", 100, 100, on(Up)),
                client("d", 100, 100, on(Down))}
  );
  const auto placement = DisplayLayout::defaultPlacement(layout, client("new", 300, 300));
  QVERIFY(placement.has_value());
  QCOMPARE(placement->side, Left);
}

void DisplayLayoutTests::defaultPlacementBridgeGoesToTheShelf()
{
  auto bridge = client("dongle", 500, 400);
  bridge.bridge = true;
  QVERIFY(!DisplayLayout::defaultPlacement(state(kSingle, {}), bridge).has_value());
}

void DisplayLayoutTests::defaultPlacementInvalidSize()
{
  QVERIFY(!DisplayLayout::defaultPlacement(state(kSingle, {}), client("new", 0, 0)).has_value());
}

void DisplayLayoutTests::defaultPlacementNoRoom()
{
  QVERIFY(!DisplayLayout::defaultPlacement(state({}, {}), client("new", 500, 400)).has_value());
}

void DisplayLayoutTests::defaultPlacementSeesOnlyWhatIsOnTheServer()
{
  // none of these takes the left side: one is on the shelf, one hangs off another computer, one
  // doesn't fit, and one sits elsewhere
  const auto layout = state(
      kSingle, {client("shelf", 100, 100), client("above", 100, 100, on(Up)),
                client("chain", 100, 100, on(Left, 0, "above")), client("zero", 0, 0, on(Left))}
  );
  QCOMPARE(DisplayLayout::defaultPlacement(layout, client("new", 500, 400))->side, Left);
}

void DisplayLayoutTests::randomLayoutsAreAlwaysValid()
{
  QRandomGenerator random(20261106);
  const QList<QList<QRect>> desks = {kSingle, kOffset, kThree, kCShape};
  for (int round = 0; round < 1000; ++round) {
    const auto &monitors = desks.at(random.bounded(desks.size()));
    const bool freeEdges = random.bounded(2) == 1;
    QList<LayoutClient> clients;
    const int count = random.bounded(1, 7);
    for (int i = 0; i < count; ++i) {
      std::optional<LayoutPlacement> placement;
      if (random.bounded(5) != 0) {
        const auto anchor = i > 0 && random.bounded(3) == 0 ? clients.at(random.bounded(i)).name : QString();
        placement = LayoutPlacement{
            anchor, static_cast<LayoutSide>(random.bounded(4)), random.bounded(-500, 4000), random.bounded(4) == 0
        };
      }
      clients.append(
          client(QStringLiteral("c%1").arg(i), random.bounded(200, 3000), random.bounded(200, 2000), placement)
      );
    }
    const auto layout = state(monitors, clients, freeEdges);
    QString problem;
    QVERIFY2(valid(layout, problem), qPrintable(QStringLiteral("round %1: %2").arg(round).arg(problem)));
  }
}

QTEST_MAIN(DisplayLayoutTests)
