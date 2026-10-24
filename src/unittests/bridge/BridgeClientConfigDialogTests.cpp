/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#include "BridgeClientConfigDialogTests.h"

#include "FakeBridgeDevice.h"

#include "common/Settings.h"
#include "gui/dialogs/BridgeClientConfigDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QDate>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSpinBox>
#include <QTabBar>
#include <QTimer>

#include <cstring>
#include <functional>

using namespace deskflow::bridge;
using namespace deskflow::gui;

/**
 * @brief Answers the message boxes and file dialogs that the dialog opens, in turn, from inside
 * their own event loops
 *
 * One more than a test expects is closed and noted, so the test fails rather than hangs.
 */
class Modals : public QObject
{
public:
  Modals()
  {
    m_timer.setInterval(5);
    connect(&m_timer, &QTimer::timeout, this, [this] { answer(); });
    m_timer.start();
  }

  /// press a button of the next message box
  void press(QMessageBox::StandardButton button)
  {
    m_answers.append([this, button](QWidget *modal) {
      auto *box = qobject_cast<QMessageBox *>(modal);
      if (box == nullptr || box->button(button) == nullptr) {
        closeUnexpected(modal);
        return;
      }
      texts.append(box->text());
      box->button(button)->click();
    });
  }

  /// choose a file in the next file dialog
  void choose(const QString &path)
  {
    m_answers.append([this, path](QWidget *modal) {
      auto *dialog = qobject_cast<QFileDialog *>(modal);
      if (dialog == nullptr) {
        closeUnexpected(modal);
        return;
      }
      suggested.append(dialog->selectedFiles().value(0));
      // selectFile() leaves the file name box alone while it has focus, as it can by now
      dialog->selectFile(path);
      if (auto *name = dialog->findChild<QLineEdit *>(QStringLiteral("fileNameEdit"))) {
        name->setText(QFileInfo(path).fileName());
      }
      static_cast<QDialog *>(dialog)->accept(); // QFileDialog's own, which checks the file
    });
  }

  bool allAnswered() const
  {
    return m_answers.isEmpty();
  }

  /// each message box's text, in turn
  QStringList texts;
  /// the file each file dialog first had selected
  QStringList suggested;
  QStringList unexpected;

private:
  void answer()
  {
    QWidget *modal = QApplication::activeModalWidget();
    if (modal == nullptr || !modal->isVisible()) {
      return;
    }
    if (modal == m_answered) {
      // still open half a second after its answer
      if (++m_pollsSinceAnswer > 100) {
        closeUnexpected(modal);
      }
      return;
    }
    m_answered = modal;
    m_pollsSinceAnswer = 0;
    if (m_answers.isEmpty()) {
      closeUnexpected(modal);
      return;
    }
    m_answers.takeFirst()(modal);
  }

  void closeUnexpected(QWidget *modal)
  {
    const auto *box = qobject_cast<QMessageBox *>(modal);
    unexpected.append(box != nullptr ? box->text() : QString::fromLatin1(modal->metaObject()->className()));
    if (auto *dialog = qobject_cast<QDialog *>(modal)) {
      dialog->reject();
    } else {
      modal->close();
    }
  }

  QTimer m_timer;
  QPointer<QWidget> m_answered;
  int m_pollsSinceAnswer = 0;
  QList<std::function<void(QWidget *)>> m_answers;
};

namespace {

const QString kDevice = QStringLiteral("fake-device");

template <typename T> T *child(const QWidget &parent, const char *name)
{
  auto *widget = parent.findChild<T *>(QString::fromLatin1(name));
  if (widget == nullptr) {
    qFatal("the dialog has no %s", name);
  }
  return widget;
}

QTabBar *tabs(const QWidget &dialog)
{
  return child<QTabBar>(dialog, "profileTabs");
}

QLineEdit *hostname(const QWidget &dialog)
{
  return child<QLineEdit>(dialog, "profileHostname");
}

void clickOk(const QWidget &dialog)
{
  dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
}

void clickCancel(const QWidget &dialog)
{
  dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Cancel)->click();
}

