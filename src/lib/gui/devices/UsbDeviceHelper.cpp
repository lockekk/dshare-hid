/*
 * Deskflow-hid -- created by locke.huang@gmail.com
 */

#include "UsbDeviceHelper.h"

#include "platform/bridge/CdcTransport.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QTextStream>
#include <algorithm>
#include <cwchar>
#include <vector>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
// clang-format off
#include <Windows.h>
#include <SetupAPI.h>
// clang-format on
#include <devguid.h>
#include <initguid.h>
#include <winreg.h>
#endif

namespace deskflow::gui {

namespace {

#ifdef Q_OS_LINUX
QString canonicalUsbDevicePath(const QString &devicePath)
{
  QFileInfo deviceInfo(devicePath);
  QString ttyName = deviceInfo.fileName();

  const QString ttyBasePath = QStringLiteral("/sys/class/tty/%1").arg(ttyName);
  QFileInfo ttyLink(ttyBasePath);
  if (!ttyLink.exists()) {
    qWarning() << "TTY entry does not exist for" << devicePath << ":" << ttyBasePath;
    return QString();
  }

  QFileInfo deviceLink(ttyBasePath + QStringLiteral("/device"));
  return deviceLink.canonicalFilePath();
}

QString readUsbAttribute(const QString &canonicalDevicePath, const QString &attribute)
{
  if (canonicalDevicePath.isEmpty())
    return QString();

  QDir currentDir(canonicalDevicePath);
  constexpr int kMaxTraversal = 6;
  for (int depth = 0; depth < kMaxTraversal; ++depth) {
    const QString candidate = currentDir.absoluteFilePath(attribute);
    QFile file(candidate);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      QTextStream in(&file);
      const QString value = in.readLine().trimmed();
      file.close();
      if (!value.isEmpty())
        return value;
    }
    if (!currentDir.cdUp())
      break;
  }

