/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "DisplayLayoutStoreTests.h"

#include "common/Settings.h"
#include "gui/layout/DisplayLayoutStore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>

using namespace deskflow::gui;
using enum LayoutSide;

namespace {

void writeFile(const QByteArray &text)
{
  QFile file(DisplayLayoutStore::filePath());
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  file.write(text);
}

StoredLayout sample()
{
  StoredLayout layout;
  layout.saved = true;
  layout.active = true;
  layout.clients = {
      {"hp", QSize(1920, 1080), true, false, LayoutPlacement{{}, Left, 360, false}},
      {"book", QSize(1440, 900), true, false, LayoutPlacement{"hp", Up, -20, true}},
      {"dongle", QSize(1080, 2424), true, false, std::nullopt},
  };
  return layout;
}

} // namespace

DisplayLayoutStoreTests::DisplayLayoutStoreTests() = default;

void DisplayLayoutStoreTests::initTestCase()
{
  m_arch.init();
  QVERIFY(m_dir.isValid());
  qputenv("XDG_CONFIG_HOME", QFile::encodeName(m_dir.path()));
  qputenv("XDG_STATE_HOME", QFile::encodeName(m_dir.path()));
  Settings::setSettingsFile(m_dir.filePath(QStringLiteral("test.conf")));
}

void DisplayLayoutStoreTests::init()
{
  QFile::remove(DisplayLayoutStore::filePath());
}

void DisplayLayoutStoreTests::missingFileIsInactive()
{
  const auto layout = DisplayLayoutStore::load();
  QVERIFY(!layout.saved);
  QVERIFY(!layout.active);
  QVERIFY(layout.clients.isEmpty());
  QVERIFY(!DisplayLayoutStore::isActive());
}

void DisplayLayoutStoreTests::savesAndLoads()
{
  QVERIFY(DisplayLayoutStore::save(sample()));
  QVERIFY(DisplayLayoutStore::isActive());

  const auto layout = DisplayLayoutStore::load();
  QVERIFY(layout.saved);
  QVERIFY(layout.active);
  QCOMPARE(layout.clients.size(), 3);
  QCOMPARE(layout.clients.at(0).placement, sample().clients.at(0).placement);
  QCOMPARE(layout.clients.at(1).placement, sample().clients.at(1).placement);
  QVERIFY(!layout.clients.at(2).placement.has_value());
}

void DisplayLayoutStoreTests::saveKeepsKnownSizes()
{
  QVERIFY(DisplayLayoutStore::save(sample()));
  const auto layout = DisplayLayoutStore::load();
  QCOMPARE(layout.sizes.value("hp"), QSize(1920, 1080));
  QCOMPARE(layout.sizes.value("dongle"), QSize(1080, 2424));
}

void DisplayLayoutStoreTests::unknownSizeIsNotStored()
{
  auto layout = sample();
  layout.clients[0].sizeKnown = false;
  QVERIFY(DisplayLayoutStore::save(layout));
  QVERIFY(!DisplayLayoutStore::load().sizes.contains("hp"));
}

void DisplayLayoutStoreTests::corruptFileIsIgnored()
{
  writeFile("not json at all {{{");
  QVERIFY(!DisplayLayoutStore::load().saved);
  QVERIFY(!DisplayLayoutStore::isActive());
}

void DisplayLayoutStoreTests::truncatedFileIsIgnored()
{
  QVERIFY(DisplayLayoutStore::save(sample()));
  QFile file(DisplayLayoutStore::filePath());
  QVERIFY(file.open(QIODevice::ReadOnly));
  const auto text = file.readAll();
  file.close();
  writeFile(text.left(text.size() / 2));
  QVERIFY(!DisplayLayoutStore::isActive());
}

void DisplayLayoutStoreTests::newerVersionIsIgnored()
{
  writeFile(R"({"version": 2, "saved": true, "active": true})");
  QVERIFY(!DisplayLayoutStore::isActive());
}

void DisplayLayoutStoreTests::notAnObjectIsIgnored()
{
  writeFile("[1, 2, 3]");
  QVERIFY(!DisplayLayoutStore::isActive());
}

void DisplayLayoutStoreTests::badPlacementGoesToTheShelf()
{
  writeFile(R"({"version": 1, "saved": true, "active": true,
                "clients": [{"name": "a", "placement": {"side": "sideways", "offset": 1}}]})");
  const auto layout = DisplayLayoutStore::load();
  QCOMPARE(layout.clients.size(), 1);
  QVERIFY(!layout.clients.first().placement.has_value());
}

void DisplayLayoutStoreTests::clientWithoutANameIsDropped()
{
  writeFile(R"({"version": 1, "saved": true, "clients": [{"name": ""}, {"name": "a"}]})");
  QCOMPARE(DisplayLayoutStore::load().clients.size(), 1);
}

void DisplayLayoutStoreTests::saveFailsWhenTheFolderIsGone()
{
  // a folder in the file's place can't be written
  QVERIFY(QDir().mkpath(DisplayLayoutStore::filePath()));
  const bool saved = DisplayLayoutStore::save(sample());
  QVERIFY(QDir().rmdir(DisplayLayoutStore::filePath()));
  QVERIFY(!saved);
}

void DisplayLayoutStoreTests::isNextToTheSettings()
{
  QCOMPARE(DisplayLayoutStore::filePath(), QStringLiteral("%1/screen-layout.json").arg(Settings::settingsPath()));
}

void DisplayLayoutStoreTests::rememberSizeBeforeALayoutExists()
{
  DisplayLayoutStore::rememberSize("dongle", {1080, 2424});
  const auto layout = DisplayLayoutStore::load();
  QCOMPARE(layout.sizes.value("dongle"), QSize(1080, 2424));
  QVERIFY(!layout.saved);
  QVERIFY(!DisplayLayoutStore::isActive());
}

