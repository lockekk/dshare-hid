#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QButtonGroup>
#include <QRadioButton>
#include <QPushButton>

namespace deskflow::gui {

class BridgeClientConfigDialog : public QDialog {
  Q_OBJECT

public:
  explicit BridgeClientConfigDialog(const QString &configPath, QWidget *parent = nullptr);

  QString screenName() const;
  int screenWidth() const;
  int screenHeight() const;
  QString screenOrientation() const;
  QString configPath() const { return m_configPath; }

Q_SIGNALS:
  void configChanged(const QString &oldConfigPath, const QString &newConfigPath);

private Q_SLOTS:
  void onAccepted();

private:
  void loadConfig();
  void saveConfig();
  QString renameConfigFile(const QString &newScreenName);

  QString m_configPath;
  QString m_originalScreenName;

  QLineEdit *m_editScreenName;
  QSpinBox *m_spinWidth;
  QSpinBox *m_spinHeight;
  QButtonGroup *m_orientationGroup = nullptr;
  QRadioButton *m_radioLandscape = nullptr;
  QRadioButton *m_radioPortrait = nullptr;
}; 

} // namespace deskflow::gui
