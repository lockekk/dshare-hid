/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#include <QButtonGroup>
#include <QCheckBox>
#include <QDialog>
#include <QGroupBox> // Added
#include <QLabel>    // Added
#include <QLineEdit>
#include <QMap> // Added
#include <QPushButton>
#include <QRadioButton>
#include <QSet>
#include <QSpinBox>
#include <QTabBar> // Added
#include <QVBoxLayout>

#include "platform/bridge/CdcTransport.h" // Added for DeviceProfile

namespace deskflow::gui {

struct BridgeProfileSet;

class BridgeClientConfigDialog : public QDialog
{
  Q_OBJECT

public:
  explicit BridgeClientConfigDialog(const QString &configPath, const QString &devicePath, QWidget *parent = nullptr);

  QString screenName() const;
  int screenWidth() const;
  int screenHeight() const;
  QString screenOrientation() const;
  QString configPath() const
  {
    return m_configPath;
  }

  QString deviceName() const;
  bool deviceNameChanged() const;
  bool bluetoothKeepAlive() const;
  bool autoConnect() const;

Q_SIGNALS:
  void configChanged(const QString &oldConfigPath, const QString &newConfigPath);

private Q_SLOTS:
  void onAccepted();

  void onProfileResetClicked();
  void onProfileBackupClicked();
  void onProfileRestoreClicked();

private:
  void loadConfig();
  void saveConfig();
  QString renameConfigFile(const QString &newScreenName);
  void setupProfileUI(QVBoxLayout *mainLayout);
  void loadProfilesFromDevice();
  void showProfiles(const BridgeProfileSet &profiles);
  void rememberDeviceProfiles(const BridgeProfileSet &profiles);
  bool applyProfiles();
  void updateProfileDetailUI(int index);

  QString m_configPath;
  QString m_devicePath;
  QString m_originalScreenName;

  QLineEdit *m_editScreenName;
  QLineEdit *m_editDeviceName;
  QDoubleSpinBox *m_spinScrollScale;
  QCheckBox *m_checkInvertScroll;
  QCheckBox *m_checkBluetoothKeepAlive;
  QCheckBox *m_checkAutoConnect;

  bool eventFilter(QObject *watched, QEvent *event) override; // Added

  // Profile UI
  QGroupBox *m_profileGroup = nullptr;
  QLabel *m_lblProfileGroupTitle = nullptr; // Added for explicit title rendering
  QTabBar *m_profileTabBar = nullptr;       // Replaced QButtonGroup
  QLineEdit *m_editProfileName = nullptr;
  QSpinBox *m_spinProfileWidth = nullptr;
  QSpinBox *m_spinProfileHeight = nullptr;
  QButtonGroup *m_profileOrientationGroup = nullptr;
  QRadioButton *m_radioProfileLandscape = nullptr;
  QLabel *m_lblIconLandscape = nullptr; // Added
  QRadioButton *m_radioProfilePortrait = nullptr;
  QLabel *m_lblIconPortrait = nullptr; // Added
  QButtonGroup *m_profileHidModeGroup = nullptr;
  QRadioButton *m_radioProfileCombo = nullptr;
  QRadioButton *m_radioProfileMouseOnly = nullptr;

  QPushButton *m_btnProfileReset = nullptr;
  QPushButton *m_btnProfileBackup = nullptr;
  QPushButton *m_btnProfileRestore = nullptr;
  QCheckBox *m_checkBondLocation = nullptr;

  QString m_originalDeviceName;

  // Cache
  QMap<int, deskflow::bridge::DeviceProfile> m_profileCache;
  int m_selectedProfileIndex = -1;     // Currently viewed/edited in UI
  int m_deviceActiveProfileIndex = -1; // Truly active on device
  // the profiles as the device has them, and those the user changed in the form or reset
  // (saved and erased on OK)
  QMap<int, deskflow::bridge::DeviceProfile> m_deviceProfiles;
  QSet<int> m_editedProfiles;
  QSet<int> m_resetProfiles; // erased on OK
  bool m_showingProfile = false;
  int m_totalProfiles = 0;

  void onProfileTabChanged(int index);
  void saveUiToCache(int index);
  void updateTabLabels();
};

} // namespace deskflow::gui
