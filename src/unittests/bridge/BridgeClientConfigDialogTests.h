/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#include "arch/Arch.h"
#include "base/Log.h"

#include <QTemporaryDir>
#include <QTest>

#include <memory>

class FakeBridgeDevice;
class Modals;

namespace deskflow::gui {
class BridgeClientConfigDialog;
}

class BridgeClientConfigDialogTests : public QObject
{
  Q_OBJECT
public:
  BridgeClientConfigDialogTests();
  ~BridgeClientConfigDialogTests() override;

private Q_SLOTS:
  void initTestCase();
  void init();
  void cleanup();

  void showsTheDeviceProfiles();
  void okWithoutChangesSendsNothing();
  void editIsSavedOnOk();
  void revertedEditSendsNothing();
  void editKeepsWhatTheFormDoesntShow();
  void summaryCancelSendsNothing();
  void shownProfileBecomesActive();
  void editThenSwitchSavesFirst();
  void resetWaitsForOk();
  void resetThenEditErasesFirst();
  void cancelKeepsResetProfile();
  void invalidEditIsRefused();
  void failedApplyKeepsDialogOpen();
  void restoreLoadsIntoTheForm();
  void restoreRefusesInvalidFile();
  void restoreRefusesLargerBackup();
  void restoreDropsPendingReset();
  void backupSavesTheDeviceProfiles();
  void backupRefusesProfileRestoreWouldReject();
  void backupWithNothingSet();

private:
  std::unique_ptr<deskflow::gui::BridgeClientConfigDialog> openDialog() const;
  QString writeFile(const QString &name, const QByteArray &contents) const;

  // the dialog reads its options through Settings, and some of its paths log
  Arch m_arch;
  Log m_log;
  QTemporaryDir m_dir;
  QString m_configPath;
  std::unique_ptr<FakeBridgeDevice> m_device;
  std::unique_ptr<Modals> m_modals;
};
