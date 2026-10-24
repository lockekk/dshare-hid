/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#include "BridgeProfileBackup.h"

#include "gui/core/BridgeDeviceService.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>

#include <cmath>
#include <cstdio>
#include <cstring>

namespace deskflow::gui {

using deskflow::bridge::DeviceProfile;

namespace {

const QString kActiveProfile = QStringLiteral("active_profile");
const QString kDeviceName = QStringLiteral("device_name");
const QString kProfiles = QStringLiteral("profiles");
const QString kIndex = QStringLiteral("index");
const QString kSlot = QStringLiteral("slot");
const QString kHostname = QStringLiteral("hostname");

// the HELLO ACK reports the device's profile count in 4 bits
constexpr int kMaxProfiles = 15;

// a profile's numbers, each with the values the firmware and the Configure dialog accept
struct Field
{
  const char *key;
  int min;
  int max;
  int (*get)(const DeviceProfile &);
  void (*set)(DeviceProfile &, int);
};

const Field kFields[] = {
    // 0: Keyboard + Mouse, 1: Mouse Only
    {"hid_mode", 0, 1, [](const DeviceProfile &p) -> int { return p.hidMode; },
     [](DeviceProfile &p, int v) { p.hidMode = static_cast<uint8_t>(v); }},
    // the dialog's resolution boxes
    {"width", 100, 10000, [](const DeviceProfile &p) -> int { return p.screenWidth; },
     [](DeviceProfile &p, int v) { p.screenWidth = static_cast<uint16_t>(v); }},
    {"height", 100, 10000, [](const DeviceProfile &p) -> int { return p.screenHeight; },
     [](DeviceProfile &p, int v) { p.screenHeight = static_cast<uint16_t>(v); }},
    // 0: portrait, 1: landscape
    {"rotation", 0, 1, [](const DeviceProfile &p) -> int { return p.rotation; },
     [](DeviceProfile &p, int v) { p.rotation = static_cast<uint8_t>(v); }},
    {"invert", 0, 1, [](const DeviceProfile &p) -> int { return p.invert; },
     [](DeviceProfile &p, int v) { p.invert = static_cast<uint8_t>(v); }},
    // the legacy scroll speed, which the core reads 0 of as 120
    {"speed", 0, 255, [](const DeviceProfile &p) -> int { return p.speed; },
     [](DeviceProfile &p, int v) { p.speed = static_cast<uint8_t>(v); }},
    // tenths, 0.1-10.0 as the dialog's scroll scale; 0 is the firmware's default, 1.0
    {"y_scroll_scale", 0, 100, [](const DeviceProfile &p) -> int { return p.yScrollScale; },
     [](DeviceProfile &p, int v) { p.yScrollScale = static_cast<uint8_t>(v); }},
};

QString prefixed(const QString &where, const QString &problem)
{
  return where.isEmpty() ? problem : BridgeProfileBackup::tr("%1: %2").arg(where, problem);
}

// a whole number from min to max, or the problem with it
bool readInteger(
    const QJsonObject &object, const QString &key, int min, int max, const QString &where, int &value,
    QStringList &problems
)
{
  const QJsonValue json = object.value(key);
  if (json.isUndefined()) {
    problems.append(prefixed(where, BridgeProfileBackup::tr("\"%1\" is missing.").arg(key)));
    return false;
  }
  const double number = json.toDouble();
  if (!json.isDouble() || number != std::floor(number) || number < min || number > max) {
    problems.append(prefixed(
        where, BridgeProfileBackup::tr("\"%1\" must be a whole number from %2 to %3.").arg(key).arg(min).arg(max)
    ));
    return false;
  }
  value = static_cast<int>(number);
  return true;
}

// the rule of the Configure dialog's hostname field, which also needs more than spaces
bool isValidHostname(const QString &hostname)
{
  static const QRegularExpression rule(QStringLiteral("^[A-Za-z0-9 _\\-\\.]{1,31}$"));
  return rule.match(hostname).hasMatch() && !hostname.trimmed().isEmpty();
}

// the dialog won't accept a name of only spaces either
bool isValidName(const QString &deviceName)
{
  return BridgeDeviceService::isValidDeviceName(deviceName) && !deviceName.trimmed().isEmpty();
}

void readProfile(
    const QJsonValue &entry, qsizetype position, QSet<int> &indexes, QMap<int, DeviceProfile> &profiles,
    QStringList &problems
)
{
  QString where = BridgeProfileBackup::tr("Entry %1 of \"profiles\"").arg(position + 1);
  if (!entry.isObject()) {
    problems.append(BridgeProfileBackup::tr("%1 isn't a profile.").arg(where));
    return;
  }
  const QJsonObject object = entry.toObject();

  int index = 0;
  if (!readInteger(object, kIndex, 0, kMaxProfiles - 1, where, index, problems)) {
    return;
  }
  where = BridgeProfileBackup::tr("Profile %1").arg(index);
  if (indexes.contains(index)) {
    problems.append(BridgeProfileBackup::tr("Profile %1 is in the backup more than once.").arg(index));
    return;
  }
  indexes.insert(index);

  DeviceProfile profile;
  std::memset(&profile, 0, sizeof(profile));
  // the firmware picks the profile's Bluetooth address and pairing by its slot
  profile.slot = static_cast<uint8_t>(index);
  bool valid = true;

  // a backup needn't say, as the slot is always the index
  if (object.contains(kSlot) && object.value(kSlot).toDouble(-1) != index) {
    problems.append(
        prefixed(where, BridgeProfileBackup::tr("\"%1\" must be %2, the profile's index.").arg(kSlot).arg(index))
    );
    valid = false;
  }

  const QJsonValue hostname = object.value(kHostname);
  if (hostname.isUndefined()) {
    problems.append(prefixed(where, BridgeProfileBackup::tr("\"%1\" is missing.").arg(kHostname)));
    valid = false;
  } else if (!hostname.isString() || !isValidHostname(hostname.toString())) {
    problems.append(prefixed(
        where, BridgeProfileBackup::tr(
                   "the hostname must be 1 to 31 characters: A-Z, 0-9, spaces and .-_ "
                   "(not only spaces)."
               )
    ));
    valid = false;
  } else {
    const QByteArray bytes = hostname.toString().toLatin1(); // ASCII, and at most 31 bytes
    std::memcpy(profile.hostname, bytes.constData(), bytes.size());
  }

  for (const auto &field : kFields) {
    int value = 0;
    if (readInteger(object, QString::fromLatin1(field.key), field.min, field.max, where, value, problems)) {
      field.set(profile, value);
    } else {
      valid = false;
    }
  }

  if (valid) {
    profiles.insert(index, profile);
  }
}

} // namespace

BridgeProfileBackup BridgeProfileBackup::fromDevice(const BridgeProfileSet &profiles, const QString &deviceName)
{
  BridgeProfileBackup backup;
  backup.activeProfile = profiles.activeIndex;
  if (isValidName(deviceName)) {
    backup.deviceName = deviceName;
  }
  for (qsizetype i = 0; i < profiles.profiles.size(); ++i) {
    if (!isUnset(static_cast<int>(i), profiles.profiles.at(i))) {
      backup.profiles.insert(static_cast<int>(i), profiles.profiles.at(i));
    }
  }
  return backup;
}

QByteArray BridgeProfileBackup::toJson() const
{
  QJsonArray entries;
  for (auto it = profiles.cbegin(); it != profiles.cend(); ++it) {
    QJsonObject entry{{kIndex, it.key()}, {kHostname, BridgeDeviceService::profileHostname(it.value())}};
    for (const auto &field : kFields) {
      entry.insert(QString::fromLatin1(field.key), field.get(it.value()));
    }
    entries.append(entry);
  }

  QJsonObject root{{kActiveProfile, activeProfile}, {kProfiles, entries}};
  if (!deviceName.isEmpty()) {
    root.insert(kDeviceName, deviceName);
  }
  return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool BridgeProfileBackup::fromJson(const QByteArray &json, BridgeProfileBackup &backup, QStringList &problems)
{
  backup = BridgeProfileBackup();
  problems.clear();

  if (json.size() > kMaxFileSize) {
    problems.append(tr("The file is too large to be a profile backup."));
    return false;
  }

  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
  if (parseError.error != QJsonParseError::NoError) {
    problems.append(
        tr("The file isn't valid JSON: %1 (at character %2).").arg(parseError.errorString()).arg(parseError.offset)
    );
    return false;
  }
  const QJsonObject root = document.object();
  if (!document.isObject() || !root.contains(kProfiles)) {
    problems.append(tr("The file isn't a profile backup: it has no \"%1\".").arg(kProfiles));
    return false;
  }

  readInteger(root, kActiveProfile, 0, kMaxProfiles - 1, QString(), backup.activeProfile, problems);

  if (root.contains(kDeviceName)) {
    const QJsonValue name = root.value(kDeviceName);
    if (name.isString() && isValidName(name.toString())) {
      backup.deviceName = name.toString();
    } else {
      problems.append(tr("The device name must be 1 to 22 characters: A-Z, 0-9, spaces and .-_ (not only spaces)."));
    }
  }

  const QJsonValue entries = root.value(kProfiles);
  if (!entries.isArray()) {
    problems.append(tr("\"%1\" must be a list.").arg(kProfiles));
  } else if (entries.toArray().isEmpty()) {
    problems.append(tr("The backup has no profiles."));
  } else {
    QSet<int> indexes;
    const QJsonArray array = entries.toArray();
    for (qsizetype i = 0; i < array.size(); ++i) {
      readProfile(array.at(i), i, indexes, backup.profiles, problems);
    }
  }

  if (!problems.isEmpty()) {
    backup = BridgeProfileBackup();
    return false;
  }
  return true;
}

bool BridgeProfileBackup::check(BridgeProfileBackup &checked, QStringList &problems) const
{
  return fromJson(toJson(), checked, problems);
}

bool BridgeProfileBackup::fitsDevice(int profileCount, QStringList &problems) const
{
  problems.clear();
  for (auto it = profiles.cbegin(); it != profiles.cend(); ++it) {
    if (it.key() >= profileCount) {
      problems.append(tr("This device has no profile %1.").arg(it.key()));
    }
  }
  if (activeProfile >= profileCount) {
    problems.append(tr("This device has no profile %1 to make active.").arg(activeProfile));
  }
  return problems.isEmpty();
}

bool BridgeProfileBackup::isUnset(int index, const DeviceProfile &profile)
{
  const DeviceProfile unset = unsetProfile(index);
  DeviceProfile zero;
  std::memset(&zero, 0, sizeof(zero));

  return std::memcmp(&profile, &unset, sizeof(profile)) == 0 || std::memcmp(&profile, &zero, sizeof(profile)) == 0;
}

QString BridgeProfileBackup::unusedFileName(const QDir &folder, const QString &screenName, const QDate &date)
{
  const QString stem = screenName.trimmed().isEmpty() ? QStringLiteral("bridge") : screenName.trimmed();
  const QString baseName = QStringLiteral("%1-profiles-%2").arg(stem, date.toString(QStringLiteral("yyyyMMdd")));
  QString path = folder.filePath(baseName + QStringLiteral(".json"));
  for (int suffix = 2; QFile::exists(path); ++suffix) {
    path = folder.filePath(QStringLiteral("%1-%2.json").arg(baseName).arg(suffix));
  }
  return path;
}

DeviceProfile BridgeProfileBackup::unsetProfile(int index)
{
  // device_config_get_profile() in the firmware
  DeviceProfile unset;
  std::memset(&unset, 0, sizeof(unset));
  std::snprintf(unset.hostname, sizeof(unset.hostname), "Slot %d", index);
  unset.slot = static_cast<uint8_t>(index);
  unset.screenWidth = 1080;
  unset.screenHeight = 2424;
  unset.speed = 30;
  return unset;
}

} // namespace deskflow::gui
