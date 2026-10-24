#pragma once

#include "platform/bridge/CdcTransport.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

namespace deskflow {
namespace gui {

class BridgeProfileBackup;

/**
 * @brief One of the device's profiles, as the main window lists it
 */
struct BridgeProfileInfo
{
  int index = 0;
  QString hostname;
  bool portrait = false;

  /// the hostname, or "Slot N" for an unused slot (as the Configure dialog's profile tabs)
  QString label() const
  {
    return hostname.isEmpty() ? QStringLiteral("Slot %1").arg(index) : hostname;
  }
};

/**
 * @brief All of the device's profiles, by index, and which one is active
 */
struct BridgeProfileSet
{
  int activeIndex = -1;
  QList<deskflow::bridge::DeviceProfile> profiles;
};

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
   * @brief Carry out a device request (see platform/bridge/BridgeRequests.h) through the
   * device's owner: its started bridge client, or the GUI itself when there is none.
   * @param fields The reply's fields, on success.
   * @param error Why it failed, otherwise.
   */
  static bool request(
      const QString &devicePath, const QString &command, const QStringList &arguments, QStringList &fields,
      QString &error
  );

  using RequestHandler = std::function<bool(
      const QString &devicePath, const QString &command, const QStringList &arguments, QStringList &fields,
      QString &error
  )>;

  /**
   * @brief For tests: carry out every request (see request()) with this instead of the device's
   * owner, until it is set back to none
   */
  static void setRequestHandler(RequestHandler handler);

  /**
   * @brief Profile requests, through the device's owner (see request()). Each one answers
   * with the device's profiles as they are afterwards.
   */
  static bool readProfiles(const QString &devicePath, BridgeProfileSet &profiles, QString &error);
  static bool switchProfile(const QString &devicePath, int profileIndex, BridgeProfileSet &profiles, QString &error);
  static bool saveProfile(
      const QString &devicePath, int profileIndex, const deskflow::bridge::DeviceProfile &profile,
      BridgeProfileSet &profiles, QString &error
  );
  static bool eraseProfile(const QString &devicePath, int profileIndex, BridgeProfileSet &profiles, QString &error);

  /**
   * @brief Read the device's profiles that were set, and its name, for a backup file
   */
  static bool backupProfiles(const QString &devicePath, BridgeProfileBackup &backup, QString &error);

  /**
   * @brief Write profiles to the device, then make the backup's active profile active
   *
   * The Configure dialog saves its changes on OK this way, a restored backup's among them.  The
   * backup is checked again as a backup file would be (BridgeProfileBackup::check()), so
   * nothing is written unless every value is valid and all of it fits the device.  Each
   * profile must read back as it was written, and the switch happens only if need be.  The
   * backup's device name is left to the caller.
   * @param written The indexes of the profiles written, also when it stops part way
   * @param profiles The device's profiles afterwards, on success
   */
  static bool writeProfiles(
      const QString &devicePath, const BridgeProfileBackup &backup, QList<int> &written, BridgeProfileSet &profiles,
      QString &error
  );

  /**
   * @brief Read all of the device's profiles over a transport the GUI has open itself.
   */
  static bool readProfiles(deskflow::bridge::CdcTransport &transport, BridgeProfileSet &profiles, QString &error);

  /**
   * @brief What the main window shows of each profile.
   */
  static QList<BridgeProfileInfo> profileInfos(const BridgeProfileSet &profiles);

  /**
   * @brief The profile's hostname, without the padding of its fixed-size field.
   */
  static QString profileHostname(const deskflow::bridge::DeviceProfile &profile);

  /**
   * @brief Validate if a device name follows the required format.
   */
  static bool isValidDeviceName(const QString &deviceName);

  /**
   * @brief Hand all devices to the firmware tool (true), or take them back (false)
   *
   * The tool is the one exception to the rule that only one owner talks to a device: while
   * it is open, the GUI opens no device and starts no bridge client.
   */
  static void setSuspended(bool suspended);

  /**
   * @brief Whether the GUI may open the device's port itself: no started bridge client owns
   * it (the core holds it exclusively) and the firmware tool isn't open
   */
  static bool mayOpen(const QString &devicePath);

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

private:
  static bool requestProfiles(
      const QString &devicePath, const QString &command, const QStringList &arguments, BridgeProfileSet &profiles,
      QString &error
  );
  static bool decodeProfileSet(const QStringList &fields, BridgeProfileSet &profiles, QString &error);
};

} // namespace gui
} // namespace deskflow
