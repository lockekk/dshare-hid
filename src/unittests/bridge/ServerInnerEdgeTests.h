/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "arch/Arch.h"
#include "base/EventQueue.h"
#include "base/Log.h"

#include <QTemporaryDir>
#include <QTest>

#include <memory>

class Desk;

namespace deskflow::core::ipc {
class CoreIpcServer;
}

/**
 * @brief Inner edges through the real Server, PrimaryClient and Config
 *
 * Only the operating system is fake: FakePlatform stands in for a screen, and FakeClient for a
 * computer on the network. If upstream changes how the server maps a cursor, this is what notices.
 */
class ServerInnerEdgeTests : public QObject
{
  Q_OBJECT
public:
  ServerInnerEdgeTests();
  ~ServerInnerEdgeTests() override;

private Q_SLOTS:
  void initTestCase();
  void init();
  void cleanup();

  // the eight rows of the plan's table
  void topOfTheShortDisplayCrosses();
  void comesBackOntoTheShortDisplay();
  void sideOfTheTallDisplayCrosses();
  void comesBackOntoTheTallDisplay();
  void sameEdgeWithoutALinkStays();
  void freeEdgeWithoutALinkStays();
  void seamBetweenDisplaysStays();
  void outerEdgeIsUnchanged();
  void linkAlongAWholeSideStaysAtTheOutside();
  void switchHotkeyFindsAPartialLink();
  void switchHotkeyPrefersTheCursorsRow();
  void switchHotkeyWithNothingOnThatSide();

  void everySide_data();
  void everySide();
  void bothEdgesOfACorner();
  void widerJumpZone();
  void switchDelayWaitsOnAFreeEdge();
  void doubleTapOnAFreeEdge();
  void lockedCursorStays();
  void offlineClientIsSkipped();
  void chainFromANotch();
  void displaysAreReadAgainOnAShapeChange();
  void wrongDisplayListChangesNothing();

  void clientSizeReachesTheGui();

private:
  Arch m_arch;
  Log m_log;
  EventQueue m_events;
  QTemporaryDir m_dir;
  QTemporaryDir m_socketDir;
  std::unique_ptr<deskflow::core::ipc::CoreIpcServer> m_ipc;
  std::unique_ptr<Desk> m_desk;
};
