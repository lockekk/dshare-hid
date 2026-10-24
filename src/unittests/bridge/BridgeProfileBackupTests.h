/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#include <QTest>

class BridgeProfileBackupTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void readsScriptBackup();
  void deviceNameIsOptional();
  void acceptsMatchingSlotAndUnknownKeys();
  void roundTrip();
  void skipsInvalidDeviceName();
  void rejectsInvalidFile_data();
  void rejectsInvalidFile();
  void reportsEveryProblem();
  void fitsDevice();
  void recognisesUnsetProfiles();
  void restoreChecksBeforeTheDevice();
};
