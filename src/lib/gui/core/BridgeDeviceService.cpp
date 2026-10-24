#include "BridgeDeviceService.h"
#include "common/Settings.h"
#include "gui/core/BridgeClientConfigManager.h"
#include "gui/core/BridgeClientProcess.h"
#include "gui/core/BridgeFirmwareNotice.h"
#include "gui/core/BridgeProfileBackup.h"
#include "platform/bridge/BridgeRequests.h"
#include "platform/bridge/CdcTransport.h"
#include <QDebug>
#include <QEventLoop>
#include <QObject>
#include <QRegularExpression>
#include <QSettings>
#include <QTimer>

#include <cstring>
#include <utility>

namespace deskflow::gui {

namespace requests = deskflow::bridge::requests;

namespace {

bool s_suspended = false;
BridgeDeviceService::RequestHandler s_requestHandler;

// Attempt to open the transport with retries (macOS port readiness can be tricky)
bool openWithRetries(deskflow::bridge::CdcTransport &transport, const QString &devicePath)
{
  for (int retry = 0; retry < 3; ++retry) {
    // checked each time: the wait below runs the event loop, which can start a bridge client
    if (!BridgeDeviceService::mayOpen(devicePath)) {
      qDebug() << "Not opening device" << devicePath << "while a bridge client or the firmware tool owns it";
      return false;
    }
    if (transport.open()) {
      return true;
    }
    qWarning() << "Failed to open device" << devicePath << "(attempt" << (retry + 1) << "/3)";
    transport.close(); // a failed handshake leaves the port open, and so locked, meanwhile

    QEventLoop loop;
    QTimer::singleShot(250, &loop, &QEventLoop::quit);
    loop.exec();
  }
  return false;
}

} // namespace

BridgeDeviceService::BridgeDeviceService(QObject *parent) : QObject(parent)
{
}

bool BridgeDeviceService::syncDeviceConfig(
    const QString &devicePath, const QString &configPath, bool *outIsBleConnected
)
{
  deskflow::bridge::CdcTransport transport(devicePath);

  const bool opened = openWithRetries(transport, devicePath);

  if (!opened) {
    qWarning() << "Aborting sync: Failed to open device" << devicePath << "after retries.";
    Q_EMIT errorOccurred(devicePath, tr("Failed to open device after retries."));
    return false;
  }

  // Ensure we actually got a configuration during the handshake
  if (!transport.hasDeviceConfig()) {
    qWarning() << "Aborting sync: Handshake completed but no device config received for" << devicePath;
    transport.close();
    return false;
  }

  if (outIsBleConnected) {
    *outIsBleConnected = transport.deviceConfig().isBleConnected;
  }

  uint8_t activeProfile = transport.deviceConfig().activeProfile;

  deskflow::bridge::DeviceProfile profile;
  if (!transport.getProfile(activeProfile, profile)) {
    qWarning() << "Failed to get profile" << static_cast<int>(activeProfile) << "from device" << devicePath;
    transport.close();
    return false;
  }

  std::string deviceName = transport.deviceConfig().deviceName;
  // If handshake didn't provide name, try fetch
  if (deviceName.empty()) {
    transport.fetchDeviceName(deviceName);
  }

  // Save activation state before closing transport
  const char *activationStateStr = transport.deviceConfig().activationStateString();
  QString activationState = QString::fromLatin1(activationStateStr);

  // the firmware it runs, for the notice to pair the hosts again after 1.2.1
  auto firmware = transport.deviceConfig();
  firmware.deviceName = deviceName;
  const QString pairingNotice =
      BridgeFirmwareNotice::noteHandshake(BridgeClientConfigManager::readSerialNumber(configPath), firmware);

  transport.close();

  using namespace deskflow;
  QSettings config(configPath, QSettings::IniFormat);

  if (!deviceName.empty()) {
    config.setValue(Settings::Bridge::DeviceName, QString::fromStdString(deviceName));
  }

  config.setValue(Settings::Bridge::ActiveProfileHostname, profileHostname(profile));

  QString orientation = (profile.rotation == 0) ? QStringLiteral("portrait") : QStringLiteral("landscape");
  config.setValue(Settings::Bridge::ActiveProfileOrientation, orientation);

  // Save activation state from device
  config.setValue(Settings::Bridge::ActivationState, activationState);

  config.sync();
  qDebug() << "Successfully synced config for" << devicePath << "Activation State:" << activationState;

  Q_EMIT configSynced(configPath, static_cast<int>(activeProfile));
  if (!pairingNotice.isEmpty()) {
    Q_EMIT pairingsReset(devicePath, pairingNotice);
  }

  return true;
}

bool BridgeDeviceService::applyFirmwareDeviceName(const QString &devicePath, const QString &deviceName)
{
  if (devicePath.isEmpty()) {
    return false;
  }
  if (!isValidDeviceName(deviceName)) {
    return false;
  }

  QStringList fields;
  QString error;
  const bool result = request(devicePath, requests::kSetName, {requests::encodeText(deviceName)}, fields, error);

  if (result) {
    Q_EMIT deviceNameUpdated(devicePath, deviceName);
  }

  return result;
}

bool BridgeDeviceService::fetchFirmwareDeviceName(const QString &devicePath, QString &outName)
{
  if (devicePath.isEmpty()) {
    return false;
  }
  QStringList fields;
  QString error;
  if (!request(devicePath, requests::kGetName, {}, fields, error)) {
    return false;
  }
  outName = requests::decodeText(fields.value(0));
  return true;
}

bool BridgeDeviceService::request(
    const QString &devicePath, const QString &command, const QStringList &arguments, QStringList &fields, QString &error
)
{
  if (s_requestHandler) {
    return s_requestHandler(devicePath, command, arguments, fields, error);
  }
  if (auto *owner = BridgeClientProcess::forDevice(devicePath)) {
    return owner->request(command, arguments, fields, error);
  }

  deskflow::bridge::CdcTransport transport(devicePath);
  if (!openWithRetries(transport, devicePath)) {
    error = s_suspended ? tr("the firmware tool is using the device") : QString::fromStdString(transport.lastError());
    return false;
  }
  const bool ok = requests::execute(transport, command, arguments, fields, error);
  transport.close();
  return ok;
}

bool BridgeDeviceService::readProfiles(const QString &devicePath, BridgeProfileSet &profiles, QString &error)
{
  return requestProfiles(devicePath, requests::kProfiles, {}, profiles, error);
}

bool BridgeDeviceService::readProfiles(
    deskflow::bridge::CdcTransport &transport, BridgeProfileSet &profiles, QString &error
)
{
  QStringList fields;
  return requests::execute(transport, requests::kProfiles, {}, fields, error) &&
         decodeProfileSet(fields, profiles, error);
}

bool BridgeDeviceService::switchProfile(
    const QString &devicePath, int profileIndex, BridgeProfileSet &profiles, QString &error
)
{
  return requestProfiles(devicePath, requests::kSwitch, {QString::number(profileIndex)}, profiles, error);
}

bool BridgeDeviceService::saveProfile(
    const QString &devicePath, int profileIndex, const deskflow::bridge::DeviceProfile &profile,
    BridgeProfileSet &profiles, QString &error
)
{
  return requestProfiles(
      devicePath, requests::kSave, {QString::number(profileIndex), requests::encodeProfile(profile)}, profiles, error
  );
}

bool BridgeDeviceService::eraseProfile(
    const QString &devicePath, int profileIndex, BridgeProfileSet &profiles, QString &error
)
{
  return requestProfiles(devicePath, requests::kErase, {QString::number(profileIndex)}, profiles, error);
}

bool BridgeDeviceService::backupProfiles(const QString &devicePath, BridgeProfileBackup &backup, QString &error)
{
  BridgeProfileSet profiles;
  if (!readProfiles(devicePath, profiles, error)) {
    return false;
  }

  // a backup needn't have the name: without it, the profiles are still worth saving
  QString name;
  QStringList fields;
  QString nameError;
  if (request(devicePath, requests::kGetName, {}, fields, nameError)) {
    name = requests::decodeText(fields.value(0));
  } else {
    qWarning() << "Failed to read the name of" << devicePath << "for its backup:" << nameError;
  }

  backup = BridgeProfileBackup::fromDevice(profiles, name);
  return true;
}

bool BridgeDeviceService::writeProfiles(
    const QString &devicePath, const BridgeProfileBackup &backup, QList<int> &written, BridgeProfileSet &profiles,
    QString &error
)
{
  written.clear();

  // the last check before the device, whoever made the backup: write the profiles as a backup
  // file gives them, every value checked and the rest of each profile zero
  BridgeProfileBackup checked;
  QStringList problems;
  if (!backup.check(checked, problems)) {
    error = problems.join(QLatin1Char(' '));
    return false;
  }

  if (!readProfiles(devicePath, profiles, error)) {
    return false;
  }
  if (!checked.fitsDevice(static_cast<int>(profiles.profiles.size()), problems)) {
    error = problems.join(QLatin1Char(' '));
    return false;
  }

  for (auto it = checked.profiles.cbegin(); it != checked.profiles.cend(); ++it) {
    if (!saveProfile(devicePath, it.key(), it.value(), profiles, error)) {
      error = tr("failed to write profile %1: %2").arg(it.key()).arg(error);
      return false;
    }
    written.append(it.key());

    // the reply has the profile as the device stored it
    if (it.key() >= profiles.profiles.size() ||
        std::memcmp(&profiles.profiles.at(it.key()), &it.value(), sizeof(deskflow::bridge::DeviceProfile)) != 0) {
      error = tr("profile %1 didn't read back as it was written").arg(it.key());
      return false;
    }
  }

  // switching re-applies the profile, which reconnects the host, so only switch if need be
  if (profiles.activeIndex != checked.activeProfile) {
    if (!switchProfile(devicePath, checked.activeProfile, profiles, error)) {
      error = tr("failed to make profile %1 active: %2").arg(checked.activeProfile).arg(error);
      return false;
    }
    if (profiles.activeIndex != checked.activeProfile) {
      error = tr("the device didn't make profile %1 active").arg(checked.activeProfile);
      return false;
    }
  }
  return true;
}

QList<BridgeProfileInfo> BridgeDeviceService::profileInfos(const BridgeProfileSet &profiles)
{
  QList<BridgeProfileInfo> infos;
  for (qsizetype i = 0; i < profiles.profiles.size(); ++i) {
    const auto &profile = profiles.profiles.at(i);
    infos.append({static_cast<int>(i), profileHostname(profile), profile.rotation == 0});
  }
  return infos;
}

bool BridgeDeviceService::requestProfiles(
    const QString &devicePath, const QString &command, const QStringList &arguments, BridgeProfileSet &profiles,
    QString &error
)
{
  QStringList fields;
  return request(devicePath, command, arguments, fields, error) && decodeProfileSet(fields, profiles, error);
}

bool BridgeDeviceService::decodeProfileSet(const QStringList &fields, BridgeProfileSet &profiles, QString &error)
{
  if (!requests::decodeProfiles(fields, profiles.activeIndex, profiles.profiles)) {
    error = tr("the device sent an invalid profile list");
    return false;
  }
  return true;
}

QString BridgeDeviceService::profileHostname(const deskflow::bridge::DeviceProfile &profile)
{
  QByteArray nameBytes(profile.hostname, sizeof(profile.hostname));
  int nullPos = nameBytes.indexOf('\0');
  if (nullPos >= 0) {
    nameBytes.truncate(nullPos);
  }
  return QString::fromUtf8(nameBytes);
}

bool BridgeDeviceService::isValidDeviceName(const QString &deviceName)
{
  if (deviceName.length() > 22) {
    return false;
  }
  static QRegularExpression regex(R"(^[a-zA-Z0-9 ._-]+$)");
  return regex.match(deviceName).hasMatch();
}

void BridgeDeviceService::setRequestHandler(RequestHandler handler)
{
  s_requestHandler = std::move(handler);
}

void BridgeDeviceService::setSuspended(bool suspended)
{
  s_suspended = suspended;
}

bool BridgeDeviceService::mayOpen(const QString &devicePath)
{
  return !s_suspended && BridgeClientProcess::forDevice(devicePath) == nullptr;
}

} // namespace deskflow::gui
