/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "ServerInnerEdgeTests.h"

#include "common/Constants.h"
#include "common/Settings.h"
#include "common/VersionInfo.h"
#include "deskflow/Computer.h"
#include "deskflow/IPlatformComputer.h"
#include "deskflow/ipc/CoreIpcServer.h"
#include "io/IStream.h"
#include "server/BaseClientProxy.h"
#include "server/Config.h"
#include "server/DisplayEdges.h"
#include "server/PrimaryClient.h"
#include "server/Server.h"

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QFile>
#include <QLocalSocket>

#include <sstream>

using deskflow::server::Config;
using deskflow::server::DisplayEdges;
using enum Direction;

namespace {

// a low laptop on the left and a taller monitor on the right, leaving a notch above the laptop
const QList<QRect> kOffset = {{0, 458, 1512, 982}, {1512, 0, 2560, 1440}};
const QList<QRect> kFlipped = {{0, 0, 2560, 1440}, {2560, 0, 1512, 982}};
const QRect kBox(0, 0, 4072, 1440);

QList<QRect> s_displays;

QList<QRect> displays()
{
  return s_displays;
}

class FakePlatform : public IPlatformComputer
{
public:
  explicit FakePlatform(IEventQueue *events) : IPlatformComputer(events)
  {
  }

  void *getEventTarget() const override
  {
    return const_cast<FakePlatform *>(this);
  }
  bool getClipboard(ClipboardID, IClipboard *) const override
  {
    return false;
  }
  void getShape(int32_t &x, int32_t &y, int32_t &width, int32_t &height) const override
  {
    x = kBox.x();
    y = kBox.y();
    width = kBox.width();
    height = kBox.height();
  }
  void getCursorPos(int32_t &x, int32_t &y) const override
  {
    x = 2000;
    y = 700;
  }
  void reconfigure(uint32_t) override
  {
  }
  uint32_t activeSides() override
  {
    return 0;
  }
  void warpCursor(int32_t, int32_t) override
  {
  }
  uint32_t registerHotKey(KeyID, KeyModifierMask) override
  {
    return 0;
  }
  void unregisterHotKey(uint32_t) override
  {
  }
  void fakeInputBegin() override
  {
  }
  void fakeInputEnd() override
  {
  }
  int32_t getJumpZoneSize() const override
  {
    return zone;
  }
  bool isAnyMouseButtonDown(uint32_t &) const override
  {
    return false;
  }
  void getCursorCenter(int32_t &x, int32_t &y) const override
  {
    x = 2000;
    y = 700;
  }
  void fakeMouseButton(ButtonID, bool) override
  {
  }
  void fakeMouseMove(int32_t, int32_t) override
  {
  }
  void fakeMouseRelativeMove(int32_t, int32_t) const override
  {
  }
  void fakeMouseWheel(ScrollDelta) const override
  {
  }
  void updateKeyMap() override
  {
  }
  void updateKeyState() override
  {
  }
  void setHalfDuplexMask(KeyModifierMask) override
  {
  }
  void fakeKeyDown(KeyID, KeyModifierMask, KeyButton, const std::string &) override
  {
  }
  bool fakeKeyRepeat(KeyID, KeyModifierMask, int32_t, KeyButton, const std::string &) override
  {
    return false;
  }
  bool fakeKeyUp(KeyButton) override
  {
    return false;
  }
  void fakeAllKeysUp() override
  {
  }
  bool fakeCtrlAltDel() override
  {
    return false;
  }
  bool fakeMediaKey(KeyID) override
  {
    return false;
  }
  bool isKeyDown(KeyButton) const override
  {
    return false;
  }
  KeyModifierMask getActiveModifiers() const override
  {
    return 0;
  }
  KeyModifierMask pollActiveModifiers() const override
  {
    return toggleMask;
  }
  int32_t pollActiveGroup() const override
  {
    return 0;
  }
  void pollPressedKeys(KeyButtonSet &) const override
  {
  }
  void clearStaleModifiers() override
  {
  }
  void enable() override
  {
  }
  void disable() override
  {
  }
  void enter() override
  {
  }
  bool canLeave() override
  {
    return true;
  }
  void leave() override
  {
  }
  bool setClipboard(ClipboardID, const IClipboard *) override
  {
    return true;
  }
  void checkClipboards() override
  {
  }
  void openScreensaver(bool) override
  {
  }
  void closeScreensaver() override
  {
  }
  void screensaver(bool) override
  {
  }
  void resetOptions() override
  {
  }
  void setOptions(const OptionsList &) override
  {
  }
  void setSequenceNumber(uint32_t) override
  {
  }
  std::string getSecureInputApp() const override
  {
    return {};
  }
  bool isPrimary() const override
  {
    return true;
  }
  void handleSystemEvent(const Event &) override
  {
  }

