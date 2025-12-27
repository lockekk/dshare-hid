/*
 * Deskflow-hid -- created by locke.huang@gmail.com
 */

#include <QButtonGroup>
#include <QCheckBox>
#include <QDialog>
#include <QGroupBox> // Added
#include <QLineEdit>
#include <QMap> // Added
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTabBar> // Added
#include <QVBoxLayout>

#include "platform/bridge/CdcTransport.h" // Added for DeviceProfile

namespace deskflow::gui {

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

  int scrollSpeed() const;
  bool invertScroll() const;
  QString deviceName() const;
  bool deviceNameChanged() const;
  bool bluetoothKeepAlive() const;
  bool autoConnect() const;

Q_SIGNALS:
  void configChanged(const QString &oldConfigPath, const QString &newConfigPath);

private Q_SLOTS:
  void onAccepted();
  void onProfileToggled(int id, bool checked);
  void onProfileSaveClicked();
  void onProfileActivateClicked();
  void onProfileResetClicked();

private:
  void loadConfig();
  void saveConfig();
  QString renameConfigFile(const QString &newScreenName);
  void setupProfileUI(QVBoxLayout *mainLayout);
  void loadProfilesFromDevice();
  void updateProfileDetailUI(int index);

  QString m_configPath;
  QString m_devicePath;
  QString m_originalScreenName;

  QLineEdit *m_editScreenName;
  QLineEdit *m_editDeviceName;
  QSpinBox *m_spinScrollSpeed;
  QCheckBox *m_checkInvertScroll;
  QCheckBox *m_checkBluetoothKeepAlive;
  QCheckBox *m_checkAutoConnect;

  // Profile UI
  QGroupBox *m_profileGroup = nullptr;
  QTabBar *m_profileTabBar = nullptr; // Replaced QButtonGroup
  QLineEdit *m_editProfileName = nullptr;
  QSpinBox *m_spinProfileWidth = nullptr;
  QSpinBox *m_spinProfileHeight = nullptr;
  QButtonGroup *m_profileOrientationGroup = nullptr;
  QRadioButton *m_radioProfileLandscape = nullptr;
  QRadioButton *m_radioProfilePortrait = nullptr;
  QButtonGroup *m_profileHidModeGroup = nullptr;
  QRadioButton *m_radioProfileCombo = nullptr;
  QRadioButton *m_radioProfileMouseOnly = nullptr;

  QPushButton *m_btnProfileSave = nullptr;
  QPushButton *m_btnProfileActivate = nullptr;
  QPushButton *m_btnProfileReset = nullptr;

  QString m_originalDeviceName;

  // Cache
  QMap<int, deskflow::bridge::DeviceProfile> m_profileCache;
  int m_selectedProfileIndex = -1;     // Currently viewed/edited in UI
  int m_deviceActiveProfileIndex = -1; // Truly active on device
  int m_totalProfiles = 0;

  void onProfileTabChanged(int index);
  void saveUiToCache(int index);
  void updateTabLabels();
};

} // namespace deskflow::gui
