/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#include "BridgeActiveProfileTests.h"

#include "../deskflow/MockEventQueue.h"
#include "FakeCdcFirmware.h"

#include "common/Settings.h"
#include "platform/bridge/BridgePlatformScreen.h"
#include "platform/bridge/BridgeRequests.h"

#include <QElapsedTimer>
#include <QFile>

#include <algorithm>
#include <map>
#include <memory>
#include <utility>

using namespace deskflow::bridge;

namespace {

// longer than CdcTransport's 1 s wait for a reply, and well inside the next command's
constexpr int kLateReplyMs = 1300;

/**
 * @brief Keeps the handlers it is given, to call them, and the types of the events it is sent
 */
class RecordingEventQueue : public MockEventQueue
{
public:
  void addHandler(EventTypes type, void *target, const EventHandler &handler) override
  {
    m_handlers[{type, target}] = handler;
  }

  void removeHandler(EventTypes type, void *target) override
  {
    m_handlers.erase({type, target});
  }

  void addEvent(Event &&event) override
  {
    added.push_back(event.getType());
  }

  bool call(EventTypes type, void *target)
  {
    const auto it = m_handlers.find({type, target});
    if (it == m_handlers.end()) {
      return false;
    }
    it->second(Event(type, target));
    return true;
  }

  std::vector<EventTypes> added;

private:
  std::map<std::pair<EventTypes, void *>, EventHandler> m_handlers;
};

int32_t shapeWidth(const BridgePlatformScreen &screen)
{
  int32_t x = 0;
  int32_t y = 0;
  int32_t width = 0;
  int32_t height = 0;
  screen.getShape(x, y, width, height);
  return width;
}

int32_t shapeHeight(const BridgePlatformScreen &screen)
{
  int32_t x = 0;
  int32_t y = 0;
  int32_t width = 0;
  int32_t height = 0;
  screen.getShape(x, y, width, height);
  return height;
}

} // namespace

void BridgeActiveProfileTests::initTestCase()
{
  // before the settings are first read, which would otherwise open the user's own file
  QVERIFY(m_settingsDir.isValid());
  qputenv("XDG_CONFIG_HOME", QFile::encodeName(m_settingsDir.path()));
  qputenv("XDG_STATE_HOME", QFile::encodeName(m_settingsDir.path()));
  Settings::setSettingsFile(m_settingsDir.filePath(QStringLiteral("test.conf")));
  Settings::setStateFile(m_settingsDir.filePath(QStringLiteral("test.state")));
  // the keep-alive would talk to the device on a timer of its own
  Settings::setValue(Settings::Bridge::BluetoothKeepAlive, false);
}

void BridgeActiveProfileTests::activeProfileComesFromDevice()
{
  FakeCdcFirmware device(4);
  QVERIFY(!device.devicePath().isEmpty());
  CdcTransport transport(device.devicePath());
  QVERIFY(transport.open(true)); // the fake can't sign its HELLO reply
  QCOMPARE(int(transport.deviceConfig().activeProfile), 0);

  // switched without the transport knowing, as when a switch's reply was lost
  device.active = 2;

  uint8_t index = 0;
  DeviceProfile profile;
  QVERIFY(transport.getActiveProfile(index, profile));
  QCOMPARE(int(index), 2);
  QCOMPARE(QByteArray(profile.hostname), QByteArray("Host 2"));
  QCOMPARE(int(transport.deviceConfig().activeProfile), 2);
}

void BridgeActiveProfileTests::activeProfileRejectsUnknownSlot()
{
  FakeCdcFirmware device(6, 5);
  device.reportedCount = 4;
  QVERIFY(!device.devicePath().isEmpty());
  CdcTransport transport(device.devicePath());
  QVERIFY(transport.open(true));

  uint8_t index = 0;
  DeviceProfile profile;
  QVERIFY(!transport.getActiveProfile(index, profile));
  QCOMPARE(int(transport.deviceConfig().activeProfile), 5); // as HELLO reported it
}

