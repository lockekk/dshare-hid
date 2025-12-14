#include "BridgeClientConfigDialog.h"

#include "common/Settings.h"
#include "gui/core/BridgeClientConfigManager.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QVBoxLayout>

namespace {
constexpr auto kLandscapeIconPath = ":/bridge-client/client/orientation_landspace.png";
constexpr auto kPortraitIconPath = ":/bridge-client/client/orientation_portrait.png";
} // namespace
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

using namespace deskflow::gui;

BridgeClientConfigDialog::BridgeClientConfigDialog(const QString &configPath, QWidget *parent)
    : QDialog(parent),
      m_configPath(configPath)
{
  setWindowTitle(tr("Bridge Client Configuration"));
  setMinimumWidth(400);

  auto *mainLayout = new QVBoxLayout(this);
  auto *formLayout = new QFormLayout();

  // Screen Name
  m_editScreenName = new QLineEdit(this);
  formLayout->addRow(tr("Screen Name:"), m_editScreenName);

  m_editDeviceName = new QLineEdit(this);
  m_editDeviceName->setMaxLength(22);
  m_editDeviceName->setValidator(new QRegularExpressionValidator(
      QRegularExpression(QStringLiteral("^[A-Za-z0-9 _\\-\\.]{0,22}$")), m_editDeviceName
  ));
  m_editDeviceName->setPlaceholderText(tr("A-Z, 0-9, spaces, .-_ (max 22 chars)"));
  formLayout->addRow(tr("Firmware Device Name:"), m_editDeviceName);

  // Screen Resolution (width x height on one line)
  auto *resolutionLayout = new QHBoxLayout();
  resolutionLayout->setContentsMargins(0, 0, 0, 0);
  resolutionLayout->setSpacing(8);

  m_spinWidth = new QSpinBox(this);
  m_spinWidth->setMinimum(640);
  m_spinWidth->setMaximum(7680);
  m_spinWidth->setSingleStep(1);
  resolutionLayout->addWidget(m_spinWidth);

  resolutionLayout->addWidget(new QLabel(tr("x"), this));

  m_spinHeight = new QSpinBox(this);
  m_spinHeight->setMinimum(480);
  m_spinHeight->setMaximum(4320);
  m_spinHeight->setSingleStep(1);
  resolutionLayout->addWidget(m_spinHeight);

  resolutionLayout->addStretch();
  formLayout->addRow(tr("Screen resolution:"), resolutionLayout);

  // Screen Orientation
  auto *orientationLayout = new QHBoxLayout();
  m_orientationGroup = new QButtonGroup(this);
  m_radioLandscape = new QRadioButton(this);
  m_radioLandscape->setIcon(QIcon(QString::fromLatin1(kLandscapeIconPath)));
  m_radioLandscape->setIconSize(QSize(36, 24));
  m_radioLandscape->setToolTip(tr("Landscape"));
  m_radioPortrait = new QRadioButton(this);
  m_radioPortrait->setIcon(QIcon(QString::fromLatin1(kPortraitIconPath)));
  m_radioPortrait->setIconSize(QSize(36, 24));
  m_radioPortrait->setToolTip(tr("Portrait"));
  m_orientationGroup->addButton(m_radioLandscape);
  m_orientationGroup->addButton(m_radioPortrait);
  orientationLayout->addWidget(m_radioLandscape);
  orientationLayout->addWidget(m_radioPortrait);
  formLayout->addRow(tr("Screen Orientation:"), orientationLayout);

  auto *scrollLayout = new QHBoxLayout();
  scrollLayout->setContentsMargins(0, 0, 0, 0);
  scrollLayout->setSpacing(8);
  m_checkInvertScroll = new QCheckBox(tr("Invert Direction"), this);
  scrollLayout->addWidget(m_checkInvertScroll);
  scrollLayout->addWidget(new QLabel(tr("Speed"), this));

  m_spinScrollSpeed = new QSpinBox(this);
  m_spinScrollSpeed->setMinimum(1);
  m_spinScrollSpeed->setMaximum(960);
  m_spinScrollSpeed->setSingleStep(1);
  scrollLayout->addWidget(m_spinScrollSpeed);
  scrollLayout->addStretch();
  formLayout->addRow(tr("Scroll:"), scrollLayout);

  // Bluetooth Keep-Alive
  m_checkBluetoothKeepAlive = new QCheckBox(tr("Bluetooth connection follow client"), this);
  m_checkBluetoothKeepAlive->setToolTip(tr("Send keep-alive commands to maintain Bluetooth connection"));
  formLayout->addRow(QString(), m_checkBluetoothKeepAlive);

  mainLayout->addLayout(formLayout);

  // Dialog buttons
  auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttonBox, &QDialogButtonBox::accepted, this, &BridgeClientConfigDialog::onAccepted);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  mainLayout->addWidget(buttonBox);
  /*
   * Deskflow-hid -- created by locke.huang@gmail.com
   */
  // Load current config
  loadConfig();
}

