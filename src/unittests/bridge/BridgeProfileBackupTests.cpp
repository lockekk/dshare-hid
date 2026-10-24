/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#include "BridgeProfileBackupTests.h"

#include "gui/core/BridgeDeviceService.h"
#include "gui/core/BridgeProfileBackup.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cstdio>
#include <cstring>

using namespace deskflow::bridge;
using namespace deskflow::gui;

namespace {

// the example in docs/dev/profile_backup_restore.md, as usb_cdc_test.py writes it
const QByteArray kScriptBackup = R"({
  "active_profile": 1,
  "device_name": "Desk Bridge",
  "profiles": [
    {
      "index": 0,
      "hostname": "Pixel",
      "hid_mode": 0,
      "width": 1080,
      "height": 2424,
      "rotation": 0,
      "invert": 0,
      "speed": 30,
      "y_scroll_scale": 10
    },
    {
      "index": 1,
      "hostname": "Ipad air",
      "hid_mode": 1,
      "width": 1080,
      "height": 2424,
      "rotation": 0,
      "invert": 0,
      "speed": 30,
      "y_scroll_scale": 10
    }
  ]
}
)";

DeviceProfile makeProfile(int index, const char *hostname, uint16_t width, uint16_t height)
{
  DeviceProfile profile;
  std::memset(&profile, 0, sizeof(profile));
  std::strncpy(profile.hostname, hostname, sizeof(profile.hostname) - 1);
  profile.slot = static_cast<uint8_t>(index);
  profile.hidMode = 1;
  profile.screenWidth = width;
  profile.screenHeight = height;
  profile.rotation = 1;
  profile.invert = 1;
  profile.speed = 30;
  profile.yScrollScale = 25;
  return profile;
}

// what the firmware reports for a profile that was never set
DeviceProfile unsetProfile(int index)
{
  DeviceProfile profile;
  std::memset(&profile, 0, sizeof(profile));
  std::snprintf(profile.hostname, sizeof(profile.hostname), "Slot %d", index);
  profile.slot = static_cast<uint8_t>(index);
  profile.screenWidth = 1080;
  profile.screenHeight = 2424;
  profile.speed = 30;
  return profile;
}

bool sameProfile(const DeviceProfile &a, const DeviceProfile &b)
{
  return std::memcmp(&a, &b, sizeof(DeviceProfile)) == 0;
}

QJsonObject validProfile(int index)
{
  return QJsonObject{
      {"index", index}, {"hostname", "Tablet"}, {"hid_mode", 0}, {"width", 1200},        {"height", 1920},
      {"rotation", 0},  {"invert", 0},          {"speed", 30},   {"y_scroll_scale", 10},
  };
}

QByteArray backupOf(const QJsonArray &profiles, const QJsonValue &activeProfile = 2)
{
  QJsonObject root{{"profiles", profiles}};
  root.insert("active_profile", activeProfile); // Undefined leaves it out
  return QJsonDocument(root).toJson();
}

// a backup of profile 2, with one of its values replaced (or removed, by Undefined)
QByteArray withProfileValue(const QString &key, const QJsonValue &value)
{
  QJsonObject profile = validProfile(2);
  profile.insert(key, value);
  return backupOf({profile});
}

QByteArray withDeviceName(const QJsonValue &name)
{
  QJsonObject root = QJsonDocument::fromJson(backupOf({validProfile(2)})).object();
  root.insert("device_name", name);
  return QJsonDocument(root).toJson();
}

} // namespace