bool sameProfile(const DeviceProfile &a, const DeviceProfile &b)
{
  return std::memcmp(&a, &b, sizeof(DeviceProfile)) == 0;
}

QString hostnameOf(const DeviceProfile &profile)
{
  return BridgeDeviceService::profileHostname(profile);
}

} // namespace

BridgeClientConfigDialogTests::BridgeClientConfigDialogTests() = default;
BridgeClientConfigDialogTests::~BridgeClientConfigDialogTests() = default;

void BridgeClientConfigDialogTests::initTestCase()
{
  // before the settings are first read, which would otherwise open the user's own file
  QVERIFY(m_dir.isValid());
  qputenv("XDG_CONFIG_HOME", QFile::encodeName(m_dir.path()));
  qputenv("XDG_STATE_HOME", QFile::encodeName(m_dir.path()));
  Settings::setSettingsFile(m_dir.filePath(QStringLiteral("test.conf")));
  Settings::setStateFile(m_dir.filePath(QStringLiteral("test.state")));
  m_arch.init();
}

void BridgeClientConfigDialogTests::init()
{
  // Pixel keeps the firmware's default scroll scale, 0; Ipad air is active
  m_device = std::make_unique<FakeBridgeDevice>(4);
  m_device->profiles[0] = setProfile(0, "Pixel", 1080, 2424);
  auto ipad = setProfile(1, "Ipad air", 1640, 2360, 1);
  ipad.hidMode = 1;
  ipad.invert = 1;
  ipad.yScrollScale = 15;
  m_device->profiles[1] = ipad;
  m_device->active = 1;
  m_device->name = QStringLiteral("Desk Bridge");

  m_configPath = m_dir.filePath(QStringLiteral("Bridge-TEST.conf"));
  QFile::remove(m_configPath);
  QSettings config(m_configPath, QSettings::IniFormat);
  config.setValue(Settings::Core::ComputerName, QStringLiteral("Bridge-TEST"));
  config.setValue(Settings::Bridge::DeviceName, QStringLiteral("Desk Bridge"));
  config.setValue(Settings::Bridge::ActivationState, QStringLiteral("activated"));
  config.setValue(Settings::Bridge::SerialNumber, QStringLiteral("TESTSN"));
  config.sync();

  m_modals = std::make_unique<Modals>();
}

void BridgeClientConfigDialogTests::cleanup()
{
  const QStringList unexpected = m_modals->unexpected;
  const bool allAnswered = m_modals->allAnswered();
  m_modals.reset();
  m_device.reset();
  QVERIFY2(unexpected.isEmpty(), qPrintable(QStringLiteral("unexpected: ") + unexpected.join(QStringLiteral(" | "))));
  QVERIFY2(allAnswered, "a message box or file dialog the test expected didn't open");
}

std::unique_ptr<BridgeClientConfigDialog> BridgeClientConfigDialogTests::openDialog() const
{
  auto dialog = std::make_unique<BridgeClientConfigDialog>(m_configPath, kDevice);
  // the profiles are read once the dialog is up
  if (!QTest::qWaitFor([&dialog] { return tabs(*dialog)->count() > 0; }, 1000)) {
    qWarning("the dialog read no profiles");
  }
  return dialog;
}

QString BridgeClientConfigDialogTests::writeFile(const QString &name, const QByteArray &contents) const
{
  const QString path = m_dir.filePath(name);
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(contents) != contents.size()) {
    qFatal("can't write %s", qPrintable(path));
  }
  return path;
}

