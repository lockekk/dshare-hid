/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "LayoutHooksTests.h"

#include <QDirIterator>
#include <QFile>
#include <QMap>

namespace {

const auto kMarker = QStringLiteral("DShare-HID: layout");

const QMap<QString, int> kExpected = {
    {"src/lib/gui/config/ServerConfig.cpp", 4},
    {"src/lib/gui/dialogs/ServerConfigDialog.cpp", 3},
    {"src/lib/gui/ComputerSetupModel.h", 2},
    {"src/lib/gui/core/CoreProcess.cpp", 2},
    {"src/lib/gui/core/CoreProcess.h", 1},
    {"src/lib/platform/CMakeLists.txt", 4},
    {"src/lib/platform/OSXComputer.mm", 2},
    {"src/lib/platform/OSXComputer.h", 1},
    {"src/lib/platform/PortalInputCapture.cpp", 6},
    {"src/lib/platform/PortalInputCapture.h", 7},
    {"src/lib/server/Server.cpp", 12},
    {"src/lib/server/Server.h", 4},
    {"src/lib/server/CMakeLists.txt", 2},
    {"src/lib/deskflow/ServerApp.cpp", 3},
};

QString read(const QString &relative)
{
  QFile file(QStringLiteral(LAYOUT_SOURCE_ROOT "/%1").arg(relative));
  return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

} // namespace

void LayoutHooksTests::markersPerFile_data()
{
  QTest::addColumn<QString>("file");
  QTest::addColumn<int>("count");
  for (auto it = kExpected.cbegin(); it != kExpected.cend(); ++it)
    QTest::newRow(qPrintable(it.key())) << it.key() << it.value();
}

void LayoutHooksTests::markersPerFile()
{
  QFETCH(QString, file);
  QFETCH(int, count);
  const auto text = read(file);
  QVERIFY2(!text.isEmpty(), qPrintable(QStringLiteral("cannot read %1").arg(file)));
  QCOMPARE(text.count(kMarker), count);
}

void LayoutHooksTests::everyHookedFileIsWatched()
{
  // the gate only runs these tests when a watched file changes, so a file missing here can lose its hook unseen
  const auto gate = read(QStringLiteral(".agent/skills/merge-upstream-master/verify_merge.sh"));
  const auto guide = read(QStringLiteral("docs/upgrade_from_upstream_guide.md"));
  for (auto it = kExpected.cbegin(); it != kExpected.cend(); ++it) {
    QVERIFY2(gate.contains(it.key()), qPrintable(it.key()));
    QVERIFY2(guide.contains(it.key()), qPrintable(it.key()));
  }
}

void LayoutHooksTests::noMarkerOutsideTheList()
{
  const QString root = QStringLiteral(LAYOUT_SOURCE_ROOT "/");
  QDirIterator files(
      root + "src/lib", {"*.cpp", "*.h", "*.mm", "CMakeLists.txt"}, QDir::Files, QDirIterator::Subdirectories
  );
  while (files.hasNext()) {
    const auto path = files.next();
    const auto relative = path.mid(root.size());
    if (read(relative).contains(kMarker))
      QVERIFY2(kExpected.contains(relative), qPrintable(QStringLiteral("add %1 to Scenario U").arg(relative)));
  }
}

void LayoutHooksTests::hooksSitAtTheirAnchors_data()
{
  QTest::addColumn<QString>("file");
  QTest::addColumn<QString>("anchor");
  QTest::addColumn<QString>("hook");

  const QString serverConfig = "src/lib/gui/config/ServerConfig.cpp";
  const QString dialog = "src/lib/gui/dialogs/ServerConfigDialog.cpp";
  const QString server = "src/lib/server/Server.cpp";
  const QString core = "src/lib/gui/core/CoreProcess.cpp";
  QTest::newRow("grid links are held back") << serverConfig << "int ServerConfig::adjacentComputerIndex(int idx"
                                            << "DisplayLayoutStore::isActive()";
  QTest::newRow("layout links are written") << serverConfig << "outStream << \"section: links\""
                                            << "DisplayLayoutStore::writeLinks";
  QTest::newRow("renames reach the layout") << serverConfig << "m_computers[index].setName(newName);"
                                            << "DisplayLayoutStore::rename";
  QTest::newRow("a client's size is sent when it connects")
      << server << "void Server::adoptClient(BaseClientProxy *client)" << "sendComputerShapeIpc(client);";
  QTest::newRow("a client's size is sent when it changes")
      << server << "void Server::handleShapeChanged(BaseClientProxy *client)" << "sendComputerShapeIpc(client);";
  QTest::newRow("a free edge reaches the box edge")
      << server << "bool Server::onMouseMovePrimary(int32_t x, int32_t y)" << "projectOntoFreeEdge(x, y, zoneSize);";
  QTest::newRow("coming back lands on a display")
      << server << "avoidJumpZone(dst, srcSide, x, y);" << "landOnDisplay(dst, srcSide, x, y);";
  QTest::newRow("the displays are read again")
      << server << "void Server::handleShapeChanged(BaseClientProxy *client)" << "DisplayEdges::refresh();";
  QTest::newRow("the core says where displays come from")
      << QString("src/lib/deskflow/ServerApp.cpp") << "ServerApp::openServerComputer()" << "DisplayEdges::setSource";
  QTest::newRow("a crossing through the Dock still hides the cursor")
      << QString("src/lib/platform/OSXComputer.mm") << "void OSXComputer::leave()" << "hideOffTheDock();";
  const QString portal = "src/lib/platform/PortalInputCapture.cpp";
  QTest::newRow("the portal's desktop is every display")
      << QString("src/lib/platform/PortalInputCapture.cpp") << "bool PortalInputCapture::getPortalBounds"
      << "boundsFromAllZones(bounds)";
  QTest::newRow("a hotkey finds a partial link")
      << server << "void Server::handleSwitchInDirectionEvent" << "nearestNeighbor(m_active, info->m_direction)";
  QTest::newRow("barriers go on free edges")
      << portal << "void PortalInputCapture::handleZonesChanged" << "addFreeEdgeBarriers(session, id);";
  QTest::newRow("a refusal falls back to the outer edges")
      << portal << "void PortalInputCapture::handleSetPointerBarriers" << "retryWithoutFreeEdges(error)";
  QTest::newRow("the GUI hears a client's size") << core << "void CoreProcess::onCoreIpcMessageReceived"
                                                 << "Q_EMIT computerShape(args);";
  QTest::newRow("the view is installed") << dialog << "initConnections();" << "DisplayLayoutView::install";
  QTest::newRow("layout changes enable Save") << dialog << "bool ServerConfigDialog::isGeneralConfigModified() const"
                                              << "DisplayLayoutView::isModified";
}

void LayoutHooksTests::hooksSitAtTheirAnchors()
{
  QFETCH(QString, file);
  QFETCH(QString, anchor);
  QFETCH(QString, hook);

  constexpr int kNearby = 3000;
  const auto text = read(file);
  const auto at = text.indexOf(anchor);
  QVERIFY2(at >= 0, qPrintable(QStringLiteral("anchor gone: %1").arg(anchor)));
  QVERIFY2(text.mid(at, kNearby).contains(hook), qPrintable(QStringLiteral("hook moved: %1").arg(hook)));
}

void LayoutHooksTests::hooksComeInTheRightOrder_data()
{
  QTest::addColumn<QString>("file");
  QTest::addColumn<QString>("first");
  QTest::addColumn<QString>("then");

  const QString server = "src/lib/server/Server.cpp";
  QTest::newRow("links are written inside the links section")
      << QString("src/lib/gui/config/ServerConfig.cpp") << "DisplayLayoutStore::writeLinks(outStream, config);"
      << "outStream << \"end\"";
  QTest::newRow("a position reaches the box before the edge test")
      << server << "projectOntoFreeEdge(x, y, zoneSize);" << "if (xc < ax + zoneSize)";
  QTest::newRow("a landing follows the jump zone") << server << "avoidJumpZone(dst, srcSide, x, y);"
                                                   << "landOnDisplay(dst, srcSide, x, y);";
  QTest::newRow("the Dock is left before the mouse is captured")
      << QString("src/lib/platform/OSXComputer.mm") << "hideOffTheDock();"
      << "CGAssociateMouseAndMouseCursorPosition(false);";
}

void LayoutHooksTests::hooksComeInTheRightOrder()
{
  QFETCH(QString, file);
  QFETCH(QString, first);
  QFETCH(QString, then);

  const auto text = read(file);
  const auto at = text.indexOf(first);
  QVERIFY2(at >= 0, qPrintable(QStringLiteral("gone: %1").arg(first)));
  QVERIFY2(text.indexOf(then, at) > at, qPrintable(QStringLiteral("%1 must come before %2").arg(first, then)));
}

QTEST_GUILESS_MAIN(LayoutHooksTests)
