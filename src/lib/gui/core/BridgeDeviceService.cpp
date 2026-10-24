#include "BridgeDeviceService.h"
#include "common/Settings.h"
#include "gui/core/BridgeClientConfigManager.h"
#include "platform/bridge/CdcTransport.h"
#include <QDebug>
#include <QEventLoop>
#include <QObject>
#include <QRegularExpression>
#include <QSettings>
#include <QTimer>

namespace deskflow::gui {

namespace {

// Attempt to open the transport with retries (macOS port readiness can be tricky)
bool openWithRetries(deskflow::bridge::CdcTransport &transport, const QString &devicePath)
{
  for (int retry = 0; retry < 3; ++retry) {
    if (transport.open()) {
      return true;
    }
    qWarning() << "Failed to open device" << devicePath << "(attempt" << (retry + 1) << "/3)";

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

  deskflow::bridge::CdcTransport transport(devicePath);
  if (!transport.open()) {
    return false;
  }

  const bool result = transport.setDeviceName(deviceName.toStdString());
  transport.close();

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
  deskflow::bridge::CdcTransport transport(devicePath);
  if (!transport.open()) {
    return false;
  }
  std::string deviceName;
  bool ok = transport.fetchDeviceName(deviceName);
  transport.close();
  if (ok) {
    outName = QString::fromStdString(deviceName);
  }
  return ok;
}

bool BridgeDeviceService::switchProfile(const QString &devicePath, int profileIndex)
{
  if (devicePath.isEmpty()) {
    return false;
  }
  deskflow::bridge::CdcTransport transport(devicePath);
  if (!openWithRetries(transport, devicePath)) {
    return false;
  }
  const bool switched = transport.switchProfile(static_cast<uint8_t>(profileIndex));
  if (!switched) {
    qWarning() << "Failed to switch device" << devicePath << "to profile" << profileIndex << ":"
               << QString::fromStdString(transport.lastError());
  }
  transport.close();
  return switched;
}

QList<BridgeProfileInfo> BridgeDeviceService::readProfiles(deskflow::bridge::CdcTransport &transport)
{
  QList<BridgeProfileInfo> profiles;
  const int total = transport.deviceConfig().totalProfiles;
  for (int i = 0; i < total; ++i) {
    deskflow::bridge::DeviceProfile profile;
    // the firmware answers for every index below the total (with defaults for an unused one),
    // so a failure means it stopped answering: don't wait out the timeout for each of the rest
    if (!transport.getProfile(static_cast<uint8_t>(i), profile)) {
      qWarning() << "Failed to read profile" << i << "from device";
      break;
    }
    profiles.append({i, profileHostname(profile), profile.rotation == 0});
  }
  return profiles;
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

} // namespace deskflow::gui