void BridgeClientConfigDialogTests::showsTheDeviceProfiles()
{
  auto dialog = openDialog();
  auto *tabBar = tabs(*dialog);
  QCOMPARE(tabBar->count(), 4);
  QCOMPARE(tabBar->tabText(0), QStringLiteral("Pixel"));
  QCOMPARE(tabBar->tabText(1), QStringLiteral("Ipad air (Active)"));
  QCOMPARE(tabBar->tabText(2), QStringLiteral("Slot 2"));
  QCOMPARE(tabBar->tabText(3), QStringLiteral("Slot 3"));

  // the active profile is shown
  QCOMPARE(tabBar->currentIndex(), 1);
  QCOMPARE(hostname(*dialog)->text(), QStringLiteral("Ipad air"));
  QCOMPARE(child<QSpinBox>(*dialog, "profileWidth")->value(), 1640);
  QCOMPARE(child<QSpinBox>(*dialog, "profileHeight")->value(), 2360);
  QVERIFY(child<QRadioButton>(*dialog, "profileLandscape")->isChecked());
  QVERIFY(child<QRadioButton>(*dialog, "profileMouseOnly")->isChecked());
  QCOMPARE(child<QDoubleSpinBox>(*dialog, "profileScrollScale")->value(), 1.5);
  QVERIFY(child<QCheckBox>(*dialog, "profileInvertScroll")->isChecked());

  QCOMPARE(m_device->received, QStringList{"profiles"});
}

void BridgeClientConfigDialogTests::okWithoutChangesSendsNothing()
{
  auto dialog = openDialog();
  // showing a profile fills in the form, which isn't an edit (Pixel's scroll scale of 0 is shown
  // as 1.0)
  for (int index : {0, 2, 3, 1}) {
    tabs(*dialog)->setCurrentIndex(index);
  }
  clickOk(*dialog);

  QCOMPARE(dialog->result(), int(QDialog::Accepted));
  QCOMPARE(m_device->received, QStringList{"profiles"});
}

void BridgeClientConfigDialogTests::editIsSavedOnOk()
{
  auto dialog = openDialog();
  hostname(*dialog)->setText(QStringLiteral("Ipad mini"));
  QCOMPARE(tabs(*dialog)->tabText(1), QStringLiteral("Ipad mini (Active)"));
  QCOMPARE(m_device->received, QStringList{"profiles"}); // not yet

  m_modals->press(QMessageBox::Apply);
  clickOk(*dialog);

  QCOMPARE(dialog->result(), int(QDialog::Accepted));
  QCOMPARE(m_modals->texts.size(), qsizetype(1));
  QVERIFY2(m_modals->texts.at(0).contains("Save Ipad mini."), qPrintable(m_modals->texts.at(0)));
  QVERIFY(!m_modals->texts.at(0).contains("Active profile"));
  QCOMPARE(m_device->received, (QStringList{"profiles", "profiles", "save 1"}));

  const auto &saved = m_device->profiles.at(1);
  QCOMPARE(hostnameOf(saved), QStringLiteral("Ipad mini"));
  QCOMPARE(saved.screenWidth, uint16_t(1640));
  QCOMPARE(saved.rotation, uint8_t(1));
  QCOMPARE(saved.hidMode, uint8_t(1));
  QCOMPARE(saved.invert, uint8_t(1));
  QCOMPARE(saved.yScrollScale, uint8_t(15));
}

void BridgeClientConfigDialogTests::revertedEditSendsNothing()
{
  auto dialog = openDialog();
  hostname(*dialog)->setText(QStringLiteral("Ipad mini"));
  child<QSpinBox>(*dialog, "profileWidth")->setValue(1200);
  hostname(*dialog)->setText(QStringLiteral("Ipad air"));
  child<QSpinBox>(*dialog, "profileWidth")->setValue(1640);
  clickOk(*dialog);

  QCOMPARE(dialog->result(), int(QDialog::Accepted));
  QCOMPARE(m_device->received, QStringList{"profiles"});
}

void BridgeClientConfigDialogTests::editKeepsWhatTheFormDoesntShow()
{
  auto dialog = openDialog();
  tabs(*dialog)->setCurrentIndex(0);
  hostname(*dialog)->setText(QStringLiteral("Phone"));
  tabs(*dialog)->setCurrentIndex(1); // the active one, so it stays active

  m_modals->press(QMessageBox::Apply);
  clickOk(*dialog);

  QCOMPARE(m_device->received, (QStringList{"profiles", "profiles", "save 0"}));
  auto expected = setProfile(0, "Phone", 1080, 2424);
  QVERIFY(sameProfile(m_device->profiles.at(0), expected)); // scroll scale still 0, speed still 30
}

