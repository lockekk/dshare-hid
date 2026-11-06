/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "DisplayLayoutStore.h"

#include "DisplayMonitors.h"
#include "common/ComputerShapeMessage.h"
#include "common/Settings.h"
#include "gui/config/ComputerList.h"
#include "gui/config/ServerConfig.h"
#include "gui/core/BridgeClientConfigManager.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTextStream>

#include <algorithm>
#include <array>

namespace deskflow::gui {

namespace {

QString s_written;

constexpr int kVersion = 1;
constexpr QSize kUnknownSize(1920, 1080);

const auto kKeyVersion = QStringLiteral("version");
const auto kKeySaved = QStringLiteral("saved");
const auto kKeyActive = QStringLiteral("active");
const auto kKeyClients = QStringLiteral("clients");
const auto kKeySizes = QStringLiteral("sizes");
const auto kKeyName = QStringLiteral("name");
const auto kKeyWidth = QStringLiteral("width");
const auto kKeyHeight = QStringLiteral("height");
const auto kKeyPlacement = QStringLiteral("placement");
const auto kKeyAnchor = QStringLiteral("anchor");
const auto kKeySide = QStringLiteral("side");
const auto kKeyOffset = QStringLiteral("offset");
const auto kKeyWholeEdge = QStringLiteral("wholeEdge");

constexpr std::array<std::pair<LayoutSide, const char *>, 4> kSideNames = {{
    {LayoutSide::Left, "left"},
    {LayoutSide::Right, "right"},
    {LayoutSide::Up, "up"},
    {LayoutSide::Down, "down"},
}};

QString sideToText(LayoutSide side)
{
  const auto found = std::ranges::find(kSideNames, side, &std::pair<LayoutSide, const char *>::first);
  return QString::fromLatin1(found->second);
}

std::optional<LayoutSide> sideFromText(const QString &text)
{
  for (const auto &[side, name] : kSideNames) {
    if (text == QLatin1String(name))
      return side;
  }
  return std::nullopt;
}

QJsonObject placementToJson(const LayoutPlacement &placement)
{
  return {
      {kKeyAnchor, placement.anchor},
      {kKeySide, sideToText(placement.side)},
      {kKeyOffset, placement.offset},
      {kKeyWholeEdge, placement.wholeEdge}
  };
}

std::optional<LayoutPlacement> placementFromJson(const QJsonObject &json)
{
  const auto side = sideFromText(json.value(kKeySide).toString());
  if (!side.has_value())
    return std::nullopt;
  return LayoutPlacement{
      json.value(kKeyAnchor).toString(), *side, json.value(kKeyOffset).toInt(), json.value(kKeyWholeEdge).toBool()
  };
}

} // namespace

QString DisplayLayoutStore::filePath()
{
  return QStringLiteral("%1/screen-layout.json").arg(Settings::settingsPath());
}

StoredLayout DisplayLayoutStore::load()
{
  QFile file(filePath());
  if (!file.open(QIODevice::ReadOnly))
    return {};

  QJsonParseError error;
  const auto document = QJsonDocument::fromJson(file.readAll(), &error);
  if (error.error != QJsonParseError::NoError || !document.isObject()) {
    qWarning() << "ignoring unreadable screen layout:" << file.fileName();
    return {};
  }

  const auto root = document.object();
  if (root.value(kKeyVersion).toInt() != kVersion) {
    qWarning() << "ignoring screen layout from another version:" << file.fileName();
    return {};
  }

  StoredLayout layout;
  layout.saved = root.value(kKeySaved).toBool();
  layout.active = root.value(kKeyActive).toBool();

  const auto sizes = root.value(kKeySizes).toObject();
  for (auto it = sizes.begin(); it != sizes.end(); ++it) {
    const auto size = it.value().toObject();
    layout.sizes.insert(it.key(), QSize(size.value(kKeyWidth).toInt(), size.value(kKeyHeight).toInt()));
  }

  const auto clients = root.value(kKeyClients).toArray();
  for (const auto &value : clients) {
    const auto json = value.toObject();
    LayoutClient client;
    client.name = json.value(kKeyName).toString();
    if (client.name.isEmpty())
      continue;
    if (json.contains(kKeyPlacement))
      client.placement = placementFromJson(json.value(kKeyPlacement).toObject());
    layout.clients.append(client);
  }
  return layout;
}

bool DisplayLayoutStore::save(const StoredLayout &layout)
{
  auto sizes = layout.sizes;
  QJsonArray clients;
  for (const auto &client : layout.clients) {
    QJsonObject json{{kKeyName, client.name}};
    if (client.placement.has_value())
      json.insert(kKeyPlacement, placementToJson(*client.placement));
    clients.append(json);
    if (client.sizeKnown)
      sizes.insert(client.name, client.size);
  }

  QJsonObject sizesJson;
  for (auto it = sizes.cbegin(); it != sizes.cend(); ++it)
    sizesJson.insert(it.key(), QJsonObject{{kKeyWidth, it.value().width()}, {kKeyHeight, it.value().height()}});

  const QJsonObject root{
      {kKeyVersion, kVersion},
      {kKeySaved, layout.saved},
      {kKeyActive, layout.active},
      {kKeyClients, clients},
      {kKeySizes, sizesJson}
  };

  // a half-written file would be ignored, which would lose the layout
  QSaveFile file(filePath());
  if (!file.open(QIODevice::WriteOnly)) {
    qWarning() << "cannot write screen layout:" << file.fileName();
    return false;
  }
  file.write(QJsonDocument(root).toJson());
  return file.commit();
}

bool DisplayLayoutStore::isActive()
{
  return load().active;
}

LayoutState
DisplayLayoutStore::buildState(const ComputerList &computers, const QString &serverName, const StoredLayout &stored)
{
  LayoutState state;
  state.serverName = serverName;
  state.monitors = DisplayMonitors::current();
  state.freeEdges = DisplayMonitors::freeEdges();

  QStringList cells;
  QStringList added;
  for (const auto &computer : computers) {
    cells.append(computer.name());
    if (computer.isNull() || computer.name() == serverName)
      continue;

    LayoutClient client;
    client.name = computer.name();
    client.bridge = !BridgeClientConfigManager::findConfigByScreenName(client.name).isEmpty();
    client.sizeKnown = stored.sizes.contains(client.name);
    client.size = stored.sizes.value(client.name, kUnknownSize);

    const auto found = std::ranges::find(stored.clients, client.name, &LayoutClient::name);
    if (found != stored.clients.end())
      client.placement = found->placement;
    else
      added.append(client.name);
    state.clients.append(client);
  }

  // a stale anchor would leave this computer parked for good
  QSet<QString> names;
  for (const auto &client : std::as_const(state.clients))
    names.insert(client.name);
  for (auto &client : state.clients) {
    if (client.placement.has_value() && !client.placement->anchor.isEmpty() &&
        !names.contains(client.placement->anchor))
      client.placement.reset();
  }

  if (!stored.saved) {
    const auto columns = Settings::value(Settings::Server::GridWidth).toInt();
    DisplayLayout::seedFromGrid(state, cells, columns);
    return state;
  }

  for (const auto &name : std::as_const(added)) {
    const auto client = std::ranges::find(state.clients, name, &LayoutClient::name);
    client->placement = DisplayLayout::defaultPlacement(state, *client);
  }
  return state;
}

QString DisplayLayoutStore::linkText(const ServerConfig &config)
{
  const auto stored = load();
  if (!stored.active)
    return {};

  // a link from a computer the core doesn't know stops the server starting
  const auto serverName = config.getServerName();
  if (!config.computerExists(serverName))
    return {};

  const auto state = buildState(config.computers(), serverName, stored);
  return DisplayLayout::linkText(state, DisplayLayout::solve(state));
}

void DisplayLayoutStore::writeLinks(QTextStream &stream, const ServerConfig &config)
{
  const auto text = linkText(config);
  stream << text;

  // "Save configuration as" comes through here too, and the core doesn't read that file
  const auto *file = qobject_cast<const QFile *>(stream.device());
  if (file != nullptr && file->fileName() == Settings::defaultValue(Settings::Server::ExternalConfigFile).toString())
    s_written = text;
}

QString DisplayLayoutStore::writtenLinks()
{
  return s_written;
}

bool DisplayLayoutStore::coreUsesLinks(bool serverMode)
{
  return serverMode && !Settings::value(Settings::Server::ExternalConfig).toBool();
}

void DisplayLayoutStore::rememberSize(const QString &name, const QSize &size)
{
  if (name.isEmpty() || size.isEmpty())
    return;
  auto layout = load();
  if (layout.sizes.value(name) == size)
    return;
  layout.sizes.insert(name, size);
  save(layout);
}

void DisplayLayoutStore::rememberShape(const QString &message)
{
  if (const auto shape = parseComputerShape(message))
    rememberSize(shape->name, shape->size);
}

void DisplayLayoutStore::rename(const QString &oldName, const QString &newName)
{
  auto layout = load();
  if (layout.sizes.contains(oldName))
    layout.sizes.insert(newName, layout.sizes.take(oldName));
  for (auto &client : layout.clients) {
    if (client.name == oldName)
      client.name = newName;
    if (client.placement.has_value() && client.placement->anchor == oldName)
      client.placement->anchor = newName;
  }
  save(layout);
  renameInProfiles(oldName, newName);
}

void DisplayLayoutStore::renameInProfiles(const QString &oldName, const QString &newName)
{
  const auto configs = BridgeClientConfigManager::getAllConfigFiles();
  for (const auto &config : configs) {
    const auto profiles = BridgeClientConfigManager::profileIds(config);
    for (const int profile : profiles) {
      auto placement = decodePlacement(BridgeClientConfigManager::readProfilePlacement(config, profile));
      if (!placement.has_value() || !placement->has_value() || (*placement)->anchor != oldName)
        continue;
      (*placement)->anchor = newName;
      BridgeClientConfigManager::writeProfilePlacement(config, profile, encodePlacement(*placement));
    }
  }
}

QString DisplayLayoutStore::encodePlacement(const std::optional<LayoutPlacement> &placement)
{
  if (!placement.has_value())
    return QStringLiteral("none");
  return QStringLiteral("%1|%2|%3|%4")
      .arg(sideToText(placement->side))
      .arg(placement->offset)
      .arg(placement->wholeEdge ? 1 : 0)
      .arg(placement->anchor);
}

std::optional<std::optional<LayoutPlacement>> DisplayLayoutStore::decodePlacement(const QString &text)
{
  if (text == QLatin1String("none"))
    return std::optional<LayoutPlacement>{};

  const auto parts = text.split(QLatin1Char('|'));
  if (parts.size() != 4)
    return std::nullopt;

  const auto side = sideFromText(parts.at(0));
  bool offsetOk = false;
  const int offset = parts.at(1).toInt(&offsetOk);
  if (!side.has_value() || !offsetOk)
    return std::nullopt;
  return LayoutPlacement{parts.at(3), *side, offset, parts.at(2) == QLatin1String("1")};
}

std::optional<LayoutPlacement> DisplayLayoutStore::placementFromGridOffset(const QPoint &offset)
{
  using enum LayoutSide;
  if (offset == QPoint(-1, 0))
    return LayoutPlacement{{}, Left, 0, true};
  if (offset == QPoint(1, 0))
    return LayoutPlacement{{}, Right, 0, true};
  if (offset == QPoint(0, -1))
    return LayoutPlacement{{}, Up, 0, true};
  if (offset == QPoint(0, 1))
    return LayoutPlacement{{}, Down, 0, true};
  return std::nullopt;
}

std::optional<LayoutPlacement> DisplayLayoutStore::placementOf(const QString &name)
{
  const auto layout = load();
  const auto found = std::ranges::find(layout.clients, name, &LayoutClient::name);
  return found == layout.clients.end() ? std::nullopt : found->placement;
}

bool DisplayLayoutStore::applyPlacement(const QString &name, const std::optional<LayoutPlacement> &placement)
{
  auto layout = load();
  const auto found = std::ranges::find(layout.clients, name, &LayoutClient::name);
  if (found == layout.clients.end() || found->placement == placement)
    return false;
  found->placement = placement;
  return save(layout);
}

bool DisplayLayoutStore::applyProfile(const QString &configPath, int profile, const QString &name)
{
  const auto decoded = decodePlacement(BridgeClientConfigManager::readProfilePlacement(configPath, profile));
  if (decoded.has_value())
    return applyPlacement(name, *decoded);
  return applyPlacement(
      name, placementFromGridOffset(BridgeClientConfigManager::readProfileScreenLocation(configPath, profile))
  );
}

void DisplayLayoutStore::rememberProfile(const QString &configPath, int profile, const QString &name)
{
  BridgeClientConfigManager::writeProfilePlacement(configPath, profile, encodePlacement(placementOf(name)));
}

} // namespace deskflow::gui
