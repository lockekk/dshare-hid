/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#include "BridgeDeviceServiceTests.h"

#include "FakeBridgeDevice.h"

#include <cstring>

using namespace deskflow::bridge;
using namespace deskflow::gui;

namespace {

const QString kDevice = QStringLiteral("fake-device");

bool sameProfile(const DeviceProfile &a, const DeviceProfile &b)
{
  return std::memcmp(&a, &b, sizeof(DeviceProfile)) == 0;
}

BridgeProfileBackup backupOf(int activeProfile, const QList<DeviceProfile> &profiles)
{
  BridgeProfileBackup backup;
  backup.activeProfile = activeProfile;
  for (const auto &profile : profiles) {
    backup.profiles.insert(profile.slot, profile);
  }
  return backup;
}

} // namespace

void BridgeDeviceServiceTests::writeSavesEachProfile()
{
  FakeBridgeDevice device(4);
  const auto pixel = setProfile(0, "Pixel", 1080, 2424);
  const auto laptop = setProfile(2, "Work Laptop", 2560, 1600, 1);

  QList<int> written;
  BridgeProfileSet profiles;
  QString error;
  QVERIFY2(
      BridgeDeviceService::writeProfiles(kDevice, backupOf(0, {pixel, laptop}), written, profiles, error),
      qPrintable(error)
  );

  QCOMPARE(device.received, (QStringList{"profiles", "save 0", "save 2"}));
  QCOMPARE(written, (QList<int>{0, 2}));
  QVERIFY(sameProfile(device.profiles.at(0), pixel));
  QVERIFY(sameProfile(device.profiles.at(2), laptop));
  // the reply is the device's profiles afterwards
  QCOMPARE(profiles.profiles.size(), qsizetype(4));
  QVERIFY(sameProfile(profiles.profiles.at(2), laptop));
}

void BridgeDeviceServiceTests::writeSwitchesOnlyIfNeedBe()
{
  FakeBridgeDevice device(4);
  device.active = 2;
  const auto laptop = setProfile(2, "Work Laptop", 2560, 1600);

  QList<int> written;
  BridgeProfileSet profiles;
  QString error;
  QVERIFY(BridgeDeviceService::writeProfiles(kDevice, backupOf(2, {laptop}), written, profiles, error));
  QCOMPARE(device.received, (QStringList{"profiles", "save 2"}));

  device.received.clear();
  QVERIFY(BridgeDeviceService::writeProfiles(kDevice, backupOf(0, {laptop}), written, profiles, error));
  // the switch comes after the profiles, so the host reconnects to what was written
  QCOMPARE(device.received, (QStringList{"profiles", "save 2", "switch 0"}));
  QCOMPARE(device.active, 0);
  QCOMPARE(profiles.activeIndex, 0);
}

void BridgeDeviceServiceTests::writeChecksBeforeTheDevice()
{
  FakeBridgeDevice device(4);
  auto pixel = setProfile(0, "Pixel", 1080, 2424);
  pixel.hidMode = 7;

  QList<int> written;
  BridgeProfileSet profiles;
  QString error;
  QVERIFY(!BridgeDeviceService::writeProfiles(kDevice, backupOf(0, {pixel}), written, profiles, error));
  QVERIFY2(error.contains("\"hid_mode\""), qPrintable(error));
  QVERIFY(device.received.isEmpty());
  QVERIFY(written.isEmpty());
}

void BridgeDeviceServiceTests::writeCleansProfiles()
{
  FakeBridgeDevice device(4);
  auto pixel = setProfile(0, "Pixel", 1080, 2424);
  pixel.slot = 3;
  std::memset(pixel.reserved, 0xAB, sizeof(pixel.reserved));

  BridgeProfileBackup backup;
  backup.activeProfile = 0;
  backup.profiles.insert(0, pixel);
  QList<int> written;
  BridgeProfileSet profiles;
  QString error;
  QVERIFY2(BridgeDeviceService::writeProfiles(kDevice, backup, written, profiles, error), qPrintable(error));

  // what reached the device: the slot is the index, and nothing past the values is kept
  QVERIFY(sameProfile(device.profiles.at(0), setProfile(0, "Pixel", 1080, 2424)));
}