void BridgeClientConfigDialog::loadConfig()
{
  QSettings config(m_configPath, QSettings::IniFormat);

  // Load screen name
  QString screenName = config.value(Settings::Core::ScreenName).toString();
  m_editScreenName->setText(screenName);
  m_originalScreenName = screenName;

  QString deviceName =
      config.value(Settings::Bridge::DeviceName, Settings::defaultValue(Settings::Bridge::DeviceName)).toString();
  m_editDeviceName->setText(deviceName);
  m_originalDeviceName = deviceName;

  // Load screen dimensions
  int width =
      config.value(Settings::Bridge::ScreenWidth, Settings::defaultValue(Settings::Bridge::ScreenWidth)).toInt();
  int height =
      config.value(Settings::Bridge::ScreenHeight, Settings::defaultValue(Settings::Bridge::ScreenHeight)).toInt();
  m_spinWidth->setValue(width);
  m_spinHeight->setValue(height);

  int scrollSpeed =
      config.value(Settings::Client::ScrollSpeed, Settings::defaultValue(Settings::Client::ScrollSpeed)).toInt();
  m_spinScrollSpeed->setValue(scrollSpeed);

  bool invertScroll =
      config
          .value(
              Settings::Client::InvertScrollDirection, Settings::defaultValue(Settings::Client::InvertScrollDirection)
          )
          .toBool();
  m_checkInvertScroll->setChecked(invertScroll);

  bool bluetoothKeepAlive =
      config.value(Settings::Bridge::BluetoothKeepAlive, Settings::defaultValue(Settings::Bridge::BluetoothKeepAlive))
          .toBool();
  m_checkBluetoothKeepAlive->setChecked(bluetoothKeepAlive);

  // Load screen orientation
  QString orientation =
      config.value(Settings::Bridge::ScreenOrientation, Settings::defaultValue(Settings::Bridge::ScreenOrientation))
          .toString();
  if (orientation == QStringLiteral("landscape")) {
    m_radioLandscape->setChecked(true);
  } else {
    m_radioPortrait->setChecked(true);
  }
}

void BridgeClientConfigDialog::saveConfig()
{
  QSettings config(m_configPath, QSettings::IniFormat);

  config.setValue(Settings::Core::ScreenName, m_editScreenName->text());
  config.setValue(Settings::Bridge::ScreenWidth, m_spinWidth->value());
  config.setValue(Settings::Bridge::ScreenHeight, m_spinHeight->value());
  config.setValue(Settings::Bridge::ScreenOrientation, screenOrientation());
  config.setValue(Settings::Bridge::DeviceName, deviceName());
  config.setValue(Settings::Client::ScrollSpeed, m_spinScrollSpeed->value());
  config.setValue(Settings::Client::InvertScrollDirection, m_checkInvertScroll->isChecked());
  config.setValue(Settings::Bridge::BluetoothKeepAlive, m_checkBluetoothKeepAlive->isChecked());

  config.sync();
}

QString BridgeClientConfigDialog::renameConfigFile(const QString &newScreenName)
{
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
          this, tr("Rename Failed"),
          tr("Failed to rename config file from '%1' to '%2'").arg(m_configPath).arg(newFilePath)
      );
      return m_configPath; // Return old path on failure
    }
    return newFilePath;
  }

  return m_configPath; // No rename needed
}

void BridgeClientConfigDialog::onAccepted()
{
  QString oldConfigPath = m_configPath;
  QString newScreenName = m_editScreenName->text().trimmed();
  const QString newDeviceName = deviceName();

  // Validate screen name
  if (newScreenName.isEmpty()) {
    QMessageBox::warning(this, tr("Invalid Input"), tr("Screen name cannot be empty."));
    return;
  }

  if (newDeviceName.isEmpty()) {
    QMessageBox::warning(this, tr("Invalid Input"), tr("Device name cannot be empty."));
    return;
  }
  if (!m_editDeviceName->hasAcceptableInput()) {
    QMessageBox::warning(
        this, tr("Invalid Input"),
        tr("Device name may only use English letters, numbers, spaces, '.', '-' or '_' and must be 22 characters or "
           "fewer.")
    );
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

QString BridgeClientConfigDialog::screenName() const
{
  return m_editScreenName->text();
}

int BridgeClientConfigDialog::screenWidth() const
{
  return m_spinWidth->value();
}

int BridgeClientConfigDialog::screenHeight() const
{
  return m_spinHeight->value();
}

QString BridgeClientConfigDialog::screenOrientation() const
{
  return m_radioPortrait->isChecked() ? QStringLiteral("portrait") : QStringLiteral("landscape");
}

int BridgeClientConfigDialog::scrollSpeed() const
{
  return m_spinScrollSpeed->value();
}

bool BridgeClientConfigDialog::invertScroll() const
{
  return m_checkInvertScroll->isChecked();
}

QString BridgeClientConfigDialog::deviceName() const
{
  return m_editDeviceName->text().trimmed();
}

bool BridgeClientConfigDialog::deviceNameChanged() const
{
  return deviceName() != m_originalDeviceName;
}

bool BridgeClientConfigDialog::bluetoothKeepAlive() const
{
  return m_checkBluetoothKeepAlive->isChecked();
}