  int32_t zone = 1;
  KeyModifierMask toggleMask = 0;
};

/// a client's connection, which the server writes to as it closes
class NullStream : public deskflow::IStream
{
public:
  void close() override
  {
  }
  uint32_t read(void *, uint32_t) override
  {
    return 0;
  }
  void write(const void *, uint32_t) override
  {
  }
  void flush() override
  {
  }
  void shutdownInput() override
  {
  }
  void shutdownOutput() override
  {
  }
  void *getEventTarget() const override
  {
    return const_cast<NullStream *>(this);
  }
  bool isReady() const override
  {
    return false;
  }
  uint32_t getSize() const override
  {
    return 0;
  }
};

struct Entered
{
  QString computer;
  QPoint at;
};

class FakeClient : public BaseClientProxy
{
public:
  FakeClient(const std::string &name, const QSize &size, Entered &entered)
      : BaseClientProxy(name),
        m_size(size),
        m_entered(entered)
  {
  }

  void *getEventTarget() const override
  {
    return const_cast<FakeClient *>(this);
  }
  bool getClipboard(ClipboardID, IClipboard *) const override
  {
    return false;
  }
  void getShape(int32_t &x, int32_t &y, int32_t &width, int32_t &height) const override
  {
    x = 0;
    y = 0;
    width = m_size.width();
    height = m_size.height();
  }
  void getCursorPos(int32_t &x, int32_t &y) const override
  {
    x = m_size.width() / 2;
    y = m_size.height() / 2;
  }
  void enter(int32_t x, int32_t y, uint32_t, KeyModifierMask, bool) override
  {
    m_entered = {QString::fromStdString(getName()), QPoint(x, y)};
  }
  bool leave() override
  {
    return true;
  }
  void setClipboard(ClipboardID, const IClipboard *) override
  {
  }
  void grabClipboard(ClipboardID) override
  {
  }
  void setClipboardDirty(ClipboardID, bool) override
  {
  }
  void keyDown(KeyID, KeyModifierMask, KeyButton, const std::string &) override
  {
  }
  void keyRepeat(KeyID, KeyModifierMask, int32_t, KeyButton, const std::string &) override
  {
  }
  void keyUp(KeyID, KeyModifierMask, KeyButton) override
  {
  }
  void mouseDown(ButtonID) override
  {
  }
  void mouseUp(ButtonID) override
  {
  }
  void mouseMove(int32_t, int32_t) override
  {
  }
  void mouseRelativeMove(int32_t, int32_t) override
  {
  }
  void mouseWheel(int32_t, int32_t) override
  {
  }
  void screensaver(bool) override
  {
  }
  void resetOptions() override
  {
  }
  void setOptions(const OptionsList &) override
  {
  }
  void sendDragInfo(uint32_t, const char *, size_t) override
  {
  }
  void fileChunkSending(uint8_t, char *, size_t) override
  {
  }
  std::string getSecureInputApp() const override
  {
    return {};
  }
  void secureInputNotification(const std::string &) const override
  {
  }
  deskflow::IStream *getStream() const override
  {
    return &m_stream;
  }

private:
  QSize m_size;
  Entered &m_entered;
  mutable NullStream m_stream;
};

/// the primary client reports where it was entered, as the fake clients do
class WatchedPrimary : public PrimaryClient
{
public:
  WatchedPrimary(deskflow::Computer *computer, Entered &entered) : PrimaryClient("srv", computer), m_entered(entered)
  {
  }

  void enter(int32_t x, int32_t y, uint32_t seqNum, KeyModifierMask mask, bool forScreensaver) override
  {
    m_entered = {QStringLiteral("srv"), QPoint(x, y)};
    PrimaryClient::enter(x, y, seqNum, mask, forScreensaver);
  }

private:
  Entered &m_entered;
};

} // namespace

