#pragma once

#include <QButtonGroup>
#include <QCheckBox>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>

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

Q_SIGNALS:
  void configChanged(const QString &oldConfigPath, const QString &newConfigPath);

private Q_SLOTS:
  void onAccepted();
  void onUnpairAllClicked();

private:
  void loadConfig();
  void saveConfig();
  QString renameConfigFile(const QString &newScreenName);

  QString m_configPath;
  QString m_devicePath;
  QString m_originalScreenName;

  QLineEdit *m_editScreenName;
  QLineEdit *m_editDeviceName;
  QSpinBox *m_spinWidth;
  QSpinBox *m_spinHeight;
  QSpinBox *m_spinScrollSpeed;
  QCheckBox *m_checkInvertScroll;
  QCheckBox *m_checkBluetoothKeepAlive;
  QButtonGroup *m_orientationGroup = nullptr;
  QRadioButton *m_radioLandscape = nullptr;
  QRadioButton *m_radioPortrait = nullptr;
  QString m_originalDeviceName;
};

} // namespace deskflow::gui