void BridgeProfileBackupTests::readsScriptBackup()
{
  BridgeProfileBackup backup;
  QStringList problems;
  QVERIFY2(BridgeProfileBackup::fromJson(kScriptBackup, backup, problems), qPrintable(problems.join('\n')));
  QVERIFY(problems.isEmpty());

  QCOMPARE(backup.activeProfile, 1);
  QCOMPARE(backup.deviceName, QStringLiteral("Desk Bridge"));
  QCOMPARE(backup.profiles.keys(), (QList<int>{0, 1}));

  const DeviceProfile &ipad = backup.profiles.value(1);
  QCOMPARE(BridgeDeviceService::profileHostname(ipad), QStringLiteral("Ipad air"));
  QCOMPARE(ipad.slot, uint8_t(1)); // the slot is the index
  QCOMPARE(ipad.hidMode, uint8_t(1));
  QCOMPARE(ipad.screenWidth, uint16_t(1080));
  QCOMPARE(ipad.screenHeight, uint16_t(2424));
  QCOMPARE(ipad.rotation, uint8_t(0));
  QCOMPARE(ipad.invert, uint8_t(0));
  QCOMPARE(ipad.speed, uint8_t(30));
  QCOMPARE(ipad.yScrollScale, uint8_t(10));

  // nothing of the hostname's field past its NUL, nor of the reserved bytes, is left unset
  const QByteArray hostname(ipad.hostname, sizeof(ipad.hostname));
  QCOMPARE(hostname.mid(8), QByteArray(sizeof(ipad.hostname) - 8, '\0'));
  QCOMPARE(
      QByteArray(reinterpret_cast<const char *>(ipad.reserved), sizeof(ipad.reserved)),
      QByteArray(sizeof(ipad.reserved), '\0')
  );
}

void BridgeProfileBackupTests::deviceNameIsOptional()
{
  BridgeProfileBackup backup;
  QStringList problems;
  QVERIFY2(
      BridgeProfileBackup::fromJson(backupOf({validProfile(2)}), backup, problems), qPrintable(problems.join('\n'))
  );
  QVERIFY(backup.deviceName.isEmpty());
  QCOMPARE(backup.profiles.keys(), QList<int>{2});
}

void BridgeProfileBackupTests::acceptsMatchingSlotAndUnknownKeys()
{
  QJsonObject profile = validProfile(2);
  profile.insert("slot", 2);
  profile.insert("comment", "from a later version");
  BridgeProfileBackup backup;
  QStringList problems;
  QVERIFY2(BridgeProfileBackup::fromJson(backupOf({profile}), backup, problems), qPrintable(problems.join('\n')));
  QCOMPARE(backup.profiles.value(2).slot, uint8_t(2));
}

void BridgeProfileBackupTests::roundTrip()
{
  BridgeProfileSet device;
  device.activeIndex = 3;
  device.profiles = {
      makeProfile(0, "Pixel 9", 1080, 2424),
      unsetProfile(1),
      DeviceProfile{}, // an older firmware that couldn't read the profile
      makeProfile(3, "Work Laptop", 2560, 1600),
  };

  const BridgeProfileBackup backup = BridgeProfileBackup::fromDevice(device, QStringLiteral("Desk Bridge"));
  QCOMPARE(backup.profiles.keys(), (QList<int>{0, 3}));

  BridgeProfileBackup read;
  QStringList problems;
  QVERIFY2(BridgeProfileBackup::fromJson(backup.toJson(), read, problems), qPrintable(problems.join('\n')));
  QCOMPARE(read.activeProfile, 3);
  QCOMPARE(read.deviceName, QStringLiteral("Desk Bridge"));
  QCOMPARE(read.profiles.keys(), (QList<int>{0, 3}));
  QVERIFY(sameProfile(read.profiles.value(0), device.profiles.at(0)));
  QVERIFY(sameProfile(read.profiles.value(3), device.profiles.at(3)));

  // the keys usb_cdc_test.py reads, and no slot
  const QJsonObject entry =
      QJsonDocument::fromJson(backup.toJson()).object().value("profiles").toArray().at(0).toObject();
  QCOMPARE(
      entry.keys(),
      (QStringList{"height", "hid_mode", "hostname", "index", "invert", "rotation", "speed", "width", "y_scroll_scale"})
  );
}

void BridgeProfileBackupTests::skipsInvalidDeviceName()
{
  BridgeProfileSet device;
  device.activeIndex = 0;
  device.profiles = {makeProfile(0, "Pixel", 1080, 2424)};

  const BridgeProfileBackup backup = BridgeProfileBackup::fromDevice(device, QStringLiteral("Deskflow HID(trial)"));
  QVERIFY(backup.deviceName.isEmpty());
  QVERIFY(!QJsonDocument::fromJson(backup.toJson()).object().contains("device_name"));
}

