/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#include <QTemporaryDir>
#include <QTest>

class BridgeFirmwareNoticeTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void init();

  void firstSightGetsNoNotice();
  void upgradeTo121GetsNoticeOnce();
  void upgradeFromDevelopmentBuild();
  void upgradeWithinNewFirmwareGetsNone();
  void upgradeBelow121GetsNone();
  void reupgradeAfterDowngrade();
  void factoryModeIsIgnored();
  void noSerialIsIgnored();
  void serialIgnoresCase();
  void devicesAreKeptApart();
  void noticeNamesTheDevice_data();
  void noticeNamesTheDevice();
  void versionsAreKeptBySettings();
  void unconfirmedUpgradeIsConfirmedOnce();
  void unconfirmedUpgradeThatDidNotTake();
  void confirmNotesNothingWithoutUnconfirmedUpgrade();
  void confirmWaitsOutFactoryMode();
  void handshakeSettlesUnconfirmedUpgrade();

private:
  QTemporaryDir m_settingsDir;
};
