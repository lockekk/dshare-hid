/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#include "BridgeFirmwareNoticeTests.h"

#include "common/Settings.h"
#include "gui/core/BridgeFirmwareNotice.h"

#include <QFile>
#include <QFileInfo>
#include <QSettings>

using namespace deskflow::bridge;
using namespace deskflow::gui;

namespace {

FirmwareConfig firmware(int version, ActivationState state = ActivationState::Activated, const char *name = "")
{
  FirmwareConfig config;
  config.firmwareVersionBcd = static_cast<uint8_t>(version);
  config.activationState = state;
  config.deviceName = name;
  return config;
}

// whether the notice is due for the device's handshake
bool due(const QString &serial, int version)
{
  return !BridgeFirmwareNotice::noteHandshake(serial, firmware(version)).isEmpty();
}

const QString kSerial = QStringLiteral("ACA704D05EA0");

} // namespace

void BridgeFirmwareNoticeTests::initTestCase()
{
  // before the settings are first read, which would otherwise open the user's own file
  QVERIFY(m_settingsDir.isValid());
  qputenv("XDG_CONFIG_HOME", QFile::encodeName(m_settingsDir.path()));
  qputenv("XDG_STATE_HOME", QFile::encodeName(m_settingsDir.path()));
  Settings::setSettingsFile(m_settingsDir.filePath(QStringLiteral("test.conf")));
  Settings::setStateFile(m_settingsDir.filePath(QStringLiteral("test.state")));
}

void BridgeFirmwareNoticeTests::init()
{
  // each test starts with no device seen
  QFile::remove(BridgeFirmwareNotice::versionsFile());
}

void BridgeFirmwareNoticeTests::firstSightGetsNoNotice()
{
  // the app can't know whether a device it first sees at 1.2.1 was ever paired before
  QVERIFY(!due(kSerial, 121));
  QVERIFY(!due(kSerial, 121));
}

void BridgeFirmwareNoticeTests::upgradeTo121GetsNoticeOnce()
{
  QVERIFY(!due(kSerial, 120));
  QVERIFY(!due(kSerial, 120)); // seen again, unchanged
  QVERIFY(due(kSerial, 121));
  QVERIFY(!due(kSerial, 121)); // once only
}

void BridgeFirmwareNoticeTests::upgradeFromDevelopmentBuild()
{
  // development builds before 1.2.1 report 1.0.1
  QVERIFY(!due(kSerial, 101));
  QVERIFY(due(kSerial, 121));
}

void BridgeFirmwareNoticeTests::upgradeWithinNewFirmwareGetsNone()
{
  QVERIFY(!due(kSerial, 120));
  QVERIFY(due(kSerial, 122)); // straight past 1.2.1
  QVERIFY(!due(kSerial, 123));
}

void BridgeFirmwareNoticeTests::upgradeBelow121GetsNone()
{
  QVERIFY(!due(kSerial, 101));
  QVERIFY(!due(kSerial, 120));
}

void BridgeFirmwareNoticeTests::reupgradeAfterDowngrade()
{
  // hosts may have been paired again on the older firmware
  QVERIFY(!due(kSerial, 120));
  QVERIFY(due(kSerial, 121));
  QVERIFY(!due(kSerial, 120));
  QVERIFY(due(kSerial, 121));
}

void BridgeFirmwareNoticeTests::factoryModeIsIgnored()
{
  // factory mode reports no version, and mustn't hide the one from before
  QVERIFY(!due(kSerial, 120));
  QVERIFY(!due(kSerial, 0));
  QVERIFY(due(kSerial, 121));
}

void BridgeFirmwareNoticeTests::noSerialIsIgnored()
{
  QVERIFY(!due(QString(), 120));
  QVERIFY(!due(QString(), 121));
  QVERIFY(!QFileInfo::exists(BridgeFirmwareNotice::versionsFile()));
}

void BridgeFirmwareNoticeTests::serialIgnoresCase()
{
  QVERIFY(!due(QStringLiteral("aca704d05ea0"), 120));
  QVERIFY(due(QStringLiteral(" ACA704D05EA0 "), 121));
}

void BridgeFirmwareNoticeTests::devicesAreKeptApart()
{
  const QString other = QStringLiteral("B0B1B2B3B4B5");
  QVERIFY(!due(kSerial, 120));
  QVERIFY(!due(other, 121)); // first sight
  QVERIFY(due(kSerial, 121));
  QVERIFY(!due(other, 121));
}