void BridgeProfileBackupTests::rejectsInvalidFile_data()
{
  QTest::addColumn<QByteArray>("json");
  QTest::addColumn<QString>("problem");

  // profile values
  QTest::newRow("hid_mode 2") << withProfileValue("hid_mode", 2) << "\"hid_mode\"";
  QTest::newRow("hid_mode as text") << withProfileValue("hid_mode", "0") << "\"hid_mode\"";
  QTest::newRow("width too small") << withProfileValue("width", 99) << "\"width\"";
  QTest::newRow("width too large") << withProfileValue("width", 10001) << "\"width\"";
  QTest::newRow("height missing") << withProfileValue("height", QJsonValue::Undefined) << "\"height\" is missing";
  QTest::newRow("rotation 2") << withProfileValue("rotation", 2) << "\"rotation\"";
  QTest::newRow("rotation 0.5") << withProfileValue("rotation", 0.5) << "\"rotation\"";
  QTest::newRow("invert -1") << withProfileValue("invert", -1) << "\"invert\"";
  QTest::newRow("invert true") << withProfileValue("invert", true) << "\"invert\"";
  QTest::newRow("speed 256") << withProfileValue("speed", 256) << "\"speed\"";
  QTest::newRow("y_scroll_scale 101") << withProfileValue("y_scroll_scale", 101) << "\"y_scroll_scale\"";
  QTest::newRow("hostname empty") << withProfileValue("hostname", "") << "hostname";
  QTest::newRow("hostname spaces") << withProfileValue("hostname", "   ") << "hostname";
  QTest::newRow("hostname 32 chars") << withProfileValue("hostname", QString(32, 'a')) << "hostname";
  QTest::newRow("hostname not ASCII") << withProfileValue("hostname", QStringLiteral("José")) << "hostname";
  QTest::newRow("hostname slash") << withProfileValue("hostname", "a/b") << "hostname";
  QTest::newRow("hostname number") << withProfileValue("hostname", 5) << "hostname";
  QTest::newRow("hostname missing") << withProfileValue("hostname", QJsonValue::Undefined) << "\"hostname\" is missing";
  QTest::newRow("slot not index") << withProfileValue("slot", 3) << "\"slot\"";
  QTest::newRow("slot as text") << withProfileValue("slot", "2") << "\"slot\"";
  QTest::newRow("index -1") << withProfileValue("index", -1) << "\"index\"";
  QTest::newRow("index 15") << withProfileValue("index", 15) << "\"index\"";
  QTest::newRow("index as text") << withProfileValue("index", "2") << "\"index\"";
  QTest::newRow("index missing") << withProfileValue("index", QJsonValue::Undefined) << "\"index\" is missing";

  // the file as a whole
  QTest::newRow("not JSON") << QByteArray("{\"profiles\": [") << "valid JSON";
  QTest::newRow("empty file") << QByteArray() << "valid JSON";
  QTest::newRow("a list") << QByteArray("[]") << "isn't a profile backup";
  QTest::newRow("no profiles key") << QByteArray("{\"active_profile\": 0}") << "isn't a profile backup";
  QTest::newRow("profiles not a list") << QByteArray("{\"active_profile\": 0, \"profiles\": {}}") << "must be a list";
  QTest::newRow("profiles empty") << backupOf({}, 0) << "no profiles";
  QTest::newRow("entry not a profile") << backupOf({1}) << "isn't a profile";
  QTest::newRow("duplicate index") << backupOf({validProfile(2), validProfile(2)}) << "more than once";
  QTest::newRow("active_profile missing")
      << backupOf({validProfile(2)}, QJsonValue::Undefined) << "\"active_profile\" is missing";
  QTest::newRow("active_profile 15") << backupOf({validProfile(2)}, 15) << "\"active_profile\"";
  QTest::newRow("active_profile -1") << backupOf({validProfile(2)}, -1) << "\"active_profile\"";
  QTest::newRow("device_name slash") << withDeviceName("bad/name") << "device name";
  QTest::newRow("device_name 23 chars") << withDeviceName(QString(23, 'a')) << "device name";
  QTest::newRow("device_name spaces") << withDeviceName("   ") << "device name";
  QTest::newRow("device_name number") << withDeviceName(7) << "device name";
  QTest::newRow("too large") << backupOf({validProfile(2)}) + QByteArray(BridgeProfileBackup::kMaxFileSize, ' ')
                             << "too large";
}

