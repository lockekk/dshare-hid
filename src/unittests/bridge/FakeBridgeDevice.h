/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#pragma once

#include "gui/core/BridgeDeviceService.h"
#include "gui/core/BridgeProfileBackup.h"
#include "platform/bridge/BridgeRequests.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <cstring>

/**
 * @brief A bridge device for tests, answering BridgeDeviceService's requests as the firmware
 * does, while it lives
 *
 * It notes each request ("profiles", "save 1", "switch 2", "erase 0", "getname"), and can be
 * told to fail one, or to store a profile other than the one it is sent.
 */
class FakeBridgeDevice
{
public:
  explicit FakeBridgeDevice(int profileCount)
  {
    for (int i = 0; i < profileCount; ++i) {
      profiles.append(deskflow::gui::BridgeProfileBackup::unsetProfile(i));
    }
    deskflow::gui::BridgeDeviceService::setRequestHandler(
        [this](
            const QString &, const QString &command, const QStringList &arguments, QStringList &fields, QString &error
        ) { return handle(command, arguments, fields, error); }
    );
  }

  ~FakeBridgeDevice()
  {
    deskflow::gui::BridgeDeviceService::setRequestHandler(nullptr);
  }

  FakeBridgeDevice(const FakeBridgeDevice &) = delete;
  FakeBridgeDevice &operator=(const FakeBridgeDevice &) = delete;

  QList<deskflow::bridge::DeviceProfile> profiles;
  int active = 0;
  QString name = QStringLiteral("Deskflow HID");

  /// each request, in turn
  QStringList received;
  /// a request to fail, as noted in received
  QString failing;
  /// store each saved profile one pixel wider than sent
  bool storesOtherThanSent = false;
  /// the profiles erased, whose pairing the firmware removes with them
  QList<int> unpaired;
  /// profiles an older firmware can't read, which BridgeRequests reports as all zero
  QList<int> unreadable;

private:
  bool handle(const QString &command, const QStringList &arguments, QStringList &fields, QString &error)
  {
    namespace requests = deskflow::bridge::requests;

    const QString request = arguments.isEmpty() || command == requests::kSetName
                                ? command
                                : QStringLiteral("%1 %2").arg(command, arguments.at(0));
    received.append(request);
    if (request == failing) {
      error = QStringLiteral("Firmware error code 1");
      return false;
    }

    bool isIndex = false;
    const int index = arguments.value(0).toInt(&isIndex);
    const bool inRange = isIndex && index >= 0 && index < profiles.size();

    if (command == requests::kGetName) {
      fields = {requests::encodeText(name)};
      return true;
    }
    if (command == requests::kSetName) {
      name = requests::decodeText(arguments.value(0));
      fields.clear();
      return true;
    }
    if (command == requests::kSave) {
      deskflow::bridge::DeviceProfile profile;
      if (!inRange || !requests::decodeProfile(arguments.value(1), profile)) {
        error = QStringLiteral("invalid profile");
        return false;
      }
      if (storesOtherThanSent) {
        profile.screenWidth += 1;
      }
      profiles[index] = profile;
    } else if (command == requests::kSwitch) {
      // the firmware won't switch to a profile without a hostname
      if (!inRange || deskflow::gui::BridgeDeviceService::profileHostname(profiles.at(index)).isEmpty()) {
        error = QStringLiteral("Firmware error code 1");
        return false;
      }
      active = index;
    } else if (command == requests::kErase) {
      if (!inRange) {
        error = QStringLiteral("Firmware error code 1");
        return false;
      }
      profiles[index] = deskflow::gui::BridgeProfileBackup::unsetProfile(index);
      unpaired.append(index);
    } else if (command != requests::kProfiles) {
      error = QStringLiteral("unknown command");
      return false;
    }

    // as GET_PROFILE answers: each profile's slot is its index
    fields = {QString::number(active)};
    for (qsizetype i = 0; i < profiles.size(); ++i) {
      auto profile = profiles.at(i);
      profile.slot = static_cast<uint8_t>(i);
      if (unreadable.contains(static_cast<int>(i))) {
        std::memset(&profile, 0, sizeof(profile));
      }
      fields.append(requests::encodeProfile(profile));
    }
    return true;
  }
};

/// a set profile with the values tests look for
inline deskflow::bridge::DeviceProfile
setProfile(int index, const char *hostname, uint16_t width, uint16_t height, uint8_t rotation = 0)
{
  deskflow::bridge::DeviceProfile profile;
  std::memset(&profile, 0, sizeof(profile));
  std::strncpy(profile.hostname, hostname, sizeof(profile.hostname) - 1);
  profile.slot = static_cast<uint8_t>(index);
  profile.screenWidth = width;
  profile.screenHeight = height;
  profile.rotation = rotation;
  profile.speed = 30;
  return profile;
}
