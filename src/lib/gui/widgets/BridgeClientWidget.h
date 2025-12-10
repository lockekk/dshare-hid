/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QString>

namespace deskflow::gui {

struct UsbDeviceInfo;
class Esp32HidToolsWidget;

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
      const QString &screenName, const QString &devicePath, const QString &configPath, QWidget *parent = nullptr
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

  /**
   * @brief Set device availability (enable/disable widget)
   * @param devicePath New device path (empty if device not available)
   * @param available true if device is plugged in, false to gray out
   */
  void setDeviceAvailable(const QString &devicePath, bool available);

  /**
   * @brief Manually update activation state indicator
   */
  void setActivationState(const QString &activationState);

  /**
   * @brief Update firmware device name label
   */
  void setDeviceName(const QString &deviceName);

  /**
   * @brief Enable/disable controls when another config with the same serial is active
   * @param locked true to disable controls, false to allow interaction
   * @param reason Optional tooltip that explains why the controls are disabled
   */
  void setGroupLocked(bool locked, const QString &reason = QString());

  /**
   * @brief Check if device is available
   */
  bool isDeviceAvailable() const
  {
    return m_deviceAvailable;
  }

Q_SIGNALS:
  /**
   * @brief Emitted when connect button is toggled
   * @param devicePath Device path
   * @param configPath Config file path
   * @param connected true to connect, false to disconnect
   */
  Q_SIGNAL void connectToggled(const QString &devicePath, const QString &configPath, bool shouldConnect);
  Q_SIGNAL void configureClicked(const QString &devicePath, const QString &configPath);
  Q_SIGNAL void deleteClicked(const QString &devicePath, const QString &configPath);
  Q_SIGNAL void refreshDevicesRequested();

protected:
private Q_SLOTS:
  void onConnectToggled(bool checked);
  void onConfigureClicked();
  void onDeleteClicked();

private:
  void refreshOrientationLabel();
  void refreshActivationStateLabel();
  void refreshDeviceNameLabel();
  void refreshButtonStates();

  QString m_screenName;
  QString m_devicePath;
  QString m_configPath;
  bool m_isConnected = false;
  bool m_deviceAvailable = false;
  bool m_groupLocked = false;
  QString m_groupLockReason;

  QPushButton *m_btnConnect;
  QPushButton *m_btnConfigure;
  QPushButton *m_btnDelete;
  QPushButton *m_btnFirmware;
  QLabel *m_deviceNameLabel;
  QLabel *m_activationStateLabel;
  QLabel *m_orientationLabel;
  QString m_deviceName;
  QString m_activationState;
  QString m_orientation;
};

} // namespace deskflow::gui
