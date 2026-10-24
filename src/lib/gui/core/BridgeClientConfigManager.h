/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#pragma once

#include <QPoint>
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
   * @brief Remove legacy security keys that are no longer used by bridge clients
   */
  static void removeLegacySecuritySettings(const QString &configPath);

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
   * @brief The name to show for a computer: for a bridge client, its device's name, which
   * reads better than its screen name (Bridge-<address>); the computer's name otherwise
   *
   * When bridge clients' devices share a name, the screen name is added to tell them apart.
   * @param computerName Screen name of the computer
   */
  static QString displayName(const QString &computerName);

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

  /**
   * @brief Delete a bridge client config file
   * @param configPath Path to config file to delete
   * @return true if deletion was successful, false otherwise
   */
  static bool deleteConfig(const QString &configPath);

  /**
   * @brief Find config file path by screen name
   * @param screenName Screen name to search for
   * @return Config file path if found, empty string otherwise
   */
  static QString findConfigByScreenName(const QString &screenName);

public:
  /**
   * @brief Read bonded screen location for a specific profile
   * @param configPath Path to config file
   * @param profileId Profile Index (e.g. 0)
   * @return QPoint(x, y) relative to server, or QPoint(0,0) if not found/invalid. Use hasProfileScreenLocation to check
   * validity.
   */
  static QPoint readProfileScreenLocation(const QString &configPath, int profileId);

  /**
   * @brief Write bonded screen location for a specific profile
   * @param configPath Path to config file
   * @param profileId Profile Index
   * @param location Relative location (e.g. QPoint(1, 0) for Right)
   */
  static void writeProfileScreenLocation(const QString &configPath, int profileId, const QPoint &location);

  /**
   * @brief Clear bonded screen location for a specific profile
   * @param configPath Path to config file
   * @param profileId Profile Index
   */
  static void clearProfileScreenLocation(const QString &configPath, int profileId);

  /**
   * @brief Check if a bonding exists for this profile
   */
  static bool hasProfileScreenLocation(const QString &configPath, int profileId);

private:
  /**
   * @brief Generate unique config file name
   * @param baseName Base name (without .conf extension)
   * @return Full path to unique config file
   */
  static QString generateUniqueConfigPath(const QString &baseName);
};

} // namespace deskflow::gui