void BridgeClientConfigDialogTests::summaryCancelSendsNothing()
{
  auto dialog = openDialog();
  hostname(*dialog)->setText(QStringLiteral("Ipad mini"));

  m_modals->press(QMessageBox::Cancel);
  clickOk(*dialog);
  QCOMPARE(dialog->result(), int(QDialog::Rejected)); // still open
  QCOMPARE(m_device->received, QStringList{"profiles"});

  // the edit is still there for the next OK
  m_modals->press(QMessageBox::Apply);
  clickOk(*dialog);
  QCOMPARE(dialog->result(), int(QDialog::Accepted));
  QCOMPARE(hostnameOf(m_device->profiles.at(1)), QStringLiteral("Ipad mini"));
}

void BridgeClientConfigDialogTests::shownProfileBecomesActive()
{
  auto dialog = openDialog();
  tabs(*dialog)->setCurrentIndex(0);

  m_modals->press(QMessageBox::Apply);
  clickOk(*dialog);

  QCOMPARE(dialog->result(), int(QDialog::Accepted));
  QVERIFY2(m_modals->texts.at(0).contains("Active profile changes to Pixel."), qPrintable(m_modals->texts.at(0)));
  QVERIFY(!m_modals->texts.at(0).contains("Save"));
  QCOMPARE(m_device->received, (QStringList{"profiles", "switch 0"}));
  QCOMPARE(m_device->active, 0);
}

void BridgeClientConfigDialogTests::editThenSwitchSavesFirst()
{
  auto dialog = openDialog();
  tabs(*dialog)->setCurrentIndex(0);
  hostname(*dialog)->setText(QStringLiteral("Phone"));

  m_modals->press(QMessageBox::Apply);
  clickOk(*dialog);

  const QString summary = m_modals->texts.at(0);
  QVERIFY2(summary.contains("Save Phone."), qPrintable(summary));
  QVERIFY(summary.contains("Active profile changes to Phone."));
  QCOMPARE(m_device->received, (QStringList{"profiles", "profiles", "save 0", "switch 0"}));
  QCOMPARE(m_device->active, 0);
}

void BridgeClientConfigDialogTests::resetWaitsForOk()
{
  auto dialog = openDialog();
  tabs(*dialog)->setCurrentIndex(0);
  child<QPushButton>(*dialog, "profileReset")->click();

  // nothing sent, and the form shows the profile as it will be
  QCOMPARE(m_device->received, QStringList{"profiles"});
  QCOMPARE(hostname(*dialog)->text(), QStringLiteral("Slot 0"));
  QCOMPARE(child<QSpinBox>(*dialog, "profileWidth")->value(), 1080);
  QCOMPARE(child<QSpinBox>(*dialog, "profileHeight")->value(), 2424);
  QVERIFY(child<QRadioButton>(*dialog, "profilePortrait")->isChecked());
  QVERIFY(child<QRadioButton>(*dialog, "profileCombo")->isChecked());
  QCOMPARE(child<QDoubleSpinBox>(*dialog, "profileScrollScale")->value(), 1.0);
  QVERIFY(!child<QCheckBox>(*dialog, "profileInvertScroll")->isChecked());
  QCOMPARE(tabs(*dialog)->tabText(0), QStringLiteral("Slot 0"));

  tabs(*dialog)->setCurrentIndex(1);
  m_modals->press(QMessageBox::Apply);
  clickOk(*dialog);

  const QString summary = m_modals->texts.at(0);
  QVERIFY2(summary.contains("Reset Pixel. Its host must pair again manually."), qPrintable(summary));
  QVERIFY(!summary.contains("Save"));
  QCOMPARE(m_device->received, (QStringList{"profiles", "erase 0"}));
  QCOMPARE(m_device->unpaired, QList<int>{0});
  QVERIFY(BridgeProfileBackup::isUnset(0, m_device->profiles.at(0)));
}