void BridgeProfileBackupTests::rejectsInvalidFile()
{
  QFETCH(QByteArray, json);
  QFETCH(QString, problem);

  BridgeProfileBackup backup;
  QStringList problems;
  QVERIFY(!BridgeProfileBackup::fromJson(json, backup, problems));
  QVERIFY2(problems.join('\n').contains(problem), qPrintable(problems.join('\n')));

  // nothing of a file with a problem is kept
  QVERIFY(backup.profiles.isEmpty());
  QVERIFY(backup.deviceName.isEmpty());
}

void BridgeProfileBackupTests::reportsEveryProblem()
{
  QJsonObject bad = validProfile(4);
  bad.insert("width", 0);
  bad.insert("hid_mode", 9);
  BridgeProfileBackup backup;
  QStringList problems;
  QVERIFY(!BridgeProfileBackup::fromJson(backupOf({validProfile(2), bad}, 20), backup, problems));
  QCOMPARE(problems.size(), qsizetype(3));
  QVERIFY(problems.join('\n').contains("Profile 4: \"width\""));
  QVERIFY(problems.join('\n').contains("Profile 4: \"hid_mode\""));
  QVERIFY(problems.join('\n').contains("\"active_profile\""));
}

void BridgeProfileBackupTests::fitsDevice()
{
  BridgeProfileBackup backup;
  QStringList problems;
  QVERIFY(BridgeProfileBackup::fromJson(kScriptBackup, backup, problems));

  QVERIFY(backup.fitsDevice(2, problems)); // profiles 0 and 1, active 1
  QVERIFY(problems.isEmpty());
  QVERIFY(!backup.fitsDevice(1, problems));
  QCOMPARE(problems.size(), qsizetype(2)); // no profile 1 to write, nor to make active

  QVERIFY(BridgeProfileBackup::fromJson(backupOf({validProfile(0)}, 3), backup, problems));
  QVERIFY(!backup.fitsDevice(2, problems));
  QCOMPARE(problems.size(), qsizetype(1));
  QVERIFY(problems.at(0).contains("3"));
}

void BridgeProfileBackupTests::recognisesUnsetProfiles()
{
  QVERIFY(BridgeProfileBackup::isUnset(4, unsetProfile(4)));

  DeviceProfile zero;
  std::memset(&zero, 0, sizeof(zero));
  QVERIFY(BridgeProfileBackup::isUnset(4, zero));

  // the default of another index is a profile someone set
  QVERIFY(!BridgeProfileBackup::isUnset(3, unsetProfile(4)));

  DeviceProfile changed = unsetProfile(4);
  changed.yScrollScale = 10;
  QVERIFY(!BridgeProfileBackup::isUnset(4, changed));
}

void BridgeProfileBackupTests::restoreChecksBeforeTheDevice()
{
  // made by hand, not read from a file
  BridgeProfileBackup backup;
  backup.activeProfile = 0;
  backup.profiles.insert(0, makeProfile(0, "Pixel", 1080, 2424));
  backup.profiles[0].hidMode = 7;

  QList<int> restored;
  BridgeProfileSet profiles;
  QString error;
  QVERIFY(
      !BridgeDeviceService::restoreProfiles(QStringLiteral("/dev/no-such-bridge"), backup, restored, profiles, error)
  );
  // refused for the value, before opening the device (which would fail differently)
  QVERIFY2(error.contains("\"hid_mode\""), qPrintable(error));
  QVERIFY(restored.isEmpty());
}

QTEST_GUILESS_MAIN(BridgeProfileBackupTests)