/// a server with these links, and a client for each computer named in `online`
class Desk
{
public:
  Desk(
      IEventQueue *events, const QString &links, const QHash<QString, QSize> &online, int32_t zone = 1,
      KeyModifierMask toggles = 0
  )
      : m_events(events),
        config(events)
  {
    const QStringList names = {"srv", "notch", "upper", "pc", "far"};
    for (const auto &name : names)
      Settings::setValue(Settings::Computer::Name.arg(name), name);
    Settings::proxy().sync();

    std::istringstream stream(
        QStringLiteral("section: links\n%1end\n\nsection: options\nend\n\n").arg(links).toStdString()
    );
    stream >> config;

    platform = new FakePlatform(events);
    platform->zone = zone;
    platform->toggleMask = toggles;
    computer = std::make_unique<deskflow::Computer>(platform, events);
    primary = std::make_unique<WatchedPrimary>(computer.get(), entered);
  }

  void start()
  {
    server = std::make_unique<Server>(config, primary.get(), computer.get(), m_events);
    for (auto it = m_online.cbegin(); it != m_online.cend(); ++it)
      server->adoptClient(new FakeClient(it.key().toStdString(), it.value(), entered));
    moveOnServer(2000, 700);
    entered = {};
  }

  void connect(const QHash<QString, QSize> &online)
  {
    m_online = online;
  }

  void pressSwitch(Direction direction)
  {
    m_events->dispatchEvent(Event(
        EventTypes::ServerSwitchInDirection, config.getInputFilter(), new Server::SwitchInDirectionInfo(direction),
        Event::EventFlags::DeliverImmediately
    ));
  }

  void moveOnServer(int x, int y)
  {
    m_events->dispatchEvent(Event(
        EventTypes::PrimaryComputerMotionOnPrimary, computer->getEventTarget(),
        IPrimaryComputer::MotionInfo::alloc(x, y)
    ));
  }

  void moveOnClient(int dx, int dy)
  {
    m_events->dispatchEvent(Event(
        EventTypes::PrimaryComputerMotionOnSecondary, computer->getEventTarget(),
        IPrimaryComputer::MotionInfo::alloc(dx, dy)
    ));
  }

  /// runs the event queue, for a timer the server started
  void wait(double seconds)
  {
    const auto until = QDeadlineTimer(static_cast<qint64>(seconds * 1000));
    while (!until.hasExpired()) {
      Event event;
      if (m_events->getEvent(event, 0.01))
        m_events->dispatchEvent(event);
    }
  }

  // the server jumps back to the primary as it closes, so this has to outlive it
  Entered entered;
  IEventQueue *m_events;
  Config config;
  FakePlatform *platform = nullptr;
  std::unique_ptr<deskflow::Computer> computer;
  std::unique_ptr<WatchedPrimary> primary;
  std::unique_ptr<Server> server;
  QHash<QString, QSize> m_online;
};

namespace {

// on top of the short display, x 0 to 700; left of the tall one above it, y 50 to 400; right of it
const auto kLinks = QStringLiteral(
    "\tsrv:\n"
    "\t\tup(0,171906e-4) = notch\n"
    "\t\tleft(34722e-4,277778e-4) = upper\n"
    "\t\tright = pc\n"
    "\tnotch:\n\t\tdown = srv(0,171906e-4)\n"
    "\tupper:\n\t\tright = srv(34722e-4,277778e-4)\n"
    "\tpc:\n\t\tleft = srv\n"
);
const QHash<QString, QSize> kOnline = {{"notch", {700, 400}}, {"upper", {800, 350}}, {"pc", {2560, 1440}}};

} // namespace

#ifdef Q_OS_WIN
ServerInnerEdgeTests::ServerInnerEdgeTests() = default;
#else
// a short path, because a socket's path has a low limit
ServerInnerEdgeTests::ServerInnerEdgeTests() : m_socketDir(QStringLiteral("/tmp/dsh-XXXXXX"))
{
}
#endif

ServerInnerEdgeTests::~ServerInnerEdgeTests() = default;

void ServerInnerEdgeTests::initTestCase()
{
  m_arch.init();
  QVERIFY(m_dir.isValid());
  qputenv("XDG_CONFIG_HOME", QFile::encodeName(m_dir.path()));
  qputenv("XDG_STATE_HOME", QFile::encodeName(m_dir.path()));
  Settings::setSettingsFile(m_dir.filePath(QStringLiteral("test.conf")));

  // listening replaces any socket of this name, so keep clear of a core that's running here
  QVERIFY(m_socketDir.isValid());
  qputenv("TMPDIR", QFile::encodeName(m_socketDir.path()));
  m_ipc = std::make_unique<deskflow::core::ipc::CoreIpcServer>(nullptr);
}

