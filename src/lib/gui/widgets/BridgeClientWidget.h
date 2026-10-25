/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#pragma once

#include "gui/core/BridgeDeviceService.h"

#include <QGroupBox>
#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QString>

class QHBoxLayout;
class QToolButton;

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
 * - While connected, the device's profiles (right of the device name) as buttons that switch to them
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
      const QString &screenName, const QString &serialNumber, const QString &devicePath, const QString &configPath,
      QWidget *parent = nullptr
  );

  ~BridgeClientWidget() override = default;

  /**
   * @brief The activation state as the device names it (activationStateToString()), in the user's language
   */
  static QString activationStateText(const QString &state);

  /**
   * @brief Get screen name
   */
  QString screenName() const
  {
    return m_screenName;
  }

  /**
   * @brief Get serial number
   */
  QString serialNumber() const
  {
    return m_serialNumber;
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
   * @brief Set server ready state
   */
  void setServerReady(bool ready);
  void resetConnectionStatus();

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
   * @brief Update active profile hostname label
   */
  void setActiveHostname(const QString &hostname);

  /**
   * @brief Update BLE connection status
   */
  void setBleConnected(bool connected);

  /**
   * @brief Show the device's profiles, the active one highlighted (cleared on disconnect)
   * @param profiles Profiles read from the device
   * @param activeIndex Index of the device's active profile
   */
  void setProfiles(const QList<BridgeProfileInfo> &profiles, int activeIndex);

  /**
   * @brief The device's profiles while connected (empty otherwise), and the active one's index
   */
  const QList<BridgeProfileInfo> &profiles() const
  {
    return m_profiles;
  }
  int activeProfileIndex() const
  {
    return m_activeProfileIndex;
  }

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
  Q_SIGNAL void profileSwitchRequested(const QString &devicePath, const QString &configPath, int profileIndex);
  /// the profiles shown, the active one, or the names they're shown under changed
  Q_SIGNAL void profilesChanged();
  Q_SIGNAL void refreshDevicesRequested();

protected:
  void changeEvent(QEvent *event) override;

private Q_SLOTS:
  void onConnectToggled(bool checked);
  void onConfigureClicked();

private:
  void updateActivationVisibility();
  void refreshActivationStateLabel();
  void refreshDeviceNameLabel();
  void refreshOrientationLabel();
  void refreshButtonStates();
  void refreshProfileButtons();
  void retranslateUi();

  QString m_screenName;
  QString m_serialNumber;
  QString m_devicePath;
  QString m_configPath;
  bool m_isConnected = false;
  bool m_deviceAvailable = false;
  bool m_serverReady = false;
  bool m_groupLocked = false;
  QString m_groupLockReason;

  QPushButton *m_btnConnect;
  QPushButton *m_btnConfigure;
  QPushButton *m_btnFirmware;
  QLabel *m_deviceNameLabel;
  QLabel *m_activeHostnameLabel;
  QLabel *m_activationStateLabel;
  QString m_deviceName;
  QString m_activeHostname;
  QString m_activationState;
  QString m_orientation;
  QLabel *m_orientationLabel = nullptr;
  bool m_isBleConnected = false;

  QWidget *m_profileRow = nullptr;
  QHBoxLayout *m_profileLayout = nullptr;
  QList<QToolButton *> m_profileButtons;
  QList<BridgeProfileInfo> m_profiles;
  int m_activeProfileIndex = -1;
};

} // namespace deskflow::gui
