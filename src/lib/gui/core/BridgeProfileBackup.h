/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#pragma once

#include "platform/bridge/CdcTransport.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QMap>
#include <QString>
#include <QStringList>

namespace deskflow::gui {

struct BridgeProfileSet;

/**
 * @brief A bridge device's profiles as a backup file holds them
 *
 * The file is the JSON that `usb_cdc_test.py backup-profiles` (esp32-hid repo) writes, plus
 * an optional "device_name" (see docs/dev/profile_backup_restore.md).  The firmware stores
 * any profile it is sent as it is, so fromJson() checks every value against what the firmware
 * and the Configure dialog accept: a profile read from a file is one the dialog could have
 * saved.
 */
class BridgeProfileBackup
{
  Q_DECLARE_TR_FUNCTIONS(BridgeProfileBackup)

public:
  /// larger files aren't read: a backup of the most profiles a device has is about 3 KB
  static constexpr qint64 kMaxFileSize = 64 * 1024;

  int activeProfile = 0;
  /// empty when the file has none
  QString deviceName;
  /// the profiles that were set, by index; each one's slot is its index
  QMap<int, deskflow::bridge::DeviceProfile> profiles;

  /**
   * @brief The device's profiles that were set, and its name if it is a valid one
   */
  static BridgeProfileBackup fromDevice(const BridgeProfileSet &profiles, const QString &deviceName);

  QByteArray toJson() const;

  /**
   * @brief Read a backup file, checking every value
   * @param problems What is wrong with the file, one line each, when it isn't valid
   */
  static bool fromJson(const QByteArray &json, BridgeProfileBackup &backup, QStringList &problems);

  /**
   * @brief Whether every profile, and the active one, is within a device's profiles
   * @param problems Which aren't, otherwise
   */
  bool fitsDevice(int profileCount, QStringList &problems) const;

  /**
   * @brief Whether the device reports the profile as never set: what the firmware returns for
   * an unset profile, or the all-zero profile shown when an older firmware can't read one
   */
  static bool isUnset(int index, const deskflow::bridge::DeviceProfile &profile);
};

} // namespace deskflow::gui