void ServerInnerEdgeTests::init()
{
  s_displays = kOffset;
  DisplayEdges::setSource(&displays);
  m_desk = std::make_unique<Desk>(&m_events, kLinks, kOnline);
  m_desk->connect(kOnline);
  m_desk->start();
}

void ServerInnerEdgeTests::cleanup()
{
  m_desk.reset();
  DisplayEdges::setSource(nullptr);
}

void ServerInnerEdgeTests::topOfTheShortDisplayCrosses()
{
  m_desk->moveOnServer(350, 600);
  QVERIFY(m_desk->entered.computer.isEmpty());
  m_desk->moveOnServer(350, 458);
  QCOMPARE(m_desk->entered.computer, QStringLiteral("notch"));
  QCOMPARE(m_desk->entered.at, QPoint(350, 399));
}

void ServerInnerEdgeTests::comesBackOntoTheShortDisplay()
{
  m_desk->moveOnServer(350, 458);
  m_desk->moveOnClient(0, 3);
  QCOMPARE(m_desk->entered.computer, QStringLiteral("srv"));
  QCOMPARE(m_desk->entered.at, QPoint(350, 459));
}

void ServerInnerEdgeTests::sideOfTheTallDisplayCrosses()
{
  m_desk->moveOnServer(1512, 200);
  QCOMPARE(m_desk->entered.computer, QStringLiteral("upper"));
  QCOMPARE(m_desk->entered.at, QPoint(799, 150));
}

void ServerInnerEdgeTests::comesBackOntoTheTallDisplay()
{
  m_desk->moveOnServer(1512, 200);
  m_desk->moveOnClient(3, 0);
  QCOMPARE(m_desk->entered.computer, QStringLiteral("srv"));
  QCOMPARE(m_desk->entered.at, QPoint(1513, 200));
}

void ServerInnerEdgeTests::sameEdgeWithoutALinkStays()
{
  m_desk->moveOnServer(1000, 458);
  QVERIFY(m_desk->entered.computer.isEmpty());
}

void ServerInnerEdgeTests::freeEdgeWithoutALinkStays()
{
  m_desk->moveOnServer(1512, 420);
  QVERIFY(m_desk->entered.computer.isEmpty());
}

void ServerInnerEdgeTests::seamBetweenDisplaysStays()
{
  m_desk->moveOnServer(1512, 700);
  QVERIFY(m_desk->entered.computer.isEmpty());
}

void ServerInnerEdgeTests::outerEdgeIsUnchanged()
{
  m_desk->moveOnServer(4071, 700);
  QCOMPARE(m_desk->entered.computer, QStringLiteral("pc"));
  QCOMPARE(m_desk->entered.at, QPoint(0, 700));

  m_desk->moveOnClient(-2000, 0);
  QCOMPARE(m_desk->entered.computer, QStringLiteral("srv"));
  QCOMPARE(m_desk->entered.at, QPoint(2072, 700));
}

void ServerInnerEdgeTests::linkAlongAWholeSideStaysAtTheOutside()
{
  m_desk.reset();
  m_desk =
      std::make_unique<Desk>(&m_events, QStringLiteral("\tsrv:\n\t\tleft = pc\n\tpc:\n\t\tright = srv\n"), kOnline);
  m_desk->connect(kOnline);
  m_desk->start();

  // what the grid writes, so the tall display's free edge stays the wall it has always been
  m_desk->moveOnServer(1512, 200);
  QVERIFY(m_desk->entered.computer.isEmpty());

  m_desk->moveOnServer(0, 800);
  QCOMPARE(m_desk->entered.computer, QStringLiteral("pc"));
}

void ServerInnerEdgeTests::switchHotkeyFindsAPartialLink()
{
  // "upper" only covers y 50 to 400 of the left side, and the cursor is far below that
  m_desk->moveOnServer(2000, 1300);
  m_desk->pressSwitch(Direction::Left);
  QCOMPARE(m_desk->entered.computer, QStringLiteral("upper"));
}

void ServerInnerEdgeTests::switchHotkeyPrefersTheCursorsRow()
{
  m_desk->moveOnServer(2000, 200);
  m_desk->pressSwitch(Direction::Left);
  QCOMPARE(m_desk->entered.computer, QStringLiteral("upper"));
}

void ServerInnerEdgeTests::switchHotkeyWithNothingOnThatSide()
{
  m_desk->moveOnServer(2000, 700);
  m_desk->pressSwitch(Direction::Bottom);
  QVERIFY(m_desk->entered.computer.isEmpty());
}

