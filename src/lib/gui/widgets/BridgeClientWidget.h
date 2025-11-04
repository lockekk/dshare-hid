/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QGroupBox>
#include <QPushButton>
#include <QString>

namespace deskflow::gui {

struct UsbDeviceInfo;

/**
 * @brief Widget for managing a single bridge client
 *
 * Displays:
 * - Group box with screen name from config
 * - Connect button (toggleable - connected/disconnected)
 * - Configure button (opens config dialog)
 */
class BridgeClientWidget : public QGroupBox
{
  Q_OBJECT

public:
  /**
   * @brief Construct bridge client widget
   * @param screenName Screen name from config file
   * @param devicePath USB CDC device path (e.g., "/dev/ttyACM0")
   * @param configPath Path to config file
   * @param parent Parent widget
   */
  explicit BridgeClientWidget(
      const QString &screenName,
      const QString &devicePath,
      const QString &configPath,
      QWidget *parent = nullptr
  );

  ~BridgeClientWidget() override = default;

  /**
   * @brief Get screen name
   */
  QString screenName() const
  {
    return m_screenName;
  }

  /**
   * @brief Get device path
   */
  QString devicePath() const
  {
    return m_devicePath;
  }

  /**
   * @brief Get config file path
   */
  QString configPath() const
  {
    return m_configPath;
  }

  /**
   * @brief Get connection state
   */
  bool isConnected() const
  {
    return m_isConnected;
  }

  /**
   * @brief Set connection state
   */
  void setConnected(bool connected);

  /**
   * @brief Update screen name and config path
   * @param screenName New screen name
   * @param configPath New config file path
   */
  void updateConfig(const QString &screenName, const QString &configPath);

Q_SIGNALS:
  /**
   * @brief Emitted when connect button is toggled
   * @param devicePath Device path
   * @param connected true to connect, false to disconnect
   */
  void connectToggled(const QString &devicePath, bool connected);

  /**
   * @brief Emitted when configure button is clicked
   * @param devicePath Device path
   * @param configPath Config file path
   */
  void configureClicked(const QString &devicePath, const QString &configPath);

private Q_SLOTS:
  void onConnectToggled(bool checked);
  void onConfigureClicked();

private:
  QString m_screenName;
  QString m_devicePath;
  QString m_configPath;
  bool m_isConnected = false;

  QPushButton *m_btnConnect;
  QPushButton *m_btnConfigure;
};

} // namespace deskflow::gui