void BridgeClientConfigDialogTests::resetThenEditErasesFirst()
{
  auto dialog = openDialog();
  tabs(*dialog)->setCurrentIndex(0);
  child<QPushButton>(*dialog, "profileReset")->click();
  hostname(*dialog)->setText(QStringLiteral("Phone"));
  tabs(*dialog)->setCurrentIndex(1);

  m_modals->press(QMessageBox::Apply);
  clickOk(*dialog);

  // listed in the order they are made
  const QString summary = m_modals->texts.at(0);
  QVERIFY2(summary.contains("Reset Pixel.") && summary.contains("Save Phone."), qPrintable(summary));
  QVERIFY(summary.indexOf("Reset Pixel.") < summary.indexOf("Save Phone."));
  QCOMPARE(m_device->received, (QStringList{"profiles", "erase 0", "profiles", "save 0"}));
  QCOMPARE(m_device->unpaired, QList<int>{0});
  QCOMPARE(hostnameOf(m_device->profiles.at(0)), QStringLiteral("Phone"));
}

void BridgeClientConfigDialogTests::cancelKeepsResetProfile()
{
  auto dialog = openDialog();
  tabs(*dialog)->setCurrentIndex(0);
  child<QPushButton>(*dialog, "profileReset")->click();
  clickCancel(*dialog);

  QCOMPARE(dialog->result(), int(QDialog::Rejected));
  QCOMPARE(m_device->received, QStringList{"profiles"});
  QCOMPARE(hostnameOf(m_device->profiles.at(0)), QStringLiteral("Pixel"));
  QVERIFY(m_device->unpaired.isEmpty());
}

void BridgeClientConfigDialogTests::invalidEditIsRefused()
{
  auto dialog = openDialog();
  hostname(*dialog)->setText(QString());

  m_modals->press(QMessageBox::Ok);
  clickOk(*dialog);

  QCOMPARE(dialog->result(), int(QDialog::Rejected)); // still open
  const QString warning = m_modals->texts.at(0);
  QVERIFY2(warning.contains("Please fix these profiles first:"), qPrintable(warning));
  QVERIFY(warning.contains("Profile 1: the hostname"));
  QCOMPARE(m_device->received, QStringList{"profiles"});
}

void BridgeClientConfigDialogTests::failedApplyKeepsDialogOpen()
{
  auto dialog = openDialog();
  m_device->failing = QStringLiteral("save 1");
  hostname(*dialog)->setText(QStringLiteral("Ipad mini"));

  m_modals->press(QMessageBox::Apply);
  m_modals->press(QMessageBox::Ok);
  clickOk(*dialog);
  QCOMPARE(dialog->result(), int(QDialog::Rejected));
  QVERIFY2(m_modals->texts.at(1).contains("Failed to update the device:"), qPrintable(m_modals->texts.at(1)));

  // OK tries again
  m_device->failing.clear();
  m_modals->press(QMessageBox::Apply);
  clickOk(*dialog);
  QCOMPARE(dialog->result(), int(QDialog::Accepted));
  QCOMPARE(hostnameOf(m_device->profiles.at(1)), QStringLiteral("Ipad mini"));
}