void BridgeFirmwareNoticeTests::noticeNamesTheDevice_data()
{
  QTest::addColumn<int>("state");
  QTest::addColumn<QString>("deviceName");
  QTest::addColumn<QString>("advertised");

  // as the firmware advertises: DEVICE_CONFIG_*_NAME, or the name once activated
  QTest::newRow("activated") << int(ActivationState::Activated) << "Desk Bridge" << "Desk Bridge";
  QTest::newRow("activated, no name") << int(ActivationState::Activated) << "" << "Deskflow HID";
  QTest::newRow("trial") << int(ActivationState::FreeTrial) << "Desk Bridge" << "Deskflow HID(trial)";
  QTest::newRow("expired") << int(ActivationState::Inactive) << "Desk Bridge" << "Deskflow HID(expired)";
  QTest::newRow("unknown") << int(ActivationState::Unknown) << "Desk Bridge" << "Deskflow HID";
}

void BridgeFirmwareNoticeTests::noticeNamesTheDevice()
{
  QFETCH(int, state);
  QFETCH(QString, deviceName);
  QFETCH(QString, advertised);

  const auto activation = static_cast<ActivationState>(state);
  QCOMPARE(BridgeFirmwareNotice::advertisedName(activation, deviceName), advertised);

  QVERIFY(!due(kSerial, 120));
  const QString notice =
      BridgeFirmwareNotice::noteHandshake(kSerial, firmware(121, activation, deviceName.toUtf8().constData()));
  QCOMPARE(
      notice, QStringLiteral("Pairings were reset. On each host, forget \"%1\" and pair again with its profile active.")
                  .arg(advertised)
  );
}

void BridgeFirmwareNoticeTests::versionsAreKeptBySettings()
{
  // next to the settings, and not as a *.conf, which the bridge clients' configs are
  QCOMPARE(
      QFileInfo(BridgeFirmwareNotice::versionsFile()).absolutePath(),
      QFileInfo(Settings::settingsPath()).absoluteFilePath()
  );
  QVERIFY(BridgeFirmwareNotice::versionsFile().endsWith(".ini"));

  QVERIFY(!due(kSerial, 120));
  QSettings versions(BridgeFirmwareNotice::versionsFile(), QSettings::IniFormat);
  QCOMPARE(versions.value(kSerial + "/firmwareVersion").toInt(), 120);
}

void BridgeFirmwareNoticeTests::unconfirmedUpgradeIsConfirmedOnce()
{
  // the check before the upgrade noted 1.2.0, then the device didn't come back in time
  QVERIFY(!due(kSerial, 120));
  BridgeFirmwareNotice::noteUnconfirmedUpgrade(kSerial);

  QString notice;
  QVERIFY(BridgeFirmwareNotice::confirmUpgrade(kSerial, firmware(121), notice));
  QVERIFY(!notice.isEmpty());

  QVERIFY(!BridgeFirmwareNotice::confirmUpgrade(kSerial, firmware(121), notice)); // once only
  QVERIFY(notice.isEmpty());
}

void BridgeFirmwareNoticeTests::unconfirmedUpgradeThatDidNotTake()
{
  QVERIFY(!due(kSerial, 120));
  BridgeFirmwareNotice::noteUnconfirmedUpgrade(kSerial);

  QString notice;
  QVERIFY(!BridgeFirmwareNotice::confirmUpgrade(kSerial, firmware(120), notice));
  QVERIFY(notice.isEmpty());
  QVERIFY(!BridgeFirmwareNotice::confirmUpgrade(kSerial, firmware(121), notice)); // nothing waits any more
}

void BridgeFirmwareNoticeTests::confirmNotesNothingWithoutUnconfirmedUpgrade()
{
  // Check for Updates notes no version of its own, so an update made elsewhere gets no notice
  QVERIFY(!due(kSerial, 120));

  QString notice;
  QVERIFY(!BridgeFirmwareNotice::confirmUpgrade(kSerial, firmware(121), notice));
  QVERIFY(notice.isEmpty());

  QSettings versions(BridgeFirmwareNotice::versionsFile(), QSettings::IniFormat);
  QCOMPARE(versions.value(kSerial + "/firmwareVersion").toInt(), 120);
}

void BridgeFirmwareNoticeTests::confirmWaitsOutFactoryMode()
{
  QVERIFY(!due(kSerial, 120));
  BridgeFirmwareNotice::noteUnconfirmedUpgrade(kSerial);

  QString notice;
  QVERIFY(!BridgeFirmwareNotice::confirmUpgrade(kSerial, firmware(0), notice));
  QVERIFY(BridgeFirmwareNotice::confirmUpgrade(kSerial, firmware(121), notice));
  QVERIFY(!notice.isEmpty());
}

void BridgeFirmwareNoticeTests::handshakeSettlesUnconfirmedUpgrade()
{
  // the checks of a later upgrade note the version, which settles the earlier one
  QVERIFY(!due(kSerial, 120));
  BridgeFirmwareNotice::noteUnconfirmedUpgrade(kSerial);
  QVERIFY(due(kSerial, 121));

  QString notice;
  QVERIFY(!BridgeFirmwareNotice::confirmUpgrade(kSerial, firmware(121), notice));
}

QTEST_GUILESS_MAIN(BridgeFirmwareNoticeTests)
