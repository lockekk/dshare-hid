/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "DisplayLayoutSeedTests.h"

#include "gui/layout/DisplayLayout.h"

using namespace deskflow::gui;
using enum LayoutSide;

namespace {

constexpr int kColumns = 5;

/// a 5x3 grid with the server in the middle
QStringList grid(const QHash<int, QString> &cells)
{
  QStringList names(15);
  names[7] = QStringLiteral("srv");
  for (auto it = cells.cbegin(); it != cells.cend(); ++it)
    names[it.key()] = it.value();
  return names;
}

LayoutState stateFor(const QStringList &cells)
{
  LayoutState state;
  state.serverName = QStringLiteral("srv");
  state.monitors = {{0, 0, 1920, 1080}};
  for (const auto &name : cells) {
    if (!name.isEmpty() && name != state.serverName)
      state.clients.append({name, QSize(1920, 1080), true, false, std::nullopt});
  }
  return state;
}

const LayoutClient &byName(const LayoutState &state, const QString &name)
{
  return *std::ranges::find(state.clients, name, &LayoutClient::name);
}

} // namespace

void DisplayLayoutSeedTests::fourNeighboursLinkWholeEdges()
{
  const auto cells = grid({{6, "left"}, {8, "right"}, {2, "top"}, {12, "bottom"}});
  auto state = stateFor(cells);
  DisplayLayout::seedFromGrid(state, cells, kColumns);

  QCOMPARE(byName(state, "left").placement->side, Left);
  QCOMPARE(byName(state, "right").placement->side, Right);
  QCOMPARE(byName(state, "top").placement->side, Up);
  QCOMPARE(byName(state, "bottom").placement->side, Down);
  for (const auto &client : std::as_const(state.clients)) {
    QVERIFY(client.placement->wholeEdge);
    QVERIFY(client.placement->anchor.isEmpty());
  }
}

void DisplayLayoutSeedTests::chainFollowsTheGrid()
{
  const auto cells = grid({{8, "near"}, {9, "far"}});
  auto state = stateFor(cells);
  DisplayLayout::seedFromGrid(state, cells, kColumns);

  QCOMPARE(byName(state, "near").placement->anchor, QString());
  QCOMPARE(byName(state, "far").placement->anchor, QStringLiteral("near"));
  QCOMPARE(byName(state, "far").placement->side, Right);
}

void DisplayLayoutSeedTests::isolatedCellGoesToTheShelf()
{
  const auto cells = grid({{0, "alone"}, {8, "near"}});
  auto state = stateFor(cells);
  DisplayLayout::seedFromGrid(state, cells, kColumns);
  QVERIFY(!byName(state, "alone").placement.has_value());
  QVERIFY(byName(state, "near").placement.has_value());
}

void DisplayLayoutSeedTests::gridLoopPlacesEveryoneOnce()
{
  // the layout that prompted this work: a dongle above the laptop and beside the top computer
  const auto cells = grid({{0, "pc"}, {2, "macbook"}, {3, "dongle1"}, {4, "dongle2"}, {6, "home"}, {8, "laptop"}});
  auto state = stateFor(cells);
  DisplayLayout::seedFromGrid(state, cells, kColumns);

  const auto solved = DisplayLayout::solve(state);
  QVERIFY(solved.parked.isEmpty());
  QVERIFY(!byName(state, "pc").placement.has_value());
  QCOMPARE(solved.rects.size(), 5);
  QVERIFY(byName(state, "dongle1").placement.has_value());
  QVERIFY(byName(state, "dongle2").placement.has_value());
}

void DisplayLayoutSeedTests::edgeOfTheGridDoesNotWrap()
{
  // cell 4 ends the first row and cell 5 starts the next: they aren't neighbours
  QStringList cells(15);
  cells[4] = QStringLiteral("srv");
  cells[5] = QStringLiteral("wrapped");
  auto state = stateFor(cells);
  DisplayLayout::seedFromGrid(state, cells, kColumns);
  QVERIFY(!byName(state, "wrapped").placement.has_value());

  cells = QStringList(15);
  cells[5] = QStringLiteral("srv");
  cells[4] = QStringLiteral("wrapped");
  state = stateFor(cells);
  DisplayLayout::seedFromGrid(state, cells, kColumns);
  QVERIFY(!byName(state, "wrapped").placement.has_value());
}

void DisplayLayoutSeedTests::neighbourBridgeKeepsItsLink()
{
  const auto cells = grid({{8, "dongle"}});
  auto state = stateFor(cells);
  state.clients[0].bridge = true;
  DisplayLayout::seedFromGrid(state, cells, kColumns);
  QVERIFY(byName(state, "dongle").placement.has_value());
}

void DisplayLayoutSeedTests::noServerOnTheGrid()
{
  QStringList cells(15);
  cells[8] = QStringLiteral("a");
  auto state = stateFor(cells);
  DisplayLayout::seedFromGrid(state, cells, kColumns);
  QVERIFY(!byName(state, "a").placement.has_value());

  DisplayLayout::seedFromGrid(state, grid({{8, "a"}}), 0);
  QVERIFY(!byName(state, "a").placement.has_value());
}

void DisplayLayoutSeedTests::linksMatchTheGrid()
{
  const auto cells = grid({{6, "left"}, {8, "right"}, {9, "far"}});
  auto state = stateFor(cells);
  DisplayLayout::seedFromGrid(state, cells, kColumns);
  QCOMPARE(
      DisplayLayout::linkText(state, DisplayLayout::solve(state)), QStringLiteral(
                                                                       "\tsrv:\n"
                                                                       "\t\tleft = left\n"
                                                                       "\t\tright = right\n"
                                                                       "\tleft:\n"
                                                                       "\t\tright = srv\n"
                                                                       "\tright:\n"
                                                                       "\t\tleft = srv\n"
                                                                       "\t\tright = far\n"
                                                                       "\tfar:\n"
                                                                       "\t\tleft = right\n"
                                                                   )
  );
}

void DisplayLayoutSeedTests::clearsOldPlacements()
{
  const auto cells = grid({{0, "alone"}});
  auto state = stateFor(cells);
  state.clients[0].placement = LayoutPlacement{{}, Left, 0, false};
  DisplayLayout::seedFromGrid(state, cells, kColumns);
  QVERIFY(!state.clients.at(0).placement.has_value());
}

void DisplayLayoutSeedTests::cellWithoutAClientIsSkipped()
{
  auto state = stateFor(grid({{8, "known"}}));
  DisplayLayout::seedFromGrid(state, grid({{8, "known"}, {6, "stranger"}}), kColumns);
  QCOMPARE(state.clients.size(), 1);
  QVERIFY(state.clients.first().placement.has_value());
}

QTEST_MAIN(DisplayLayoutSeedTests)