void DisplayLayoutStoreTests::rememberSizeIgnoresNothingUseful()
{
  DisplayLayoutStore::rememberSize({}, {100, 100});
  DisplayLayoutStore::rememberSize("a", {0, 0});
  QVERIFY(!QFile::exists(DisplayLayoutStore::filePath()));
}

void DisplayLayoutStoreTests::rememberSizeSkipsAnUnchangedSize()
{
  DisplayLayoutStore::rememberSize("a", {100, 100});
  const auto modified = QFileInfo(DisplayLayoutStore::filePath()).lastModified();
  QTest::qWait(20);
  DisplayLayoutStore::rememberSize("a", {100, 100});
  QCOMPARE(QFileInfo(DisplayLayoutStore::filePath()).lastModified(), modified);
}

void DisplayLayoutStoreTests::rememberShapeFromTheCore()
{
  DisplayLayoutStore::rememberShape("2560,1440,desktop-pc");
  QCOMPARE(DisplayLayoutStore::load().sizes.value("desktop-pc"), QSize(2560, 1440));
}

void DisplayLayoutStoreTests::rememberShapeIgnoresNonsense()
{
  DisplayLayoutStore::rememberShape("not a size");
  QVERIFY(!QFile::exists(DisplayLayoutStore::filePath()));
}

void DisplayLayoutStoreTests::renameMovesEverything()
{
  QVERIFY(DisplayLayoutStore::save(sample()));
  DisplayLayoutStore::rename("hp", "laptop");

  const auto layout = DisplayLayoutStore::load();
  QCOMPARE(layout.clients.at(0).name, QStringLiteral("laptop"));
  QCOMPARE(layout.clients.at(1).placement->anchor, QStringLiteral("laptop"));
  QCOMPARE(layout.sizes.value("laptop"), QSize(1920, 1080));
  QVERIFY(!layout.sizes.contains("hp"));

  // a computer whose size was never known keeps not having one
  DisplayLayoutStore::rename("nobody", "somebody");
  QVERIFY(!DisplayLayoutStore::load().sizes.contains("somebody"));
}

void DisplayLayoutStoreTests::placementTextRoundTrips()
{
  const LayoutPlacement placement{"hp", Up, -20, true};
  QCOMPARE(*DisplayLayoutStore::decodePlacement(DisplayLayoutStore::encodePlacement(placement)), placement);

  const LayoutPlacement onServer{{}, Right, 300, false};
  QCOMPARE(*DisplayLayoutStore::decodePlacement(DisplayLayoutStore::encodePlacement(onServer)), onServer);

  const auto none = DisplayLayoutStore::decodePlacement(DisplayLayoutStore::encodePlacement(std::nullopt));
  QVERIFY(none.has_value());
  QVERIFY(!none->has_value());
}

void DisplayLayoutStoreTests::placementTextRejectsNonsense()
{
  QVERIFY(!DisplayLayoutStore::decodePlacement({}).has_value());
  QVERIFY(!DisplayLayoutStore::decodePlacement("left|1").has_value());
  QVERIFY(!DisplayLayoutStore::decodePlacement("sideways|1|0|").has_value());
  QVERIFY(!DisplayLayoutStore::decodePlacement("left|x|0|").has_value());
}

void DisplayLayoutStoreTests::gridOffsetMeansASide()
{
  QCOMPARE(DisplayLayoutStore::placementFromGridOffset({-1, 0})->side, Left);
  QCOMPARE(DisplayLayoutStore::placementFromGridOffset({1, 0})->side, Right);
  QCOMPARE(DisplayLayoutStore::placementFromGridOffset({0, -1})->side, Up);
  QCOMPARE(DisplayLayoutStore::placementFromGridOffset({0, 1})->side, Down);
  QVERIFY(DisplayLayoutStore::placementFromGridOffset({1, 0})->wholeEdge);
  QVERIFY(!DisplayLayoutStore::placementFromGridOffset({2, 0}).has_value());
  QVERIFY(!DisplayLayoutStore::placementFromGridOffset({1, 1}).has_value());
}

void DisplayLayoutStoreTests::placementOfUnknownComputer()
{
  QVERIFY(DisplayLayoutStore::save(sample()));
  QCOMPARE(DisplayLayoutStore::placementOf("hp")->offset, 360);
  QVERIFY(!DisplayLayoutStore::placementOf("nobody").has_value());
}

void DisplayLayoutStoreTests::applyPlacementReportsAChange()
{
  QVERIFY(DisplayLayoutStore::save(sample()));
  const LayoutPlacement placement{{}, Right, 10, false};
  QVERIFY(DisplayLayoutStore::applyPlacement("hp", placement));
  QVERIFY(!DisplayLayoutStore::applyPlacement("hp", placement));
  QVERIFY(!DisplayLayoutStore::applyPlacement("nobody", placement));
  QCOMPARE(DisplayLayoutStore::placementOf("hp"), placement);
}

void DisplayLayoutStoreTests::onlyAServerOnItsOwnConfigUsesTheLinks()
{
  QVERIFY(DisplayLayoutStore::coreUsesLinks(true));
  QVERIFY(!DisplayLayoutStore::coreUsesLinks(false));

  // the writer never runs for an external config, so what it last wrote can't catch up
  Settings::setValue(Settings::Server::ExternalConfig, true);
  QVERIFY(!DisplayLayoutStore::coreUsesLinks(true));
  Settings::setValue(Settings::Server::ExternalConfig, false);
}

QTEST_MAIN(DisplayLayoutStoreTests)