void ServerInnerEdgeTests::everySide_data()
{
  QTest::addColumn<QPoint>("edge");
  QTest::addColumn<QString>("links");
  QTest::addColumn<QPoint>("back");
  QTest::addColumn<QPoint>("landing");

  // the flipped desk has its notch below and to the right of the short display
  QTest::newRow("bottom") << QPoint(3000, 981)
                          << "\tsrv:\n\t\tdown(628684e-4,100) = notch\n\tnotch:\n\t\tup = srv(628684e-4,100)\n"
                          << QPoint(0, -3) << QPoint(3000, 980);
  QTest::newRow("right") << QPoint(2559, 1200)
                         << "\tsrv:\n\t\tright(681944e-4,100) = notch\n\tnotch:\n\t\tleft = srv(681944e-4,100)\n"
                         << QPoint(-3, 0) << QPoint(2558, 1200);
}

void ServerInnerEdgeTests::everySide()
{
  QFETCH(QPoint, edge);
  QFETCH(QString, links);
  QFETCH(QPoint, back);
  QFETCH(QPoint, landing);

  m_desk.reset();
  s_displays = kFlipped;
  DisplayEdges::refresh();
  m_desk = std::make_unique<Desk>(&m_events, links, QHash<QString, QSize>());
  m_desk->connect({{"notch", {1512, 458}}});
  m_desk->start();

  m_desk->moveOnServer(edge.x(), edge.y());
  QCOMPARE(m_desk->entered.computer, QStringLiteral("notch"));
  m_desk->moveOnClient(back.x(), back.y());
  QCOMPARE(m_desk->entered.computer, QStringLiteral("srv"));
  QCOMPARE(m_desk->entered.at, landing);
}

void ServerInnerEdgeTests::bothEdgesOfACorner()
{
  m_desk.reset();
  // one client in the corner of the notch, reached from either display
  m_desk = std::make_unique<Desk>(
      &m_events,
      QStringLiteral(
          "\tsrv:\n"
          "\t\tleft(109722e-4,318056e-4) = notch(0,100)\n"
          "\t\tup(223969e-4,371316e-4) = notch(0,100)\n"
          "\tnotch:\n"
          "\t\tright = srv(109722e-4,318056e-4)\n"
          "\t\tdown = srv(223969e-4,371316e-4)\n"
      ),
      QHash<QString, QSize>()
  );
  m_desk->connect({{"notch", {600, 300}}});
  m_desk->start();

  m_desk->moveOnServer(1200, 458);
  QCOMPARE(m_desk->entered.computer, QStringLiteral("notch"));
  m_desk->moveOnClient(0, 3);
  QCOMPARE(m_desk->entered.computer, QStringLiteral("srv"));

  m_desk->entered = {};
  m_desk->moveOnServer(2000, 300);
  m_desk->moveOnServer(1512, 300);
  QCOMPARE(m_desk->entered.computer, QStringLiteral("notch"));
  m_desk->moveOnClient(3, 0);
  QCOMPARE(m_desk->entered.computer, QStringLiteral("srv"));
}

void ServerInnerEdgeTests::widerJumpZone()
{
  m_desk.reset();
  m_desk = std::make_unique<Desk>(&m_events, kLinks, kOnline, 3);
  m_desk->connect(kOnline);
  m_desk->start();

  m_desk->moveOnServer(350, 461);
  QVERIFY(m_desk->entered.computer.isEmpty());
  m_desk->moveOnServer(350, 460);
  QCOMPARE(m_desk->entered.computer, QStringLiteral("notch"));

  // landing clears the zone, so the cursor doesn't bounce straight back
  m_desk->moveOnClient(0, 3);
  QCOMPARE(m_desk->entered.computer, QStringLiteral("srv"));
  QCOMPARE(m_desk->entered.at, QPoint(350, 461));
}

void ServerInnerEdgeTests::switchDelayWaitsOnAFreeEdge()
{
  m_desk.reset();
  m_desk = std::make_unique<Desk>(&m_events, kLinks, kOnline);
  m_desk->config.addOption("", kOptionComputerSwitchDelay, 50);
  m_desk->connect(kOnline);
  m_desk->start();

  m_desk->moveOnServer(350, 458);
  QVERIFY(m_desk->entered.computer.isEmpty());
  m_desk->wait(0.2);
  QCOMPARE(m_desk->entered.computer, QStringLiteral("notch"));
}

