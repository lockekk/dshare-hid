/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "Settings.h"

#include "UrlConstants.h"
#include "base/Log.h"

#include <QCoreApplication>
#include <QFile>
#include <QRect>
#include <QRegularExpression>
#include <QStandardPaths>

Settings *Settings::instance()
{
  static Settings m;
  return &m;
}

void Settings::setSettingsFile(const QString &settingsFile)
{
  if (Settings::settingsFile() == settingsFile) {
    qDebug("settings file already set, skipping");
    return;
  }

  if (instance()->m_settings)
    instance()->m_settings->deleteLater();

  instance()->m_settings = new QSettings(settingsFile, QSettings::IniFormat, instance());
  instance()->m_settingsProxy->load(settingsFile);
  qInfo().noquote() << "settings file changed:" << instance()->m_settings->fileName();

  instance()->setupScreenName();
}

void Settings::setStateFile(const QString &stateFile)
{
  if (instance()->m_stateSettings->fileName() == stateFile) {
    qDebug("state file already set, skipping");
    return;
  }

  if (instance()->m_stateSettings)
    instance()->m_stateSettings->deleteLater();

  instance()->m_stateSettings = new QSettings(stateFile, QSettings::IniFormat, instance());
  qInfo().noquote() << "state file changed:" << instance()->m_stateSettings->fileName();
}

Settings::Settings(QObject *parent) : QObject(parent)
{
  QString fileToLoad;
#ifdef Q_OS_WIN
  const auto portableFile = portableSettingsFile();
  qDebug().noquote() << "checking for portable settings file at:" << portableFile;
  if (QFile(portableFile).exists())
    fileToLoad = portableFile;
#else
  if (const auto xdgConfigHome = qEnvironmentVariable("XDG_CONFIG_HOME"); !xdgConfigHome.isEmpty())
    fileToLoad = QStringLiteral("%1/%2/%2.conf").arg(xdgConfigHome, kAppName);
#endif
  else if (QFile(UserSettingFile).exists())
    fileToLoad = UserSettingFile;
  else if (QFile(SystemSettingFile).exists())
    fileToLoad = SystemSettingFile;
  else
    fileToLoad = UserSettingFile;

  m_settings = new QSettings(fileToLoad, QSettings::IniFormat, this);
  m_settingsProxy = std::make_shared<QSettingsProxy>();
  m_settingsProxy->load(fileToLoad);
  qInfo().noquote() << "initial settings file:" << m_settings->fileName();

  const auto xdgStateHome = qEnvironmentVariable("XDG_STATE_HOME");
  const auto stateBase = !xdgStateHome.isEmpty()
                             ? xdgStateHome
                             : QStandardPaths::standardLocations(QStandardPaths::GenericStateLocation).at(0);
  const auto stateFile = QStringLiteral("%1/%2.state").arg(stateBase, kAppName);

  m_stateSettings = new QSettings(stateFile, QSettings::IniFormat, this);

  cleanSettings();
  cleanStateSettings();
  setupScreenName();
}

void Settings::cleanSettings()
{
  const QStringList keys = m_settings->allKeys();
  for (const QString &key : keys) {
    if (key.startsWith(QStringLiteral("internalConfig")))
      continue;
    if (!m_validKeys.contains(key))
      m_settings->remove(key);
    if (m_settings->value(key).toString().isEmpty())
      m_settings->remove(key);
  }
}

void Settings::cleanStateSettings()
{
  const QStringList keys = m_stateKeys;
  for (const QString &key : keys) {
    if (!m_stateKeys.contains(key))
      m_stateSettings->remove(key);
    if (m_stateSettings->value(key).toString().isEmpty() && !m_stateSettings->value(key).toRect().isValid())
      m_stateSettings->remove(key);
  }
}

void Settings::setupScreenName()
{
  if (m_settings->value(Settings::Core::ScreenName).toString().isEmpty())
    m_settings->setValue(Settings::Core::ScreenName, cleanScreenName(QSysInfo::machineHostName()));
}

