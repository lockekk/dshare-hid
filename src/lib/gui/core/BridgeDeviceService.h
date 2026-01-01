#pragma once

#include <QObject>
#include <QString>

namespace deskflow {
namespace gui {

/**
 * @brief Service class to handle direct interactions with Bridge HID devices.
 *
 * This class encapsulates hardware-specific logic such as synchronizing
 * configuration from the device, updating firmware settings, and validating
 * hardware-level constraints.
 */
class BridgeDeviceService : public QObject
{
  Q_OBJECT
public:
  explicit BridgeDeviceService(QObject *parent = nullptr);

  /**
   * @brief Synchronize device configuration from hardware to the local configuration file.
   * @param devicePath Path to the USB CDC device.
   * @param configPath Path to the local .conf file.
   * @param outIsBleConnected Optional output for BLE connection status.
   * @return True if sync was successful.
   */
  bool syncDeviceConfig(const QString &devicePath, const QString &configPath, bool *outIsBleConnected = nullptr);

  /**
   * @brief Update the device name stored in the firmware.
   * @param devicePath Path to the USB CDC device.
   * @param deviceName New name to set.
   * @return True if successful.
   */
  bool applyFirmwareDeviceName(const QString &devicePath, const QString &deviceName);

  /**
   * @brief Fetch the current device name from the firmware.
   * @param devicePath Path to the USB CDC device.
   * @param outName Retrieved name.
   * @return True if successful.
   */
  bool fetchFirmwareDeviceName(const QString &devicePath, QString &outName);

  /**
   * @brief Validate if a device name follows the required format.
   */
  static bool isValidDeviceName(const QString &deviceName);

Q_SIGNALS:
  /**
   * @brief Emitted when a device configuration has been successfully synchronized.
   * @param configPath The path to the updated configuration file.
   * @param activeProfile The active profile index reported by the device.
   */
  void configSynced(const QString &configPath, int activeProfile);

  /**
   * @brief Emitted when a device name has been updated in firmware.
   */
  void deviceNameUpdated(const QString &devicePath, const QString &newName);

  /**
   * @brief Emitted when an error occurs during hardware interaction.
   */
  void errorOccurred(const QString &devicePath, const QString &error);
};

} // namespace gui
} // namespace deskflow
