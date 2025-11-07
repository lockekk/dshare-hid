/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>
#include <QMap>

namespace deskflow::gui {

/**
 * @brief Helper functions for USB device operations
 */
class UsbDeviceHelper
{
public:
  /**
   * @brief Read serial number from USB CDC device
   * @param devicePath USB CDC device path (e.g., "/dev/ttyACM0")
   * @return Serial number string, or empty if not found
   *
   * On Linux, reads from sysfs: /sys/class/tty/ttyACM0/../../../../serial
   */
  static QString readSerialNumber(const QString &devicePath);

  /**
   * @brief Get all currently connected USB CDC devices with their serial numbers
   * @return Map of device path -> serial number
   *
   * On Linux, scans /dev/ttyACM* devices and reads their serial numbers
   */
  static QMap<QString, QString> getConnectedDevices();

  /**
   * @brief Check if the device path belongs to a supported bridge firmware device
   */
  static bool isSupportedBridgeDevice(const QString &devicePath);

  /**
   * @brief Perform a USB HELLO/ACK handshake with an ESP32 bridge to verify firmware presence.
   * @return true if the device responded with a valid ACK before the timeout expires.
   */
  static bool verifyBridgeHandshake(const QString &devicePath, int timeoutMs = 1500);

  inline static const QString kEspressifVendorId = QStringLiteral("303a");
  inline static const QString kEspressifProductId = QStringLiteral("1001");
};

} // namespace deskflow::gui
