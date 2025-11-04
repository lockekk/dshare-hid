/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "UsbDeviceHelper.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QStringList>

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

} // namespace

QString UsbDeviceHelper::readSerialNumber(const QString &devicePath)
{
#ifdef Q_OS_LINUX
  // Extract tty device name from path (e.g., /dev/ttyACM0 -> ttyACM0)
  QFileInfo deviceInfo(devicePath);
  QString ttyName = deviceInfo.fileName();

  const QString ttyBasePath = QStringLiteral("/sys/class/tty/%1").arg(ttyName);
  QFileInfo ttyLink(ttyBasePath);
  if (!ttyLink.exists()) {
    qWarning() << "TTY entry does not exist for" << devicePath << ":" << ttyBasePath;
    return QString();
  }

  QFileInfo deviceLink(ttyBasePath + QStringLiteral("/device"));
  QStringList attemptedPaths;

  auto tryReadSerial = [&](const QString &candidatePath) -> QString {
    attemptedPaths << candidatePath;
    QFile serialFile(candidatePath);
    if (!serialFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
      return QString();
    }
    QTextStream in(&serialFile);
    QString serialNumber = in.readLine().trimmed();
    serialFile.close();
    return serialNumber;
  };

  // Some kernels expose the serial directly on the tty node (rare, but fast to check).
  const QString directSerial = tryReadSerial(ttyBasePath + QStringLiteral("/serial"));
  if (!directSerial.isEmpty()) {
    qDebug() << "Read serial number for" << devicePath << ":" << directSerial;
    return directSerial;
  }

  // Resolve the tty device symlink and search upwards for a "serial" attribute on the USB node.
  const QString canonicalDevicePath = deviceLink.canonicalFilePath();
  if (!canonicalDevicePath.isEmpty()) {
    const QString serial = readUsbAttribute(canonicalDevicePath, QStringLiteral("serial"));
    if (!serial.isEmpty()) {
      qDebug() << "Read serial number for" << devicePath << ":" << serial;
      return serial;
    }
  }

  qWarning() << "Failed to read serial number for" << devicePath << "after checking"
             << attemptedPaths;
#endif

  return QString();
}

QMap<QString, QString> UsbDeviceHelper::getConnectedDevices()
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

    QString serialNumber = readSerialNumber(devicePath);

    if (!serialNumber.isEmpty()) {
      devices[devicePath] = serialNumber;
    }
  }

  qDebug() << "Found" << devices.size() << "connected USB CDC devices";
#endif

  return devices;
}

bool UsbDeviceHelper::isSupportedBridgeDevice(const QString &devicePath)
{
#ifdef Q_OS_LINUX
  const QString canonicalPath = canonicalUsbDevicePath(devicePath);
  const QString vendorId = readUsbAttribute(canonicalPath, QStringLiteral("idVendor")).toLower();
  if (vendorId.isEmpty())
    return false;

  if (vendorId == kPicoVendorId) {
    return true;
  }

  qDebug() << "Device" << devicePath << "has unsupported vendor id" << vendorId;
  return false;
#else
  Q_UNUSED(devicePath);
  return false;
#endif
}

} // namespace deskflow::gui
