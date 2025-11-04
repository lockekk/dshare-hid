/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "BridgeClientConfigManager.h"

#include "common/Settings.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>

namespace deskflow::gui {

QString BridgeClientConfigManager::bridgeClientsDir()
{
  QString path = QStringLiteral("%1/bridge-clients").arg(Settings::settingsPath());
  QDir dir(path);
  if (!dir.exists()) {
    if (!dir.mkpath(path)) {
      qWarning() << "Failed to create bridge-clients directory:" << path;
    }
  }
  return path;
}

QStringList BridgeClientConfigManager::findConfigsBySerialNumber(const QString &serialNumber)
{
  QStringList matches;

  if (serialNumber.isEmpty()) {
    return matches;
  }

  QString dir = bridgeClientsDir();
  QDir configDir(dir);

  // Find all .conf files
  QStringList confFiles = configDir.entryList(QStringList() << "*.conf", QDir::Files);

  // Check each config file for matching serial number
  for (const QString &filename : confFiles) {
    QString filePath = configDir.absoluteFilePath(filename);
    QSettings config(filePath, QSettings::IniFormat);

    QString sn = config.value(Settings::Bridge::SerialNumber).toString();
    if (!sn.isEmpty() && sn == serialNumber) {
      matches.append(filePath);
    }
  }

  return matches;
}

QString BridgeClientConfigManager::createDefaultConfig(const QString &serialNumber, const QString &devicePath)
{
  // Default screen name uses device basename with Bridge- prefix
  const QString deviceBaseName = QFileInfo(devicePath).fileName();
  const QString defaultScreenName = QStringLiteral("Bridge-%1").arg(deviceBaseName);

  // Sanitize screen name for filesystem usage
  QString sanitizedBaseName = defaultScreenName;
  static const QRegularExpression kInvalidChars(QStringLiteral("[^a-zA-Z0-9_-]"));
  sanitizedBaseName.replace(kInvalidChars, QStringLiteral("_"));

  // Generate unique config path using sanitized screen name
  QString configPath = generateUniqueConfigPath(sanitizedBaseName);

  // Create config file with default values
  QSettings config(configPath, QSettings::IniFormat);

  // [bridge] section
  config.setValue(Settings::Bridge::SerialNumber, serialNumber);
  config.setValue(Settings::Bridge::ScreenWidth, 1920);  // Default resolution
  config.setValue(Settings::Bridge::ScreenHeight, 1080);
  config.setValue(Settings::Bridge::ScreenOrientation, "landscape");

  // [client] section
  config.setValue(Settings::Client::InvertScrollDirection, false);
  config.setValue(Settings::Client::ScrollSpeed, 120);
  config.setValue(Settings::Client::LanguageSync, true);

  // [core] section
  config.setValue(Settings::Core::ScreenName, defaultScreenName);
  config.setValue(Settings::Core::RestartOnFailure, true);
  config.setValue(Settings::Core::ProcessMode, Settings::Desktop);
  config.setValue(Settings::Core::CoreMode, Settings::Client);

  // [log] section
  config.setValue(Settings::Log::Level, "INFO");
  config.setValue(Settings::Log::ToFile, false);

  config.remove(QStringLiteral("security"));
  config.sync();

  qDebug() << "Created default bridge client config:" << configPath;
  return configPath;
}

void BridgeClientConfigManager::removeLegacySecuritySettings(const QString &configPath)
{
  QSettings config(configPath, QSettings::IniFormat);
  if (config.childGroups().contains(QStringLiteral("security")) ||
      config.contains(Settings::Security::TlsEnabled) ||
      config.contains(Settings::Security::CheckPeers)) {
    config.beginGroup(QStringLiteral("security"));
    config.remove(QLatin1String(""));
    config.endGroup();
    config.sync();
    qDebug() << "Removed legacy security keys from bridge config" << configPath;
  }
}

QString BridgeClientConfigManager::readScreenName(const QString &configPath)
{
  QSettings config(configPath, QSettings::IniFormat);
  return config.value(Settings::Core::ScreenName).toString();
}

QString BridgeClientConfigManager::readSerialNumber(const QString &configPath)
{
  QSettings config(configPath, QSettings::IniFormat);
  return config.value(Settings::Bridge::SerialNumber).toString();
}

QStringList BridgeClientConfigManager::getAllConfigFiles()
{
  QString dir = bridgeClientsDir();
  QDir configDir(dir);

  // Find all .conf files
  QStringList confFiles = configDir.entryList(QStringList() << "*.conf", QDir::Files);

  // Convert to absolute paths
  QStringList absolutePaths;
  for (const QString &filename : confFiles) {
    absolutePaths.append(configDir.absoluteFilePath(filename));
  }

  return absolutePaths;
}

QString BridgeClientConfigManager::generateUniqueConfigPath(const QString &baseName)
{
  QString dir = bridgeClientsDir();
  QString configPath = QStringLiteral("%1/%2.conf").arg(dir, baseName);

  // If file doesn't exist, use it
  if (!QFileInfo::exists(configPath)) {
    return configPath;
  }

  // Otherwise, append a number
  int counter = 1;
  while (true) {
    QString numberedPath = QStringLiteral("%1/%2-%3.conf").arg(dir, baseName).arg(counter);
    if (!QFileInfo::exists(numberedPath)) {
      return numberedPath;
    }
    counter++;
  }
}

} // namespace deskflow::gui
