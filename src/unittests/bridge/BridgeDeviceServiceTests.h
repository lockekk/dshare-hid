/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#include <QTest>

class BridgeDeviceServiceTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void writeSavesEachProfile();
  void writeSwitchesOnlyIfNeedBe();
  void writeChecksBeforeTheDevice();
  void writeCleansProfiles();
  void writeRefusesWhatDoesntFit();
  void writeCatchesBadReadBack();
  void writeStopsAtFailedSave();
  void writeReportsFailedSwitch();
  void backupHoldsSetProfiles();
  void backupWithoutName();
  void backupFailsWithoutProfiles();
};
