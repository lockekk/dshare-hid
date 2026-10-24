/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#include "BridgeFirmwareNotice.h"

#include "common/Settings.h"

#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include <QSettings>
#include <QTimer>
#include <QWidget>

namespace deskflow::gui {

using deskflow::bridge::ActivationState;

namespace {

const QString kFirmwareVersion = QStringLiteral("firmwareVersion");

// the firmware's DEVICE_CONFIG_*_NAME
const QString kDefaultName = QStringLiteral("Deskflow HID");
const QString kTrialName = QStringLiteral("Deskflow HID(trial)");
const QString kExpiredName = QStringLiteral("Deskflow HID(expired)");

} // namespace

QString BridgeFirmwareNotice::noteHandshake(const QString &serialNumber, const deskflow::bridge::FirmwareConfig &config)
{
  const QString serial = serialNumber.trimmed().toUpper();
  const int version = config.firmwareVersionBcd;
  if (serial.isEmpty() || version == 0) {
    return QString();
  }

  QSettings versions(versionsFile(), QSettings::IniFormat);
  versions.beginGroup(serial);
  const QVariant previous = versions.value(kFirmwareVersion);
  versions.setValue(kFirmwareVersion, version);
  versions.endGroup();
  versions.sync();

  if (!previous.isValid() || previous.toInt() >= kPairingResetVersion || version < kPairingResetVersion) {
    return QString();
  }
  return tr("Pairings were reset. On each host, forget \"%1\" and pair again with its profile active.")
      .arg(advertisedName(config.activationState, QString::fromStdString(config.deviceName)));
}

QString BridgeFirmwareNotice::advertisedName(ActivationState state, const QString &deviceName)
{
  switch (state) {
  case ActivationState::FreeTrial:
    return kTrialName;
  case ActivationState::Inactive:
    return kExpiredName;
  case ActivationState::Activated:
    return deviceName.trimmed().isEmpty() ? kDefaultName : deviceName.trimmed();
  default:
    // the firmware doesn't advertise then: the name a host paired with it most likely shows
    return kDefaultName;
  }
}

void BridgeFirmwareNotice::show(QWidget *parent, const QString &notice)
{
  if (notice.isEmpty()) {
    return;
  }
  QObject *context = parent != nullptr ? static_cast<QObject *>(parent) : qApp;
  QTimer::singleShot(0, context, [parent, notice] { QMessageBox::information(parent, tr("Pair Again"), notice); });
}

QString BridgeFirmwareNotice::versionsFile()
{
  return QDir(Settings::settingsPath()).filePath(QStringLiteral("bridge-firmware.ini"));
}

} // namespace deskflow::gui
