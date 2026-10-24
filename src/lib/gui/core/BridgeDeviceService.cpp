#include "BridgeDeviceService.h"
#include "common/Settings.h"
#include "gui/core/BridgeClientConfigManager.h"
#include "gui/core/BridgeClientProcess.h"
#include "platform/bridge/BridgeRequests.h"
#include "platform/bridge/CdcTransport.h"
#include <QDebug>
#include <QEventLoop>
#include <QObject>
#include <QRegularExpression>
#include <QSettings>
#include <QTimer>

namespace deskflow::gui {

namespace requests = deskflow::bridge::requests;

namespace {

bool s_suspended = false;

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

void BridgeDeviceService::setSuspended(bool suspended)
{
  s_suspended = suspended;
}

bool BridgeDeviceService::mayOpen(const QString &devicePath)
{
  return !s_suspended && BridgeClientProcess::forDevice(devicePath) == nullptr;
}

} // namespace deskflow::gui
