/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#include "BridgeFirmwareNotice.h"

#include "common/Settings.h"

#include <QDir>
#include <QSettings>

namespace deskflow::gui {

using deskflow::bridge::ActivationState;

namespace {

const QString kFirmwareVersion = QStringLiteral("firmwareVersion");
const QString kUpgradeUnconfirmed = QStringLiteral("upgradeUnconfirmed");

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
  versions.remove(kUpgradeUnconfirmed);
  versions.endGroup();
  versions.sync();

  if (!previous.isValid() || previous.toInt() >= kPairingResetVersion || version < kPairingResetVersion) {
    return QString();
  }
  return tr("Pairings were reset. On each host, forget \"%1\" and pair again with its profile active.")
      .arg(advertisedName(config.activationState, QString::fromStdString(config.deviceName)));
}

void BridgeFirmwareNotice::noteUnconfirmedUpgrade(const QString &serialNumber)
{
  const QString serial = serialNumber.trimmed().toUpper();
  if (serial.isEmpty()) {
    return;
  }

  QSettings versions(versionsFile(), QSettings::IniFormat);
  versions.beginGroup(serial);
  versions.setValue(kUpgradeUnconfirmed, true);
  versions.endGroup();
  versions.sync();
}

bool BridgeFirmwareNotice::confirmUpgrade(
    const QString &serialNumber, const deskflow::bridge::FirmwareConfig &config, QString &notice
)
{
  notice.clear();
  const QString serial = serialNumber.trimmed().toUpper();
  if (serial.isEmpty()) {
    return false;
  }

  int before = 0;
  {
    QSettings versions(versionsFile(), QSettings::IniFormat);
    versions.beginGroup(serial);
    if (!versions.value(kUpgradeUnconfirmed).toBool()) {
      return false;
    }
    before = versions.value(kFirmwareVersion).toInt();
  }

  // a version of 0 (factory mode) is not noted, so the upgrade stays to be confirmed
  notice = noteHandshake(serial, config);
  return config.firmwareVersionBcd > before;
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

QString BridgeFirmwareNotice::versionsFile()
{
  return QDir(Settings::settingsPath()).filePath(QStringLiteral("bridge-firmware.ini"));
}

} // namespace deskflow::gui