QString Settings::cleanScreenName(const QString &name)
{
  static const auto hyphen = QStringLiteral("-");
  static const auto space = QStringLiteral(" ");
  static const auto underscore = QStringLiteral("_");
  static const auto period = QStringLiteral(".");
  static const auto nothing = QStringLiteral("");
  static const auto nameRegex = QRegularExpression(QStringLiteral("[^\\w\\-\\.]"));

  QString cleanName = name.simplified();
  cleanName.replace(space, underscore);
  cleanName.replace(nameRegex, nothing);
  while (cleanName.startsWith(hyphen) || cleanName.startsWith(underscore) || cleanName.startsWith(period))
    cleanName.removeFirst();
  while (cleanName.endsWith(hyphen) || cleanName.endsWith(underscore) || cleanName.endsWith(period))
    cleanName.removeLast();
  if (cleanName.length() > 255) {
    cleanName.truncate(255);
    cleanName = cleanScreenName(cleanName);
  }
  return cleanName;
}

int Settings::logLevelToInt(const QString &level)
{
  if (level.isEmpty() || !m_logLevels.contains(level, Qt::CaseInsensitive))
    return 4;
  return static_cast<int>(m_logLevels.indexOf(level, 0, Qt::CaseInsensitive));
}

void Settings::setBridgeClientMode(bool enabled)
{
  instance()->m_bridgeClientMode = enabled;
}

bool Settings::isBridgeClientMode()
{
  return instance()->m_bridgeClientMode;
}

QVariant Settings::defaultValue(const QString &key)
{
  if (key == Gui::SymbolicTrayIcon)
    return false;

  if (m_defaultFalseValues.contains(key))
    return false;

  if (m_defaultTrueValues.contains(key))
    return true;

  // Disable automatic update check by default
  if (key == Gui::AutoUpdateCheck)
    return false;

  if (key == Security::Certificate) {
    // Bridge clients (in bridge-clients subdirectory) share the server's certificates
    const auto currentPath = instance()->settingsPath();
    const bool bridgeClient = instance()->m_bridgeClientMode || currentPath.contains("bridge-clients");
    if (bridgeClient) {
      return QStringLiteral("%1/%2.pem").arg(bridgeClientTlsDir(), kAppId);
    }
    return QStringLiteral("%1/%2.pem").arg(Settings::tlsDir(), kAppId);
  }

  if (key == Security::KeySize)
    return 2048;

  if (key == Log::File)
    return QStringLiteral("%1/%2.log").arg(QDir::homePath(), kAppId);

  if (key == Log::Level)
    return 4; // INFO

  if (key == Daemon::Elevate)
    return !Settings::isPortableMode();

  if (key == Gui::UpdateCheckUrl)
    return kUrlUpdateCheck;

  if (key == Server::ExternalConfigFile)
    return QStringLiteral("%1/%2-server.conf").arg(Settings::settingsPath(), kAppId);

  if (key == Core::Port)
    return 24800;

  if (key == Core::ProcessMode) {
#ifdef Q_OS_WIN
    if (!Settings::isPortableMode())
      return Settings::ProcessMode::Service;
#endif
    return Settings::ProcessMode::Desktop;
  }

  if (key == Daemon::LogFile)
    return QStringLiteral("%1/%2-daemon.log").arg(Settings::settingsPath(), kAppId);

  if (key == Client::ScrollSpeed)
    return 120;

  if (key == Bridge::SerialNumber) {
    return QString(); // Empty by default, populated from CDC device
  }

  if (key == Bridge::HostOs) {
    return QStringLiteral("unknown");
  }

  if (key == Bridge::ActivationState) {
    return QStringLiteral("unknown");
  }

  if (key == Bridge::DeviceName) {
    return QString();
  }

  if (key == Bridge::BluetoothKeepAlive) {
    return true;
  }

  if (key == Bridge::ActiveProfileHostname) {
    return QString();
  }

  if (key == Bridge::ActiveProfileOrientation) {
    return QStringLiteral("landscape");
  }

  return QVariant();
}