void BridgeDeviceServiceTests::writeRefusesWhatDoesntFit()
{
  FakeBridgeDevice device(2); // a trial device
  const auto laptop = setProfile(3, "Work Laptop", 2560, 1600);

  QList<int> written;
  BridgeProfileSet profiles;
  QString error;
  QVERIFY(!BridgeDeviceService::writeProfiles(kDevice, backupOf(0, {laptop}), written, profiles, error));
  QVERIFY2(error.contains("no profile 3"), qPrintable(error));
  QCOMPARE(device.received, QStringList{"profiles"});

  QVERIFY(!BridgeDeviceService::writeProfiles(
      kDevice, backupOf(2, {setProfile(0, "Pixel", 1080, 2424)}), written, profiles, error
  ));
  QCOMPARE(device.received, (QStringList{"profiles", "profiles"}));
}

void BridgeDeviceServiceTests::writeCatchesBadReadBack()
{
  FakeBridgeDevice device(4);
  device.storesOtherThanSent = true;

  QList<int> written;
  BridgeProfileSet profiles;
  QString error;
  QVERIFY(!BridgeDeviceService::writeProfiles(
      kDevice, backupOf(0, {setProfile(0, "Pixel", 1080, 2424), setProfile(1, "Ipad air", 1640, 2360)}), written,
      profiles, error
  ));
  QVERIFY2(error.contains("profile 0 didn't read back"), qPrintable(error));
  QCOMPARE(written, QList<int>{0}); // written, though wrongly
  QCOMPARE(device.received, (QStringList{"profiles", "save 0"}));
}

void BridgeDeviceServiceTests::writeStopsAtFailedSave()
{
  FakeBridgeDevice device(4);
  device.failing = QStringLiteral("save 1");

  QList<int> written;
  BridgeProfileSet profiles;
  QString error;
  QVERIFY(!BridgeDeviceService::writeProfiles(
      kDevice,
      backupOf(
          2, {setProfile(0, "Pixel", 1080, 2424), setProfile(1, "Ipad air", 1640, 2360),
              setProfile(2, "Work Laptop", 2560, 1600)}
      ),
      written, profiles, error
  ));
  QVERIFY2(error.contains("failed to write profile 1"), qPrintable(error));
  QCOMPARE(written, QList<int>{0});
  // nothing after the failure, and no switch
  QCOMPARE(device.received, (QStringList{"profiles", "save 0", "save 1"}));
}

void BridgeDeviceServiceTests::writeReportsFailedSwitch()
{
  FakeBridgeDevice device(4);
  std::memset(&device.profiles[3], 0, sizeof(DeviceProfile)); // no hostname to switch to

  QList<int> written;
  BridgeProfileSet profiles;
  QString error;
  QVERIFY(!BridgeDeviceService::writeProfiles(
      kDevice, backupOf(3, {setProfile(0, "Pixel", 1080, 2424)}), written, profiles, error
  ));
  QVERIFY2(error.contains("failed to make profile 3 active"), qPrintable(error));
  QCOMPARE(written, QList<int>{0});
  QCOMPARE(device.active, 0);
}

void BridgeDeviceServiceTests::backupHoldsSetProfiles()
{
  FakeBridgeDevice device(4);
  device.profiles[1] = setProfile(1, "Ipad air", 1640, 2360);
  device.unreadable = {2};
  device.active = 1;
  device.name = QStringLiteral("Desk Bridge");

  BridgeProfileBackup backup;
  QString error;
  QVERIFY2(BridgeDeviceService::backupProfiles(kDevice, backup, error), qPrintable(error));
  QCOMPARE(device.received, (QStringList{"profiles", "getname"}));
  QCOMPARE(backup.profiles.keys(), QList<int>{1});
  QVERIFY(sameProfile(backup.profiles.value(1), device.profiles.at(1)));
  QCOMPARE(backup.activeProfile, 1);
  QCOMPARE(backup.deviceName, QStringLiteral("Desk Bridge"));
}

void BridgeDeviceServiceTests::backupWithoutName()
{
  FakeBridgeDevice device(4);
  device.profiles[0] = setProfile(0, "Pixel", 1080, 2424);
  device.failing = QStringLiteral("getname");

  // the name is optional in a backup
  BridgeProfileBackup backup;
  QString error;
  QVERIFY2(BridgeDeviceService::backupProfiles(kDevice, backup, error), qPrintable(error));
  QCOMPARE(backup.profiles.keys(), QList<int>{0});
  QVERIFY(backup.deviceName.isEmpty());
}

void BridgeDeviceServiceTests::backupFailsWithoutProfiles()
{
  FakeBridgeDevice device(4);
  device.failing = QStringLiteral("profiles");

  BridgeProfileBackup backup;
  QString error;
  QVERIFY(!BridgeDeviceService::backupProfiles(kDevice, backup, error));
  QVERIFY(!error.isEmpty());
}

QTEST_GUILESS_MAIN(BridgeDeviceServiceTests)
