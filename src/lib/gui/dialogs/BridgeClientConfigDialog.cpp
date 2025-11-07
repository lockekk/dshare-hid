#include "BridgeClientConfigDialog.h"

#include "common/Settings.h"
#include "gui/core/BridgeClientConfigManager.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QSettings>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QRegularExpression>

using namespace deskflow::gui;

BridgeClientConfigDialog::BridgeClientConfigDialog(const QString &configPath, QWidget *parent)
    : QDialog(parent), m_configPath(configPath) {
  setWindowTitle(tr("Bridge Client Configuration"));
  setMinimumWidth(400);

  auto *mainLayout = new QVBoxLayout(this);
  auto *formLayout = new QFormLayout();

  // Screen Name
  m_editScreenName = new QLineEdit(this);
  formLayout->addRow(tr("Screen Name:"), m_editScreenName);

  // Screen Width
  m_spinWidth = new QSpinBox(this);
  m_spinWidth->setMinimum(640);
  m_spinWidth->setMaximum(7680);
  m_spinWidth->setSingleStep(1);
  formLayout->addRow(tr("Screen Width:"), m_spinWidth);

  // Screen Height
  m_spinHeight = new QSpinBox(this);
  m_spinHeight->setMinimum(480);
  m_spinHeight->setMaximum(4320);
  m_spinHeight->setSingleStep(1);
  formLayout->addRow(tr("Screen Height:"), m_spinHeight);

  // Screen Orientation
  m_comboOrientation = new QComboBox(this);
  m_comboOrientation->addItem(tr("Landscape"), "landscape");
  m_comboOrientation->addItem(tr("Portrait"), "portrait");
  formLayout->addRow(tr("Screen Orientation:"), m_comboOrientation);

  mainLayout->addLayout(formLayout);

  // Dialog buttons
  auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttonBox, &QDialogButtonBox::accepted, this, &BridgeClientConfigDialog::onAccepted);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  mainLayout->addWidget(buttonBox);

  // Load current config
  loadConfig();
}

void BridgeClientConfigDialog::loadConfig() {
  QSettings config(m_configPath, QSettings::IniFormat);

  // Load screen name
  QString screenName = config.value(Settings::Core::ScreenName).toString();
  m_editScreenName->setText(screenName);
  m_originalScreenName = screenName;

  // Load screen dimensions
  int width = config
                 .value(
                     Settings::Bridge::ScreenWidth,
                     Settings::defaultValue(Settings::Bridge::ScreenWidth))
                 .toInt();
  int height = config
                  .value(
                      Settings::Bridge::ScreenHeight,
                      Settings::defaultValue(Settings::Bridge::ScreenHeight))
                  .toInt();
  m_spinWidth->setValue(width);
  m_spinHeight->setValue(height);

  // Load screen orientation
  QString orientation = config
                            .value(
                                Settings::Bridge::ScreenOrientation,
                                Settings::defaultValue(Settings::Bridge::ScreenOrientation))
                            .toString();
  int index = m_comboOrientation->findData(orientation);
  if (index >= 0) {
    m_comboOrientation->setCurrentIndex(index);
  }
}

void BridgeClientConfigDialog::saveConfig() {
  QSettings config(m_configPath, QSettings::IniFormat);

  config.setValue(Settings::Core::ScreenName, m_editScreenName->text());
  config.setValue(Settings::Bridge::ScreenWidth, m_spinWidth->value());
  config.setValue(Settings::Bridge::ScreenHeight, m_spinHeight->value());
  config.setValue(Settings::Bridge::ScreenOrientation, m_comboOrientation->currentData().toString());

  config.sync();
}

QString BridgeClientConfigDialog::renameConfigFile(const QString &newScreenName) {
  QString dir = BridgeClientConfigManager::bridgeClientsDir();
  QFileInfo oldFileInfo(m_configPath);

  // Generate new filename based on screen name
  QString baseFileName = newScreenName;
  // Sanitize filename - replace invalid characters
  QRegularExpression invalidChars("[^a-zA-Z0-9_-]");
  baseFileName.replace(invalidChars, "_");

  QString newFileName = baseFileName + ".conf";
  QString newFilePath = QDir(dir).absoluteFilePath(newFileName);

  // Check if file already exists (and it's not the same file)
  if (QFile::exists(newFilePath) && oldFileInfo.absoluteFilePath() != newFilePath) {
    // Find unique name by appending number
    int suffix = 1;
    while (QFile::exists(newFilePath)) {
      newFileName = QString("%1_%2.conf").arg(baseFileName).arg(suffix);
      newFilePath = QDir(dir).absoluteFilePath(newFileName);
      suffix++;
    }
  }

  // Rename the file if the name changed
  if (oldFileInfo.absoluteFilePath() != newFilePath) {
    if (!QFile::rename(m_configPath, newFilePath)) {
      QMessageBox::warning(
          this, tr("Rename Failed"), tr("Failed to rename config file from '%1' to '%2'")
                                         .arg(m_configPath)
                                         .arg(newFilePath)
      );
      return m_configPath; // Return old path on failure
    }
    return newFilePath;
  }

  return m_configPath; // No rename needed
}

void BridgeClientConfigDialog::onAccepted() {
  QString oldConfigPath = m_configPath;
  QString newScreenName = m_editScreenName->text().trimmed();

  // Validate screen name
  if (newScreenName.isEmpty()) {
    QMessageBox::warning(this, tr("Invalid Input"), tr("Screen name cannot be empty."));
    return;
  }

  // Save config first
  saveConfig();

  // Rename config file if screen name changed
  if (newScreenName != m_originalScreenName) {
    QString newConfigPath = renameConfigFile(newScreenName);
    m_configPath = newConfigPath;
    Q_EMIT configChanged(oldConfigPath, newConfigPath);
  }

  accept();
}

QString BridgeClientConfigDialog::screenName() const {
  return m_editScreenName->text();
}

int BridgeClientConfigDialog::screenWidth() const {
  return m_spinWidth->value();
}

int BridgeClientConfigDialog::screenHeight() const {
  return m_spinHeight->value();
}

QString BridgeClientConfigDialog::screenOrientation() const {
  return m_comboOrientation->currentData().toString();
}