QString Settings::logLevelText()
{
  return Settings::m_logLevels.at(Settings::value(Log::Level).toInt());
}

QSettingsProxy &Settings::proxy()
{
  return *instance()->m_settingsProxy;
}

void Settings::save(bool emitSaving)
{
  if (emitSaving)
    Q_EMIT instance()->serverSettingsChanged();
  instance()->m_settings->sync();
  instance()->m_stateSettings->sync();
}

QStringList Settings::validKeys()
{
  return Settings::m_validKeys;
}

bool Settings::isWritable()
{
  return instance()->m_settings->isWritable();
}

bool Settings::isPortableMode()
{
  // Enable portable mode only if the portable settings file exists in the expected location.
  return QFile(portableSettingsFile()).exists();
}

QString Settings::settingsFile()
{
  return instance()->m_settings->fileName();
}

QString Settings::settingsPath()
{
#ifdef Q_OS_WIN
  if (!isPortableMode())
    return SystemDir;
#endif

  return QFileInfo(instance()->m_settings->fileName()).absolutePath();
}

QString Settings::tlsDir()
{
  return QStringLiteral("%1/tls").arg(instance()->settingsPath());
}

QString Settings::bridgeClientTlsDir()
{
  const QString bridgeFolder = QStringLiteral("bridge-clients");
  const QString currentPath = settingsPath();

  QDir bridgeDir(currentPath);
  QDir probe = bridgeDir;
  bool foundBridgeFolder = false;
  while (true) {
    if (probe.dirName().compare(bridgeFolder, Qt::CaseInsensitive) == 0) {
      foundBridgeFolder = true;
      break;
    }
    if (!probe.cdUp()) {
      break;
    }
  }

  if (foundBridgeFolder) {
    // One level up from "bridge-clients" is the server root that hosts tls/
    probe.cdUp();
    return probe.filePath(kTlsDirName);
  }

  return bridgeDir.filePath(kTlsDirName);
}

QString Settings::tlsTrustedServersDb()
{
  // Bridge clients (in bridge-clients subdirectory) share the server's trusted fingerprints
  auto currentPath = instance()->settingsPath();
  const bool bridgeClient = instance()->m_bridgeClientMode || currentPath.contains("bridge-clients");
  if (bridgeClient) {
    return QStringLiteral("%1/trusted-servers").arg(bridgeClientTlsDir());
  }
  return QStringLiteral("%1/trusted-servers").arg(instance()->tlsDir());
}

QString Settings::tlsTrustedClientsDb()
{
  return QStringLiteral("%1/trusted-clients").arg(instance()->tlsDir());
}

void Settings::setValue(const QString &key, const QVariant &value)
{
  const bool useState = Settings::m_stateKeys.contains(key) && !instance()->isPortableMode();
  auto settings = useState ? instance()->m_stateSettings : instance()->m_settings;

  if (settings->value(key) == value)
    return;

  if (!value.isValid())
    settings->remove(key);
  else {
    if (key == Settings::Core::ScreenName)
      settings->setValue(key, cleanScreenName(value.toString()));
    else
      settings->setValue(key, value);
  }

  settings->sync();
  Q_EMIT instance()->settingsChanged(key);
}

QVariant Settings::value(const QString &key)
{
  const bool useState = Settings::m_stateKeys.contains(key) && !instance()->isPortableMode();
  auto settings = useState ? instance()->m_stateSettings : instance()->m_settings;
  return settings->value(key, defaultValue(key));
}

void Settings::restoreDefaultSettings()
{
  for (const auto &key : m_validKeys) {
    instance()->setValue(key, defaultValue(key));
  }
}

QString Settings::portableSettingsFile()
{
  static const auto filename =
      QStringLiteral("%1/settings/%2.conf").arg(QCoreApplication::applicationDirPath(), kAppName);
  return filename;
}