void BridgeActiveProfileTests::profileReadSkipsLateSuccess()
{
  FakeCdcFirmware device(4);
  QVERIFY(!device.devicePath().isEmpty());
  CdcTransport transport(device.devicePath());
  QVERIFY(transport.open(true));

  device.lateSuccess = true;
  DeviceProfile profile;
  QVERIFY(transport.getProfile(1, profile));
  QVERIFY(!device.lateSuccess); // it was sent
  QCOMPARE(QByteArray(profile.hostname), QByteArray("Host 1"));
  QCOMPARE(int(profile.screenWidth), 1100);
}

void BridgeActiveProfileTests::profileReadFailsOnError()
{
  FakeCdcFirmware device(4);
  QVERIFY(!device.devicePath().isEmpty());
  CdcTransport transport(device.devicePath());
  QVERIFY(transport.open(true));

  device.failProfileReads = true;
  DeviceProfile profile;
  QElapsedTimer timer;
  timer.start();
  QVERIFY(!transport.getProfile(1, profile));
  // only a bare success is skipped: an error ends the wait
  QVERIFY(timer.elapsed() < 500);
}

void BridgeActiveProfileTests::activeProfileAfterSwitchTimedOut()
{
  FakeCdcFirmware device(4);
  QVERIFY(!device.devicePath().isEmpty());
  CdcTransport transport(device.devicePath());
  QVERIFY(transport.open(true));

  // the firmware switches at once, but its reply comes after the transport stopped waiting
  device.switchReplyDelayMs = kLateReplyMs;
  QVERIFY(!transport.switchProfile(2));
  QCOMPARE(int(transport.deviceConfig().activeProfile), 0);

  // the late reply comes while this waits for its own
  uint8_t index = 0;
  DeviceProfile profile;
  QVERIFY(transport.getActiveProfile(index, profile));
  QCOMPARE(int(index), 2);
  QCOMPARE(int(transport.deviceConfig().activeProfile), 2);
}

void BridgeActiveProfileTests::screenStartsAtActiveProfileSize()
{
  FakeCdcFirmware device(4, 1);
  QVERIFY(!device.devicePath().isEmpty());
  auto transport = std::make_shared<CdcTransport>(device.devicePath());
  QVERIFY(transport->open(true));

  // the size the GUI passes when it couldn't read the device
  RecordingEventQueue events;
  BridgePlatformScreen screen(&events, transport, std::make_shared<requests::Queue>(), 1920, 1080);

  QCOMPARE(shapeWidth(screen), 1100);
  QCOMPARE(shapeHeight(screen), 2100);
  // nothing is connected yet to be told
  QVERIFY(events.added.empty());
}

void BridgeActiveProfileTests::screenKeepsGivenSizeWithoutProfile()
{
  FakeCdcFirmware device(4, 1);
  QVERIFY(!device.devicePath().isEmpty());
  auto transport = std::make_shared<CdcTransport>(device.devicePath());
  QVERIFY(transport->open(true));

  device.failProfileReads = true;
  RecordingEventQueue events;
  BridgePlatformScreen screen(&events, transport, std::make_shared<requests::Queue>(), 1920, 1080);

  QCOMPARE(shapeWidth(screen), 1920);
  QCOMPARE(shapeHeight(screen), 1080);
}

void BridgeActiveProfileTests::screenFollowsSwitchThatTimedOut()
{
  FakeCdcFirmware device(4);
  QVERIFY(!device.devicePath().isEmpty());
  auto transport = std::make_shared<CdcTransport>(device.devicePath());
  QVERIFY(transport->open(true));

  RecordingEventQueue events;
  auto queue = std::make_shared<requests::Queue>();
  BridgePlatformScreen screen(&events, transport, queue, 1920, 1080);
  QCOMPARE(shapeWidth(screen), 1000);

  // the GUI asks for profile 2; the device switches, but its reply is too late
  device.switchReplyDelayMs = kLateReplyMs;
  queue->push(requests::formatRequest(1, requests::kSwitch, {QStringLiteral("2")}).toStdString());
  QVERIFY(events.call(EventTypes::StreamInputReady, queue.get()));

  QCOMPARE(shapeWidth(screen), 1200);
  QCOMPARE(shapeHeight(screen), 2200);
  QCOMPARE(int(transport->deviceConfig().activeProfile), 2);
  // so the client sends the server the new size
  QVERIFY(std::find(events.added.begin(), events.added.end(), EventTypes::ComputerShapeChanged) != events.added.end());
}

QTEST_MAIN(BridgeActiveProfileTests)
