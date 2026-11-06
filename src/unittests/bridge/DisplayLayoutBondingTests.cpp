/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "DisplayLayoutBondingTests.h"

#include "common/Settings.h"
#include "gui/core/BridgeClientConfigManager.h"
#include "gui/layout/DisplayLayoutStore.h"

#include <QFile>

using namespace deskflow::gui;
using enum LayoutSide;

namespace {

const auto kName = QStringLiteral("dongle");
const LayoutPlacement kStart{{}, Left, 100, false};

void saveLayout(const std::optional<LayoutPlacement> &placement)
{
  StoredLayout layout;
  layout.saved = true;
  layout.active = true;
  layout.clients = {{kName, QSize(1920, 1080), true, true, placement}};
  QVERIFY(DisplayLayoutStore::save(layout));
}

} // namespace

DisplayLayoutBondingTests::DisplayLayoutBondingTests() = default;

void DisplayLayoutBondingTests::initTestCase()
{
  m_arch.init();
  QVERIFY(m_dir.isValid());
  qputenv("XDG_CONFIG_HOME", QFile::encodeName(m_dir.path()));
  qputenv("XDG_STATE_HOME", QFile::encodeName(m_dir.path()));
  Settings::setSettingsFile(m_dir.filePath(QStringLiteral("test.conf")));
  m_config = m_dir.filePath(QStringLiteral("bridge.conf"));
}

void DisplayLayoutBondingTests::init()
{
  QFile::remove(m_config);
  saveLayout(kStart);
}

void DisplayLayoutBondingTests::exactPlacementPerProfile()
{
  DisplayLayoutStore::rememberProfile(m_config, 0, kName);
  DisplayLayoutStore::applyPlacement(kName, LayoutPlacement{{}, Right, 300, true});
  DisplayLayoutStore::rememberProfile(m_config, 1, kName);

  QCOMPARE(*DisplayLayoutStore::decodePlacement(BridgeClientConfigManager::readProfilePlacement(m_config, 0)), kStart);
  QCOMPARE(
      *DisplayLayoutStore::decodePlacement(BridgeClientConfigManager::readProfilePlacement(m_config, 1)),
      LayoutPlacement({}, Right, 300, true)
  );
}

void DisplayLayoutBondingTests::profileSwitchMovesTheComputer()
{
  BridgeClientConfigManager::writeProfilePlacement(m_config, 1, QStringLiteral("right|300|1|"));
  QVERIFY(DisplayLayoutStore::applyProfile(m_config, 1, kName));
  QCOMPARE(DisplayLayoutStore::placementOf(kName)->side, Right);
  QCOMPARE(DisplayLayoutStore::placementOf(kName)->offset, 300);
}

void DisplayLayoutBondingTests::sameProfileChangesNothing()
{
  DisplayLayoutStore::rememberProfile(m_config, 0, kName);
  QVERIFY(!DisplayLayoutStore::applyProfile(m_config, 0, kName));
}

void DisplayLayoutBondingTests::profileWithOnlyTheOldOffset()
{
  BridgeClientConfigManager::writeProfileScreenLocation(m_config, 2, QPoint(0, -1));
  QVERIFY(DisplayLayoutStore::applyProfile(m_config, 2, kName));
  QCOMPARE(DisplayLayoutStore::placementOf(kName)->side, Up);
  QVERIFY(DisplayLayoutStore::placementOf(kName)->wholeEdge);
}

void DisplayLayoutBondingTests::farOffsetMeansNotLinked()
{
  BridgeClientConfigManager::writeProfileScreenLocation(m_config, 2, QPoint(2, -1));
  QVERIFY(DisplayLayoutStore::applyProfile(m_config, 2, kName));
  QVERIFY(!DisplayLayoutStore::placementOf(kName).has_value());
}

void DisplayLayoutBondingTests::exactPlacementBeatsTheOldOffset()
{
  BridgeClientConfigManager::writeProfileScreenLocation(m_config, 3, QPoint(0, 1));
  BridgeClientConfigManager::writeProfilePlacement(m_config, 3, QStringLiteral("right|40|0|"));
  QVERIFY(DisplayLayoutStore::applyProfile(m_config, 3, kName));
  QCOMPARE(DisplayLayoutStore::placementOf(kName)->side, Right);
}

void DisplayLayoutBondingTests::placementCanBeNotLinked()
{
  saveLayout(std::nullopt);
  DisplayLayoutStore::rememberProfile(m_config, 0, kName);
  QCOMPARE(BridgeClientConfigManager::readProfilePlacement(m_config, 0), QStringLiteral("none"));

  saveLayout(kStart);
  QVERIFY(DisplayLayoutStore::applyProfile(m_config, 0, kName));
  QVERIFY(!DisplayLayoutStore::placementOf(kName).has_value());
}

void DisplayLayoutBondingTests::oldOffsetStaysForTheClassicGrid()
{
  BridgeClientConfigManager::writeProfileScreenLocation(m_config, 0, QPoint(1, 0));
  DisplayLayoutStore::rememberProfile(m_config, 0, kName);
  QCOMPARE(BridgeClientConfigManager::readProfileScreenLocation(m_config, 0), QPoint(1, 0));
  QVERIFY(BridgeClientConfigManager::hasProfileScreenLocation(m_config, 0));
}

void DisplayLayoutBondingTests::noProfileKeepsNothing()
{
  BridgeClientConfigManager::writeProfilePlacement(m_config, -1, QStringLiteral("right|40|0|"));
  QCOMPARE(BridgeClientConfigManager::readProfilePlacement(m_config, -1), QString());
}

void DisplayLayoutBondingTests::renamingAnAnchorReachesEveryProfile()
{
  const auto config = QStringLiteral("%1/%2.conf").arg(BridgeClientConfigManager::bridgeClientsDir(), kName);
  BridgeClientConfigManager::writeProfilePlacement(config, 0, QStringLiteral("right|0|0|laptop"));
  BridgeClientConfigManager::writeProfilePlacement(config, 1, QStringLiteral("down|300|1|laptop"));
  BridgeClientConfigManager::writeProfilePlacement(config, 2, QStringLiteral("left|0|0|other"));
  BridgeClientConfigManager::writeProfilePlacement(config, 3, QStringLiteral("none"));
  BridgeClientConfigManager::writeProfilePlacement(config, 4, QStringLiteral("not a placement"));

  DisplayLayoutStore::rename(QStringLiteral("laptop"), QStringLiteral("desk"));

  QCOMPARE(BridgeClientConfigManager::readProfilePlacement(config, 0), QStringLiteral("right|0|0|desk"));
  QCOMPARE(BridgeClientConfigManager::readProfilePlacement(config, 1), QStringLiteral("down|300|1|desk"));
  QCOMPARE(BridgeClientConfigManager::readProfilePlacement(config, 2), QStringLiteral("left|0|0|other"));
  QCOMPARE(BridgeClientConfigManager::readProfilePlacement(config, 3), QStringLiteral("none"));
  QCOMPARE(BridgeClientConfigManager::readProfilePlacement(config, 4), QStringLiteral("not a placement"));
  QVERIFY(QFile::remove(config));
}

QTEST_GUILESS_MAIN(DisplayLayoutBondingTests)