void BridgeClientConfigDialogTests::restoreLoadsIntoTheForm()
{
  // Pixel as the device has it, so it isn't saved again
  BridgeProfileBackup backup;
  backup.activeProfile = 2;
  backup.deviceName = QStringLiteral("Home Bridge");
  backup.profiles.insert(0, m_device->profiles.at(0));
  const auto laptop = setProfile(2, "Work Laptop", 2560, 1600, 1);
  backup.profiles.insert(2, laptop);
  const QString path = writeFile(QStringLiteral("backup.json"), backup.toJson());

  auto dialog = openDialog();
  m_modals->choose(path);
  m_modals->press(QMessageBox::Ok);
  child<QPushButton>(*dialog, "profileRestore")->click();

  QVERIFY2(m_modals->texts.at(0).contains("Backup loaded. Click OK to apply."), qPrintable(m_modals->texts.at(0)));
  QCOMPARE(m_device->received, QStringList{"profiles"}); // nothing sent
  QCOMPARE(tabs(*dialog)->currentIndex(), 2);
  QCOMPARE(tabs(*dialog)->tabText(2), QStringLiteral("Work Laptop"));
  QCOMPARE(hostname(*dialog)->text(), QStringLiteral("Work Laptop"));
  QCOMPARE(child<QSpinBox>(*dialog, "profileWidth")->value(), 2560);
  QCOMPARE(child<QLineEdit>(*dialog, "deviceName")->text(), QStringLiteral("Home Bridge"));

  m_modals->press(QMessageBox::Apply);
  clickOk(*dialog);

  QCOMPARE(dialog->result(), int(QDialog::Accepted));
  const QString summary = m_modals->texts.at(1);
  QVERIFY2(summary.contains("Save Work Laptop."), qPrintable(summary));
  QVERIFY(!summary.contains("Pixel"));
  QVERIFY(summary.contains("Active profile changes to Work Laptop."));
  QVERIFY(summary.contains("Device name changes to Home Bridge."));
  QCOMPARE(m_device->received, (QStringList{"profiles", "profiles", "save 2", "switch 2"}));
  QVERIFY(sameProfile(m_device->profiles.at(2), laptop));
  QCOMPARE(m_device->active, 2);
  // the rename itself is the main window's, once the dialog is accepted
  QVERIFY(dialog->deviceNameChanged());
  QCOMPARE(dialog->deviceName(), QStringLiteral("Home Bridge"));
}

void BridgeClientConfigDialogTests::restoreRefusesInvalidFile()
{
  BridgeProfileBackup backup;
  backup.activeProfile = 0;
  backup.profiles.insert(0, setProfile(0, "Phone", 1080, 2424));
  QJsonObject root = QJsonDocument::fromJson(backup.toJson()).object();
  QJsonArray profiles = root.value("profiles").toArray();
  QJsonObject phone = profiles.at(0).toObject();
  phone.insert("hid_mode", 2);
  profiles.replace(0, phone);
  root.insert("profiles", profiles);
  const QString path = writeFile(QStringLiteral("invalid.json"), QJsonDocument(root).toJson());

  auto dialog = openDialog();
  m_modals->choose(path);
  m_modals->press(QMessageBox::Ok);
  child<QPushButton>(*dialog, "profileRestore")->click();

  const QString warning = m_modals->texts.at(0);
  QVERIFY2(warning.contains("This file can't be restored:"), qPrintable(warning));
  QVERIFY(warning.contains("\"hid_mode\""));
  // the form is as it was
  QCOMPARE(tabs(*dialog)->currentIndex(), 1);
  QCOMPARE(hostname(*dialog)->text(), QStringLiteral("Ipad air"));
  QCOMPARE(tabs(*dialog)->tabText(0), QStringLiteral("Pixel"));

  clickOk(*dialog);
  QCOMPARE(dialog->result(), int(QDialog::Accepted));
  QCOMPARE(m_device->received, QStringList{"profiles"});
}

void BridgeClientConfigDialogTests::restoreRefusesLargerBackup()
{
  BridgeProfileBackup backup;
  backup.activeProfile = 0;
  backup.profiles.insert(5, setProfile(5, "Spare", 1080, 2424));
  const QString path = writeFile(QStringLiteral("larger.json"), backup.toJson());

  auto dialog = openDialog();
  m_modals->choose(path);
  m_modals->press(QMessageBox::Ok);
  child<QPushButton>(*dialog, "profileRestore")->click();

  QVERIFY2(m_modals->texts.at(0).contains("This device has no profile 5."), qPrintable(m_modals->texts.at(0)));
  clickOk(*dialog);
  QCOMPARE(m_device->received, QStringList{"profiles"});
}

