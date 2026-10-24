/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#include "BridgeRequests.h"

#include <QByteArray>

#include <cstring>
#include <utility>

namespace deskflow::bridge::requests {

namespace {

const QString kOk = QStringLiteral("ok");
const QString kError = QStringLiteral("error");
const QString kIn = QStringLiteral("in");
const QString kOut = QStringLiteral("out");

QString transportError(const CdcTransport &transport)
{
  return QString::fromStdString(transport.lastError());
}

bool parseIndex(const QStringList &arguments, uint8_t &index, QString &error)
{
  bool ok = false;
  const int value = arguments.value(0).toInt(&ok);
  if (!ok || value < 0 || value > 0xFF) {
    error = QStringLiteral("invalid profile index");
    return false;
  }
  index = static_cast<uint8_t>(value);
  return true;
}

// hex that QByteArray::fromHex() decodes without skipping anything
bool fromHex(const QString &field, QByteArray &bytes)
{
  const QByteArray hex = field.toLatin1();
  bytes = QByteArray::fromHex(hex);
  return bytes.toHex() == hex.toLower();
}

// older firmware doesn't report how many profiles it has
constexpr int kFallbackProfileCount = 6;

bool readProfiles(CdcTransport &transport, QStringList &fields, QString &error)
{
  const auto &config = transport.deviceConfig();
  const bool countKnown = config.totalProfiles > 0;
  const int count = countKnown ? config.totalProfiles : kFallbackProfileCount;
  fields = {QString::number(config.activeProfile)};
  for (int i = 0; i < count; ++i) {
    DeviceProfile profile;
    if (!transport.getProfile(static_cast<uint8_t>(i), profile)) {
      if (countKnown) {
        error = transportError(transport);
        return false;
      }
      std::memset(&profile, 0, sizeof(profile)); // shown as an empty slot
    }
    fields.append(encodeProfile(profile));
  }
  return true;
}

// the reply to a command that changed the device
bool readProfilesAfterChange(CdcTransport &transport, QStringList &fields, QString &error)
{
  if (readProfiles(transport, fields, error)) {
    return true;
  }
  error = QStringLiteral("the device was changed, but reading its profiles back failed: %1").arg(error);
  return false;
}

} // namespace

QString formatRequest(quint64 id, const QString &command, const QStringList &arguments)
{
  QStringList parts{kRequest, QString::number(id), command};
  parts += arguments;
  return parts.join(QLatin1Char(' '));
}

bool parseRequest(const QString &line, Request &request)
{
  const QStringList parts = line.trimmed().split(QLatin1Char(' '), Qt::SkipEmptyParts);
  if (parts.size() < 3 || parts.at(0) != kRequest) {
    return false;
  }
  bool ok = false;
  request.id = parts.at(1).toULongLong(&ok);
  request.command = parts.at(2);
  request.arguments = parts.mid(3);
  return ok;
}

QString formatResponse(quint64 id, bool ok, const QStringList &fields)
{
  QStringList parts{kResponse, QString::number(id)};
  if (ok) {
    parts.append(kOk);
    parts += fields;
  } else {
    parts.append(kError);
    parts.append(fields.value(0).simplified()); // one line
  }
  return parts.join(QLatin1Char(' '));
}

bool parseResponse(const QString &line, quint64 &id, bool &ok, QStringList &fields)
{
  const qsizetype start = line.indexOf(kResponse);
  if (start < 0) {
    return false;
  }
  const QStringList parts = line.mid(start).split(QLatin1Char(' '), Qt::SkipEmptyParts);
  if (parts.size() < 3 || parts.at(0) != kResponse) {
    return false;
  }
  bool validId = false;
  id = parts.at(1).toULongLong(&validId);
  if (!validId) {
    return false;
  }
  if (parts.at(2) == kOk) {
    ok = true;
    fields = parts.mid(3);
    return true;
  }
  if (parts.at(2) == kError) {
    ok = false;
    fields = {parts.mid(3).join(QLatin1Char(' '))};
    return true;
  }
  return false;
}

QString formatFocus(bool entered)
{
  return QStringLiteral("%1 %2").arg(kFocus, entered ? kIn : kOut);
}

bool parseFocus(const QString &line, bool &entered)
{
  const qsizetype start = line.indexOf(kFocus);
  if (start < 0) {
    return false;
  }
  const QStringList parts = line.mid(start).split(QLatin1Char(' '), Qt::SkipEmptyParts);
  if (parts.size() != 2 || parts.at(0) != kFocus || (parts.at(1) != kIn && parts.at(1) != kOut)) {
    return false;
  }
  entered = (parts.at(1) == kIn);
  return true;
}

bool execute(
    CdcTransport &transport, const QString &command, const QStringList &arguments, QStringList &fields, QString &error
)
{
  fields.clear();
  if (command == kProfiles) {
    return readProfiles(transport, fields, error);
  }

  if (command == kSwitch || command == kErase) {
    uint8_t index = 0;
    if (!parseIndex(arguments, index, error)) {
      return false;
    }
    const bool done = (command == kSwitch) ? transport.switchProfile(index) : transport.eraseProfile(index);
    if (!done) {
      error = transportError(transport);
      return false;
    }
    return readProfilesAfterChange(transport, fields, error);
  }

  if (command == kSave) {
    uint8_t index = 0;
    DeviceProfile profile;
    if (!parseIndex(arguments, index, error)) {
      return false;
    }
    if (!decodeProfile(arguments.value(1), profile)) {
      error = QStringLiteral("invalid profile");
      return false;
    }
    if (!transport.setProfile(index, profile)) {
      error = transportError(transport);
      return false;
    }
    return readProfilesAfterChange(transport, fields, error);
  }

  if (command == kGetName) {
    std::string name;
    if (!transport.fetchDeviceName(name)) {
      error = transportError(transport);
      return false;
    }
    if (!name.empty()) {
      fields.append(encodeText(QString::fromStdString(name)));
    }
    return true;
  }

  if (command == kSetName) {
    const QString name = decodeText(arguments.value(0));
    if (name.isEmpty()) {
      error = QStringLiteral("invalid name");
      return false;
    }
    if (!transport.setDeviceName(name.toStdString())) {
      error = transportError(transport);
      return false;
    }
    return true;
  }

  error = QStringLiteral("unknown command: %1").arg(command);
  return false;
}

bool changesActiveProfile(const QString &command)
{
  return command == kSwitch || command == kSave || command == kErase;
}

QString encodeProfile(const DeviceProfile &profile)
{
  return QString::fromLatin1(QByteArray(reinterpret_cast<const char *>(&profile), sizeof(profile)).toHex());
}

bool decodeProfile(const QString &field, DeviceProfile &profile)
{
  QByteArray bytes;
  if (!fromHex(field, bytes) || bytes.size() != static_cast<qsizetype>(sizeof(profile))) {
    return false;
  }
  std::memcpy(&profile, bytes.constData(), sizeof(profile));
  return true;
}

bool decodeProfiles(const QStringList &fields, int &activeIndex, QList<DeviceProfile> &profiles)
{
  bool ok = false;
  activeIndex = fields.value(0).toInt(&ok);
  if (!ok) {
    return false;
  }
  profiles.clear();
  for (qsizetype i = 1; i < fields.size(); ++i) {
    DeviceProfile profile;
    if (!decodeProfile(fields.at(i), profile)) {
      return false;
    }
    profiles.append(profile);
  }
  return true;
}

QString encodeText(const QString &text)
{
  return QString::fromLatin1(text.toUtf8().toHex());
}

QString decodeText(const QString &field)
{
  QByteArray bytes;
  return fromHex(field, bytes) ? QString::fromUtf8(bytes) : QString();
}

void Queue::push(std::string line)
{
  std::scoped_lock lock{m_mutex};
  m_lines.push_back(std::move(line));
}

std::vector<std::string> Queue::takeAll()
{
  std::scoped_lock lock{m_mutex};
  return std::exchange(m_lines, {});
}

void Queue::postUnlessClosed(const std::function<void()> &post)
{
  std::scoped_lock lock{m_mutex};
  if (!m_closed) {
    post();
  }
}

void Queue::close()
{
  std::scoped_lock lock{m_mutex};
  m_closed = true;
}

} // namespace deskflow::bridge::requests
