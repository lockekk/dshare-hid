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

BridgeDeviceService::BridgeDeviceService(QObject *parent) : QObject(parent)
{
}

bool BridgeDeviceService::syncDeviceConfig(
    const QString &devicePath, const QString &configPath, bool *outIsBleConnected
)
{
  deskflow::bridge::CdcTransport transport(devicePath);

  // Attempt to open the transport with retries (macOS port readiness can be tricky)
  bool opened = false;
  for (int retry = 0; retry < 3; ++retry) {
    if (transport.open()) {
      opened = true;
      break;
    }
    qWarning() << "Failed to open device" << devicePath << "(attempt" << (retry + 1) << "/3)";

    QEventLoop loop;
    QTimer::singleShot(250, &loop, &QEventLoop::quit);
    loop.exec();
  }

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

  QByteArray nameBytes(profile.hostname, sizeof(profile.hostname));
  int nullPos = nameBytes.indexOf('\0');
  if (nullPos >= 0) {
    nameBytes.truncate(nullPos);
  }
  config.setValue(Settings::Bridge::ActiveProfileHostname, QString::fromUtf8(nameBytes));

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

bool BridgeDeviceService::isValidDeviceName(const QString &deviceName)
{
  if (deviceName.length() > 22) {
    return false;
  }
  static QRegularExpression regex(R"(^[a-zA-Z0-9 ._-]+$)");
  return regex.match(deviceName).hasMatch();
}

} // namespace deskflow::gui
