/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#pragma once

#include <QMap>
#include <QString>

#include "platform/bridge/CdcTransport.h"

namespace deskflow::gui {

/**
 * @brief Helper functions for USB device operations
 */
class UsbDeviceHelper
{
public:
  /**
   * @brief Read serial number from USB CDC device via CDC command
   * @param devicePath USB CDC device path (e.g., "/dev/ttyACM0")
   * @return Serial number string, or empty if not found
   *
   * Reads serial number by sending CDC command to firmware.
   * Only reads when device is not opened by bridge client to avoid conflicts.
   * Works on Linux and Windows platforms.
   */
  static QString readSerialNumber(const QString &devicePath);

  /**
   * @brief Get all currently connected USB CDC devices with their serial numbers
   * @param queryDevice If true, attempt to read serial number via CDC (can be slow/timeout).
   * @return Map of device path -> serial number
   *
   * Scans platform-specific device paths.
   * Works on Linux and Windows platforms.
   */
  static QMap<QString, QString> getConnectedDevices(bool queryDevice = true);

  /**
   * @brief Check if the device path belongs to a supported bridge firmware device
   */
  static bool isSupportedBridgeDevice(const QString &devicePath);

  /**
   * @brief Perform a USB HELLO/ACK handshake with an ESP32 bridge to verify firmware presence.
   * @return true if the device responded with a valid ACK before the timeout expires.
   */
  static bool verifyBridgeHandshake(
      const QString &devicePath, deskflow::bridge::FirmwareConfig *configOut = nullptr, int timeoutMs = 1500
  );

  inline static const QString kEspressifVendorId = QStringLiteral("303a");
  inline static const QString kEspressifProductId = QStringLiteral("1001");
};

} // namespace deskflow::gui