void BridgeClientConfigDialogTests::restoreDropsPendingReset()
{
  BridgeProfileBackup backup;
  backup.activeProfile = 1;
  backup.profiles.insert(0, m_device->profiles.at(0));
  const QString path = writeFile(QStringLiteral("pixel.json"), backup.toJson());

  auto dialog = openDialog();
  tabs(*dialog)->setCurrentIndex(0);
  child<QPushButton>(*dialog, "profileReset")->click();
  m_modals->choose(path);
  m_modals->press(QMessageBox::Ok);
  child<QPushButton>(*dialog, "profileRestore")->click();

  // the backup's Pixel replaces the reset, and is what the device has: nothing to change
  QCOMPARE(tabs(*dialog)->tabText(0), QStringLiteral("Pixel"));
  clickOk(*dialog);
  QCOMPARE(dialog->result(), int(QDialog::Accepted));
  QCOMPARE(m_device->received, QStringList{"profiles"});
  QVERIFY(m_device->unpaired.isEmpty());
}

void BridgeClientConfigDialogTests::backupSavesTheDeviceProfiles()
{
  const QString path = m_dir.filePath(QStringLiteral("saved.json"));
  QFile::remove(path);

  auto dialog = openDialog();
  hostname(*dialog)->setText(QStringLiteral("Ipad mini")); // not sent, so not in the backup
  m_modals->choose(path);
  m_modals->press(QMessageBox::Ok);
  child<QPushButton>(*dialog, "profileBackup")->click();

  QCOMPARE(m_device->received, (QStringList{"profiles", "profiles", "getname"}));
  const QString suggested = QFileInfo(m_modals->suggested.value(0)).fileName();
  QVERIFY2(
      suggested.startsWith(QStringLiteral("Bridge-TEST-profiles-%1").arg(QDate::currentDate().toString("yyyyMMdd"))),
      qPrintable(suggested)
  );
  const QString saved = m_modals->texts.at(0);
  QVERIFY2(saved.contains("Backup saved to:"), qPrintable(saved));
  QVERIFY(saved.contains(QDir::toNativeSeparators(path)));

  QFile file(path);
  QVERIFY(file.open(QIODevice::ReadOnly));
  BridgeProfileBackup read;
  QStringList problems;
  QVERIFY2(BridgeProfileBackup::fromJson(file.readAll(), read, problems), qPrintable(problems.join('\n')));
  QCOMPARE(read.profiles.keys(), (QList<int>{0, 1}));
  QVERIFY(sameProfile(read.profiles.value(0), m_device->profiles.at(0)));
  QVERIFY(sameProfile(read.profiles.value(1), m_device->profiles.at(1)));
  QCOMPARE(hostnameOf(read.profiles.value(1)), QStringLiteral("Ipad air"));
  QCOMPARE(read.activeProfile, 1);
  QCOMPARE(read.deviceName, QStringLiteral("Desk Bridge"));

  clickCancel(*dialog);
}

void BridgeClientConfigDialogTests::backupRefusesProfileRestoreWouldReject()
{
  // set by the script, with a hostname the dialog doesn't allow
  m_device->profiles[0] = setProfile(0, "Jos\xc3\xa9", 1080, 2424);

  auto dialog = openDialog();
  m_modals->press(QMessageBox::Ok);
  child<QPushButton>(*dialog, "profileBackup")->click();

  // no file dialog: the device's profiles are checked first
  const QString warning = m_modals->texts.at(0);
  QVERIFY2(warning.contains("Please fix these profiles first:"), qPrintable(warning));
  QVERIFY(warning.contains("Profile 0: the hostname"));
}

void BridgeClientConfigDialogTests::backupWithNothingSet()
{
  m_device->profiles[0] = BridgeProfileBackup::unsetProfile(0);
  m_device->profiles[1] = BridgeProfileBackup::unsetProfile(1);
  m_device->active = 0;

  auto dialog = openDialog();
  m_modals->press(QMessageBox::Ok);
  child<QPushButton>(*dialog, "profileBackup")->click();

  QCOMPARE(m_modals->texts.at(0), QStringLiteral("This device has no profiles to backup."));
}

int main(int argc, char **argv)
{
  // no windows on screen, and Qt's own dialogs, which Modals can answer
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
  }
  QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
  QApplication app(argc, argv);
  BridgeClientConfigDialogTests tests;
  return QTest::qExec(&tests, argc, argv);
}