  return QString();
}
#endif

#ifdef Q_OS_WIN
QString windowsVendorPattern(const QString &vendorId)
{
  return QStringLiteral("VID_%1").arg(vendorId.toUpper());
}

QString windowsProductPattern(const QString &productId)
{
  return QStringLiteral("PID_%1").arg(productId.toUpper());
}

bool matchesHardwareId(const QString &id, const QString &vendorId, const QString &productId)
{
  const QString upper = id.toUpper();
  const QString vendorPattern = windowsVendorPattern(vendorId);
  const QString productPattern = windowsProductPattern(productId);
  if (!upper.contains(vendorPattern)) {
    return false;
  }
  if (productId.isEmpty()) {
    return true;
  }
  return upper.contains(productPattern);
}

bool deviceMatchesBridge(HDEVINFO infoSet, SP_DEVINFO_DATA &infoData, const QString &vendorId, const QString &productId)
{
  std::vector<wchar_t> buffer(1024);
  DWORD propertyType = 0;
  while (true) {
    DWORD requiredSize = 0;
    if (SetupDiGetDeviceRegistryPropertyW(
            infoSet, &infoData, SPDRP_HARDWAREID, &propertyType, reinterpret_cast<PBYTE>(buffer.data()),
            static_cast<DWORD>(buffer.size() * sizeof(wchar_t)), &requiredSize
        )) {
      break;
    }
    DWORD error = GetLastError();
    if (error == ERROR_INSUFFICIENT_BUFFER) {
      const size_t newSize = (requiredSize / sizeof(wchar_t)) + 1;
      buffer.resize(std::max(buffer.size() * 2, newSize));
      continue;
    }
    return false;
  }

  if (buffer.empty()) {
    return false;
  }

  const wchar_t *entry = buffer.data();
  while (*entry != L'\0') {
    const QString hardwareId = QString::fromWCharArray(entry);
    if (matchesHardwareId(hardwareId, vendorId, productId)) {
      return true;
    }
    entry += wcslen(entry) + 1;
  }

  return false;
}

QString getPortName(HDEVINFO infoSet, SP_DEVINFO_DATA &infoData)
{
  HKEY key = SetupDiOpenDevRegKey(infoSet, &infoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
  if (key == INVALID_HANDLE_VALUE) {
    return QString();
  }

  wchar_t buffer[256] = {0};
  DWORD type = 0;
  DWORD size = sizeof(buffer);
  QString portName;

  if (RegQueryValueExW(key, L"PortName", nullptr, &type, reinterpret_cast<LPBYTE>(buffer), &size) == ERROR_SUCCESS &&
      (type == REG_SZ || type == REG_EXPAND_SZ)) {
    portName = QString::fromWCharArray(buffer).trimmed();
  }

  RegCloseKey(key);
  return portName;
}
#endif

} // namespace

QString UsbDeviceHelper::readSerialNumber(const QString &devicePath)
{
#if defined(Q_OS_LINUX) || defined(Q_OS_WIN)
  // Read serial number via CDC command from firmware
  // This ensures we only read when device is not opened by bridge client

  qDebug() << "Reading serial number via CDC for device:" << devicePath;

  try {
    // Create CDC transport instance
    qInfo() << "DebugTrace: Creating transport for" << devicePath;
    deskflow::bridge::CdcTransport transport(devicePath);

    // Check if device is busy by attempting to open it
    // This will fail if the bridge client already has it open
    qInfo() << "DebugTrace: Opening transport for" << devicePath;
    qInfo() << "DebugTrace: Opening transport for" << devicePath;
    if (!transport.open()) {
      qDebug() << "Device is busy or not accessible (likely opened by bridge client):" << devicePath;
      return QString(); // Device is in use, don't block
    }
    qInfo() << "DebugTrace: Transport opened successfully for" << devicePath;

    // Read serial number via CDC command
    std::string serial;
    qInfo() << "DebugTrace: Fetching serial number for" << devicePath;
    if (transport.fetchSerialNumber(serial)) {
      qInfo() << "DebugTrace: Closing transport for" << devicePath;
      transport.close();
      qInfo() << "DebugTrace: Converting serial";
      QString qSerial = QString::fromStdString(serial);
      if (!qSerial.isEmpty()) {
        qDebug() << "Read serial number via CDC for" << devicePath << ":" << qSerial;
        return qSerial;
      } else {
        qDebug() << "Device returned empty serial number:" << devicePath;
        return QString();
      }
    } else {
      qWarning() << "Failed to read serial number via CDC for" << devicePath
                 << "error:" << QString::fromStdString(transport.lastError());
      transport.close();
      return QString();
    }

  } catch (const std::exception &e) {
    qWarning() << "Exception reading serial number via CDC for" << devicePath << "error:" << e.what();
    return QString();
  }

#elif defined(Q_OS_IOS)
  // TODO: Implement CDC serial number reading for iOS
  // iOS has limited access to CDC devices, may need different approach
  qWarning() << "Serial number reading not implemented for iOS platform";
  return QString();

#else
  qWarning() << "Serial number reading not implemented for this platform";
  return QString();
#endif
}

QMap<QString, QString> UsbDeviceHelper::getConnectedDevices(bool queryDevice)
{
  QMap<QString, QString> devices;

#ifdef Q_OS_LINUX
  // Scan /dev for ttyACM* devices
  QDir devDir("/dev");
  QStringList filters;
  filters << "ttyACM*";
  QStringList deviceFiles = devDir.entryList(filters, QDir::System);

  for (const QString &deviceFile : deviceFiles) {
    QString devicePath = QStringLiteral("/dev/%1").arg(deviceFile);

    if (!isSupportedBridgeDevice(devicePath)) {
      qDebug() << "Skipping non-bridge CDC device" << devicePath;
      continue;
    }

    QString serialNumber;
    if (queryDevice) {
      serialNumber = readSerialNumber(devicePath);
    }

    if (serialNumber.isEmpty()) {
      serialNumber = "Unknown";
    }
    devices[devicePath] = serialNumber;
  }

  qDebug() << "Found" << devices.size() << "connected USB CDC devices";
#elif defined(Q_OS_WIN)
  GUID classGuid = GUID_DEVCLASS_PORTS;
  HDEVINFO deviceInfoSet = SetupDiGetClassDevsW(&classGuid, nullptr, nullptr, DIGCF_PRESENT);
  if (deviceInfoSet == INVALID_HANDLE_VALUE) {
    qWarning() << "Failed to enumerate COM ports:" << GetLastError();
    return devices;
  }

  SP_DEVINFO_DATA devInfoData = {};
  devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
  for (DWORD index = 0; SetupDiEnumDeviceInfo(deviceInfoSet, index, &devInfoData); ++index) {
    if (!deviceMatchesBridge(
            deviceInfoSet, devInfoData, UsbDeviceHelper::kEspressifVendorId, UsbDeviceHelper::kEspressifProductId
        )) {
      continue;
    }

    QString portName = getPortName(deviceInfoSet, devInfoData);
    if (portName.isEmpty() || !portName.startsWith(QStringLiteral("COM"), Qt::CaseInsensitive)) {
      continue;
    }

    const QString devicePath = QStringLiteral("\\\\.\\%1").arg(portName);
    QString serialNumber;
    if (queryDevice) {
      serialNumber = readSerialNumber(devicePath);
    }
    if (serialNumber.isEmpty()) {
      // Fallback to port name if serial read failed or skipped
      serialNumber = portName;
    }

    devices[devicePath] = serialNumber;
    qDebug() << "Found bridge device at" << devicePath;
  }

  SetupDiDestroyDeviceInfoList(deviceInfoSet);
  qDebug() << "Enumerated" << devices.size() << "bridge-capable USB CDC device(s) on Windows";
#endif

  return devices;
}

bool UsbDeviceHelper::isSupportedBridgeDevice(const QString &devicePath)
{
#ifdef Q_OS_LINUX
  const QString canonicalPath = canonicalUsbDevicePath(devicePath);
  const QString vendorId = readUsbAttribute(canonicalPath, QStringLiteral("idVendor")).toLower();
  const QString productId = readUsbAttribute(canonicalPath, QStringLiteral("idProduct")).toLower();
  if (vendorId.isEmpty())
    return false;

  if (vendorId == kEspressifVendorId) {
    if (productId.isEmpty() || productId == kEspressifProductId) {
      return true;
    }
    qDebug() << "Device" << devicePath << "has Espressif vendor id but unexpected product id" << productId;
    return true;
  }

  qDebug() << "Device" << devicePath << "has unsupported vendor id" << vendorId;
  return false;
#elif defined(Q_OS_WIN)
  // On Windows, try to open and handshake with the device
  // This is simpler than querying hardware IDs from the registry
  deskflow::bridge::CdcTransport transport(devicePath);
  if (!transport.open()) {
    return false;
  }

  bool isSupported = transport.hasDeviceConfig();
  transport.close();
  return isSupported;
#else
  Q_UNUSED(devicePath);
  return false;
#endif
}

bool UsbDeviceHelper::verifyBridgeHandshake(
    const QString &devicePath, deskflow::bridge::FirmwareConfig *configOut, int timeoutMs
)
{
#if defined(Q_OS_LINUX) || defined(Q_OS_WIN)
  Q_UNUSED(timeoutMs);

  deskflow::bridge::CdcTransport transport(devicePath);
  if (!transport.open()) {
    qWarning() << "Bridge handshake failed for" << devicePath << ":" << QString::fromStdString(transport.lastError());
    return false;
  }

  if (transport.hasDeviceConfig()) {
    const auto &cfg = transport.deviceConfig();
    if (configOut != nullptr) {
      *configOut = cfg;
    }

    std::string deviceName;
    if (transport.fetchDeviceName(deviceName)) {
      if (configOut != nullptr) {
        configOut->deviceName = deviceName;
      }
    } else {
      qWarning() << "Unable to fetch device name:" << QString::fromStdString(transport.lastError());
    }

    const QString nameForLog = QString::fromStdString(cfg.deviceName);

    qInfo() << "Bridge handshake successful with" << devicePath << "proto:" << cfg.protocolVersion
            << "activation_state:" << cfg.activationStateString() << "(" << static_cast<unsigned>(cfg.activationState)
            << ")"
            << "fw_bcd:" << cfg.firmwareVersionBcd << "hw_bcd:" << cfg.hardwareVersionBcd << "name:" << nameForLog;
  } else {
    qInfo() << "Bridge handshake successful with" << devicePath << "(no config payload)";
  }

  transport.close();
  return true;
#else
  Q_UNUSED(devicePath)
  Q_UNUSED(timeoutMs)
  Q_UNUSED(configOut)
  return false;
#endif
}

} // namespace deskflow::gui
