/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>
#include <QStringList>

namespace deskflow::gui {

/**
 * @brief Manages bridge client configuration files
 *
 * Responsibilities:
 * - Find config files by serial number
 * - Create default config files for new devices
 * - Read screen names from config files
 */
class BridgeClientConfigManager
{
public:
  /**
   * @brief Find config file(s) matching the given serial number
   * @param serialNumber Device serial number
   * @return List of matching config file paths (may be empty or have multiple matches)
   */
  static QStringList findConfigsBySerialNumber(const QString &serialNumber);

  /**
   * @brief Create default config file for a new device
   * @param serialNumber Device serial number
   * @param devicePath USB CDC device path (for default screen name)
   * @return Path to created config file
   */
  static QString createDefaultConfig(const QString &serialNumber, const QString &devicePath);

  /**
   * @brief Read screen name from config file
   * @param configPath Path to config file
   * @return Screen name, or empty string if not found
   */
  static QString readScreenName(const QString &configPath);

  /**
   * @brief Read serial number from config file
   * @param configPath Path to config file
   * @return Serial number, or empty string if not found
   */
  static QString readSerialNumber(const QString &configPath);

  /**
   * @brief Get all bridge client config files
   * @return List of absolute paths to all .conf files in bridge-clients directory
   */
  static QStringList getAllConfigFiles();

  /**
   * @brief Get bridge clients config directory
   * @return Path to ~/.config/deskflow/bridge-clients/
   */
  static QString bridgeClientsDir();

private:
  /**
   * @brief Generate unique config file name
   * @param baseName Base name (without .conf extension)
   * @return Full path to unique config file
   */
  static QString generateUniqueConfigPath(const QString &baseName);
};

} // namespace deskflow::gui
