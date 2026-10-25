/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#include "base/Log.h"

#include <QTemporaryDir>
#include <QTest>

class BridgeActiveProfileTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();

  // CdcTransport, against FakeCdcFirmware
  void activeProfileComesFromDevice();
  void activeProfileRejectsUnknownSlot();
  void profileReadSkipsLateSuccess();
  void profileReadFailsOnError();
  void activeProfileAfterSwitchTimedOut();

  // BridgePlatformScreen's screen size
  void screenStartsAtActiveProfileSize();
  void screenKeepsGivenSizeWithoutProfile();
  void screenFollowsSwitchThatTimedOut();

private:
  Log m_log;
  // the screen reads the Bluetooth keep-alive setting
  QTemporaryDir m_settingsDir;
};
