/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#pragma once

#include "platform/bridge/CdcTransport.h"

#include <QCoreApplication>
#include <QString>

namespace deskflow::gui {

/**
 * @brief The notice to pair the hosts again once a device runs firmware 1.2.1, which resets
 * every Bluetooth pairing once on its first boot (see docs/dev/profile_backup_restore.md)
 *
 * The app keeps the last firmware version it saw on each device, by serial number.  The notice
 * is due when that goes from below 1.2.1 to 1.2.1 or above.
 */
class BridgeFirmwareNotice
{
  Q_DECLARE_TR_FUNCTIONS(BridgeFirmwareNotice)

public:
  /// 1.2.1 as the HELLO ACK reports it: major × 100 + minor × 10 + patch
  static constexpr int kPairingResetVersion = 121;

  /**
   * @brief Note the firmware a device runs, from its handshake
   * @param serialNumber The device's serial number (CdcTransport::fetchSerialNumber())
   * @return The notice, if it is due.  Nothing for a device the app sees for the first time
   * (it can't know whether it was ever paired), a version of 0 (factory mode) or no serial number.
   */
  static QString noteHandshake(const QString &serialNumber, const deskflow::bridge::FirmwareConfig &config);

  /// the name hosts see the device by, as the firmware advertises it
  static QString advertisedName(deskflow::bridge::ActivationState state, const QString &deviceName);

  /// where the versions are kept: next to the app's settings
  static QString versionsFile();
};

} // namespace deskflow::gui