void ServerInnerEdgeTests::doubleTapOnAFreeEdge()
{
  m_desk.reset();
  m_desk = std::make_unique<Desk>(&m_events, kLinks, kOnline);
  m_desk->config.addOption("", kOptionComputerSwitchTwoTap, 500);
  m_desk->connect(kOnline);
  m_desk->start();

  m_desk->moveOnServer(350, 458);
  QVERIFY(m_desk->entered.computer.isEmpty());

  // the server wants two moves away from the edge before it counts the tap
  m_desk->moveOnServer(350, 520);
  m_desk->moveOnServer(350, 600);
  QVERIFY(m_desk->entered.computer.isEmpty());
  m_desk->moveOnServer(350, 458);
  QCOMPARE(m_desk->entered.computer, QStringLiteral("notch"));
}

void ServerInnerEdgeTests::lockedCursorStays()
{
  m_desk.reset();
  m_desk = std::make_unique<Desk>(&m_events, kLinks, kOnline, 1, KeyModifierScrollLock);
  m_desk->connect(kOnline);
  m_desk->start();

  m_desk->moveOnServer(350, 458);
  QVERIFY(m_desk->entered.computer.isEmpty());
}

void ServerInnerEdgeTests::offlineClientIsSkipped()
{
  m_desk.reset();
  m_desk = std::make_unique<Desk>(&m_events, kLinks, QHash<QString, QSize>());
  m_desk->connect({{"pc", {2560, 1440}}});
  m_desk->start();

  m_desk->moveOnServer(350, 458);
  QVERIFY(m_desk->entered.computer.isEmpty());
  m_desk->moveOnServer(1512, 200);
  QVERIFY(m_desk->entered.computer.isEmpty());
}

void ServerInnerEdgeTests::chainFromANotch()
{
  m_desk.reset();
  m_desk = std::make_unique<Desk>(
      &m_events,
      QStringLiteral(
          "\tsrv:\n\t\tup(0,171906e-4) = notch\n"
          "\tnotch:\n\t\tdown = srv(0,171906e-4)\n\t\tup = far\n"
          "\tfar:\n\t\tdown = notch\n"
      ),
      QHash<QString, QSize>()
  );
  m_desk->connect({{"notch", {700, 400}}, {"far", {700, 500}}});
  m_desk->start();

  m_desk->moveOnServer(350, 458);
  QCOMPARE(m_desk->entered.computer, QStringLiteral("notch"));
  m_desk->moveOnClient(0, -500);
  QCOMPARE(m_desk->entered.computer, QStringLiteral("far"));
}

void ServerInnerEdgeTests::displaysAreReadAgainOnAShapeChange()
{
  m_desk->moveOnServer(350, 458);
  QCOMPARE(m_desk->entered.computer, QStringLiteral("notch"));
  m_desk->moveOnClient(0, 3);

  // the short display moves down, and the edge goes with it
  s_displays = {{0, 600, 1512, 840}, {1512, 0, 2560, 1440}};
  m_events.dispatchEvent(Event(EventTypes::ComputerShapeChanged, m_desk->computer->getEventTarget()));
  m_desk->entered = {};
  m_desk->moveOnServer(350, 700);
  m_desk->moveOnServer(350, 600);
  QCOMPARE(m_desk->entered.computer, QStringLiteral("notch"));
}

void ServerInnerEdgeTests::wrongDisplayListChangesNothing()
{
  s_displays = {{0, 229, 756, 491}, {756, 0, 1280, 720}};
  DisplayEdges::refresh();
  m_desk->moveOnServer(350, 229);
  QVERIFY(m_desk->entered.computer.isEmpty());
  m_desk->moveOnServer(4071, 700);
  QCOMPARE(m_desk->entered.computer, QStringLiteral("pc"));
}

void ServerInnerEdgeTests::clientSizeReachesTheGui()
{
#if defined(Q_OS_WIN)
  QSKIP("a pipe's name is shared with a core that's running here");
#endif
  m_ipc->listen();
  QLocalSocket socket;
  socket.connectToServer(QString::fromLatin1(kCoreIpcName));
  QVERIFY(socket.waitForConnected(2000));
  socket.write(QStringLiteral("hello=%1+%2\n").arg(kVersion, kVersionGitSha).toUtf8());
  socket.flush();

  QByteArray received;
  QTRY_VERIFY(received.append(socket.readAll()).contains("computerShape=2560,1440,pc\n"));
  QVERIFY(received.contains("computerShape=700,400,notch\n"));
}

QTEST_GUILESS_MAIN(ServerInnerEdgeTests)
