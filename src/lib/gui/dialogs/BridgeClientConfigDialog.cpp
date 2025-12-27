/*
 * Deskflow-hid -- created by locke.huang@gmail.com
 */

#include "BridgeClientConfigDialog.h"

#include "common/Settings.h"
#include "gui/core/BridgeClientConfigManager.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSettings>
#include <QTabBar>
#include <QTimer>
#include <QVBoxLayout>

#include "platform/bridge/CdcTransport.h"

namespace {
constexpr auto kLandscapeIconPath = ":/bridge-client/client/orientation_landscape.svg";
constexpr auto kPortraitIconPath = ":/bridge-client/client/orientation_portrait.svg";
} // namespace

using namespace deskflow::gui;

BridgeClientConfigDialog::BridgeClientConfigDialog(
    const QString &configPath, const QString &devicePath, QWidget *parent
)
    : QDialog(parent),
      m_configPath(configPath),
      m_devicePath(devicePath)
{
  setWindowTitle(tr("Bridge Client Configuration"));
  setMinimumWidth(600);

  auto *mainLayout = new QVBoxLayout(this);
  auto *topLayout = new QGridLayout();
  // topLayout->setContentsMargins(0, 0, 0, 0); // Optional: Match form layout defaults if needed

  // Screen Name
  auto *lblScreenName = new QLabel(tr("Screen Name:"), this);
  m_editScreenName = new QLineEdit(this);
  m_editScreenName->setMinimumHeight(30);

  topLayout->addWidget(lblScreenName, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);
  topLayout->addWidget(m_editScreenName, 0, 1);

  // Device Name
  auto *lblDeviceName = new QLabel(tr("Device name:"), this);
  m_editDeviceName = new QLineEdit(this);
  m_editDeviceName->setMaxLength(22);
  m_editDeviceName->setValidator(new QRegularExpressionValidator(
      QRegularExpression(QStringLiteral("^[A-Za-z0-9 _\\-\\.]{0,22}$")), m_editDeviceName
  ));
  m_editDeviceName->setPlaceholderText(tr("A-Z, 0-9, spaces, .-_ (max 22 chars)"));

  topLayout->addWidget(lblDeviceName, 1, 0, Qt::AlignLeft | Qt::AlignVCenter);
  topLayout->addWidget(m_editDeviceName, 1, 1);

  topLayout->setColumnStretch(1, 1); // Ensure fields expand

  mainLayout->addLayout(topLayout);

  // Profile Configuration Group (User requested this above Advanced)
  setupProfileUI(mainLayout);

  // Advanced Options Group
  auto *advancedGroup = new QGroupBox(tr("Advanced"), this);
  auto *advancedLayout = new QVBoxLayout(advancedGroup);

  // Bluetooth Keep-Alive
  m_checkBluetoothKeepAlive = new QCheckBox(tr("Automatically disconnect Bluetooth"), this);
  m_checkBluetoothKeepAlive->setToolTip(tr("Disconnects from the host device when you stop using Deskflow."));
  advancedLayout->addWidget(m_checkBluetoothKeepAlive);

  // Auto-connect
  m_checkAutoConnect = new QCheckBox(tr("Auto-connect when USB device plugged in"), this);
  m_checkAutoConnect->setToolTip(tr("Automatically connect to the Deskflow server when the device is plugged in."));
  advancedLayout->addWidget(m_checkAutoConnect);

  mainLayout->addWidget(advancedGroup);

  // Dialog buttons
  auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttonBox, &QDialogButtonBox::accepted, this, &BridgeClientConfigDialog::onAccepted);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  mainLayout->addWidget(buttonBox);

  // Load current config
  loadConfig();

  // Prevent initial focus on inputs by focusing the dialog itself
  this->setFocus();
}

void BridgeClientConfigDialog::setupProfileUI(QVBoxLayout *mainLayout)
{
  m_profileGroup = new QGroupBox(tr("Profiles (Device)"), this);
  auto *groupLayout = new QVBoxLayout(m_profileGroup);

  // Tab Bar for Profile Selection
  m_profileTabBar = new QTabBar(this);
  m_profileTabBar->setShape(QTabBar::RoundedNorth);
  m_profileTabBar->setDrawBase(false); // We don't have a widget below it directly attached style-wise
  connect(m_profileTabBar, &QTabBar::currentChanged, this, &BridgeClientConfigDialog::onProfileTabChanged);

  groupLayout->addWidget(m_profileTabBar);

  // Profile Details Form
  auto *detailsLayout = new QFormLayout();
  detailsLayout->setContentsMargins(10, 15, 10, 15);
  detailsLayout->setVerticalSpacing(14);
  detailsLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  // QFormLayout manages columns automatically

  int row = 0; // Not strictly needed for QFormLayout addRow, but keeping if logic relied on it (it didn't really)

  // Helper for consistent zero-margin HBoxes
  auto createHBox = []() {
    auto *l = new QHBoxLayout();
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(10); // Standard spacing between items
    return l;
  };

  // Helper to add complex rows as a container widget
  auto addComplexRow = [&](const QString &label, QLayout *layout) {
    auto *container = new QWidget(this);
    container->setLayout(layout);
    detailsLayout->addRow(label, container);
  };

  // Hostname
  {
    auto *l = createHBox();
    m_editProfileName = new QLineEdit(this);
    m_editProfileName->setMaxLength(31); // 32 chars including null
    m_editProfileName->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^[A-Za-z0-9 _\\-\\.]{0,31}$")), m_editProfileName
    ));
    m_editProfileName->setPlaceholderText(tr("A-Z, 0-9, spaces, .-_ (max 31 chars)"));
    m_editProfileName->setMinimumHeight(30);
    m_editProfileName->setMinimumWidth(400);
    l->addWidget(m_editProfileName);
    l->addStretch();

    addComplexRow(tr("Hostname:"), l);

    connect(m_editProfileName, &QLineEdit::textChanged, this, [this](const QString &text) {
      if (m_selectedProfileIndex >= 0 && m_profileCache.contains(m_selectedProfileIndex)) {
        auto &p = m_profileCache[m_selectedProfileIndex];
        std::string s = text.toStdString();
        std::strncpy(p.hostname, s.c_str(), sizeof(p.hostname));
        if (sizeof(p.hostname) > 0)
          p.hostname[sizeof(p.hostname) - 1] = '\0';
        updateTabLabels();
      }
    });
  }

  // Resolution
  auto *resLayout = createHBox();

  // Container for Width + 'x' to match 180px column alignment
  auto *resContainer = new QWidget(this);
  resContainer->setMinimumWidth(180);
  {
    auto *l = createHBox();
    m_spinProfileWidth = new QSpinBox(this);
    m_spinProfileWidth->setRange(100, 10000);
    m_spinProfileWidth->setMinimumWidth(100);
    l->addWidget(m_spinProfileWidth);

    l->addStretch();
    l->addWidget(new QLabel("x"));
    l->addStretch();

    resContainer->setLayout(l);
  }

  m_spinProfileHeight = new QSpinBox(this);
  m_spinProfileHeight->setRange(100, 10000);
  m_spinProfileHeight->setMinimumWidth(100);

  resLayout->addWidget(resContainer);
  resLayout->addSpacing(10);
  resLayout->addWidget(m_spinProfileHeight);
  resLayout->addStretch();

  addComplexRow(tr("Resolution:"), resLayout);

  // Orientation
  m_profileOrientationGroup = new QButtonGroup(this);
  m_radioProfileLandscape = new QRadioButton(this);
  m_radioProfileLandscape->setIcon(QIcon(kLandscapeIconPath));
  m_radioProfileLandscape->setIconSize(QSize(32, 32));
  m_radioProfileLandscape->setToolTip(tr("Landscape"));
  m_radioProfileLandscape->setMinimumHeight(32);

  m_radioProfilePortrait = new QRadioButton(this);
  m_radioProfilePortrait->setIcon(QIcon(kPortraitIconPath));
  m_radioProfilePortrait->setIconSize(QSize(32, 32));
  m_radioProfilePortrait->setToolTip(tr("Portrait"));
  m_radioProfilePortrait->setMinimumHeight(32);
  m_radioProfilePortrait->setMinimumWidth(180); // Fix alignment for 2nd column

  m_profileOrientationGroup->addButton(m_radioProfilePortrait, 0);
  m_profileOrientationGroup->addButton(m_radioProfileLandscape, 1);

  {
    auto *l = createHBox();
    l->addWidget(m_radioProfilePortrait);
    l->addSpacing(10);
    l->addWidget(m_radioProfileLandscape);
    l->addStretch();
    addComplexRow(tr("Orientation:"), l);
  }

  // HID Mode
  m_profileHidModeGroup = new QButtonGroup(this);
  m_radioProfileCombo = new QRadioButton(tr("Keyboard + Mouse"), this);
  m_radioProfileCombo->setMinimumWidth(180); // Fix alignment for 2nd column

  m_radioProfileMouseOnly = new QRadioButton(tr("Mouse Only"), this);

  m_profileHidModeGroup->addButton(m_radioProfileCombo, 0);
  m_profileHidModeGroup->addButton(m_radioProfileMouseOnly, 1);

  {
    auto *l = createHBox();
    l->addWidget(m_radioProfileCombo);
    l->addSpacing(10);
    l->addWidget(m_radioProfileMouseOnly);
    l->addStretch();
    addComplexRow(tr("HID Mode:"), l);
  }

  // Scroll Settings
  auto *scrollLayout = createHBox();

  // Column 1: SpinBox wrapped to taking 180px to match column alignment
  auto *spinContainer = new QWidget(this);
  spinContainer->setMinimumWidth(180);
  {
    auto *spinLayout = createHBox();
    m_spinScrollSpeed = new QSpinBox(this);
    m_spinScrollSpeed->setRange(0, 255);
    m_spinScrollSpeed->setSingleStep(1);
    spinLayout->addWidget(m_spinScrollSpeed);
    spinLayout->addStretch();
    spinContainer->setLayout(spinLayout);
  }
  scrollLayout->addWidget(spinContainer);
  scrollLayout->addSpacing(10);

  m_checkInvertScroll = new QCheckBox(tr("Invert direction"), this);
  m_checkInvertScroll->setMinimumWidth(150);
  scrollLayout->addWidget(m_checkInvertScroll);
  scrollLayout->addStretch();

  addComplexRow(tr("Scroll speed:"), scrollLayout);

  groupLayout->addLayout(detailsLayout);

  // Buttons
  auto *btnLayout = new QHBoxLayout();
  m_btnProfileReset = new QPushButton(tr("Reset"), this);
  m_btnProfileSave = new QPushButton(tr("Save to Device"), this); // Clarified label
  m_btnProfileActivate = new QPushButton(tr("Activate"), this);

  connect(m_btnProfileReset, &QPushButton::clicked, this, &BridgeClientConfigDialog::onProfileResetClicked);
  connect(m_btnProfileSave, &QPushButton::clicked, this, &BridgeClientConfigDialog::onProfileSaveClicked);
  connect(m_btnProfileActivate, &QPushButton::clicked, this, &BridgeClientConfigDialog::onProfileActivateClicked);

  btnLayout->addWidget(m_btnProfileReset);
  btnLayout->addStretch();
  btnLayout->addWidget(m_btnProfileSave);
  btnLayout->addWidget(m_btnProfileActivate);
  groupLayout->addLayout(btnLayout);

  mainLayout->addWidget(m_profileGroup);

  // Initial state disabled until loaded
  m_profileGroup->setEnabled(false);
  m_selectedProfileIndex = -1;
  m_deviceActiveProfileIndex = -1;
}

void BridgeClientConfigDialog::loadConfig()
{
  // Same implementation as before, keeping file structure clean
  QSettings config(m_configPath, QSettings::IniFormat);

  QString screenName = config.value(Settings::Core::ScreenName).toString();
  m_editScreenName->setText(screenName);
  m_originalScreenName = screenName;

  QString deviceName =
      config.value(Settings::Bridge::DeviceName, Settings::defaultValue(Settings::Bridge::DeviceName)).toString();
  m_editDeviceName->setText(deviceName);
  m_originalDeviceName = deviceName;

  bool btKeepAlive =
      config.value(Settings::Bridge::BluetoothKeepAlive, Settings::defaultValue(Settings::Bridge::BluetoothKeepAlive))
          .toBool();
  m_checkBluetoothKeepAlive->setChecked(btKeepAlive);

  bool autoConnect =
      config.value(Settings::Bridge::AutoConnect, Settings::defaultValue(Settings::Bridge::AutoConnect)).toBool();
  m_checkAutoConnect->setChecked(autoConnect);

  QString activationState =
      config.value(Settings::Bridge::ActivationState, Settings::defaultValue(Settings::Bridge::ActivationState))
          .toString();
  if (activationState.compare("activated", Qt::CaseInsensitive) != 0) {
    m_editDeviceName->setEnabled(false);
    m_editDeviceName->setToolTip(tr("Device must be activated to change its name."));
  } else {
    m_editDeviceName->setEnabled(true);
    m_editDeviceName->setToolTip(QString());
  }

  // Load profiles from device
  QTimer::singleShot(0, this, &BridgeClientConfigDialog::loadProfilesFromDevice);
}

void BridgeClientConfigDialog::loadProfilesFromDevice()
{
  if (m_devicePath.isEmpty()) {
    m_profileGroup->setTitle(tr("Profiles (Device Not Connected)"));
    m_profileGroup->setEnabled(false);
    return;
  }

  deskflow::bridge::CdcTransport transport(m_devicePath);
  if (!transport.open()) {
    m_profileGroup->setTitle(tr("Profiles (Failed to Open Device)"));
    m_profileGroup->setEnabled(false);
    return;
  }

  m_totalProfiles = 0;
  m_deviceActiveProfileIndex = -1;

  if (transport.hasDeviceConfig()) {
    m_totalProfiles = transport.deviceConfig().totalProfiles;
    m_deviceActiveProfileIndex = transport.deviceConfig().activeProfile;
  }

  // Safety fallback
  if (m_totalProfiles == 0)
    m_totalProfiles = 6;
  if (m_deviceActiveProfileIndex < 0)
    m_deviceActiveProfileIndex = 0; // Default fallback

  // Populate Tabs
  while (m_profileTabBar->count() > 0)
    m_profileTabBar->removeTab(0);

  for (int i = 0; i < m_totalProfiles; ++i) {
    m_profileTabBar->addTab(QString("Slot %1").arg(i));
  }

  // Fetch profiles
  for (int i = 0; i < m_totalProfiles; ++i) {
    deskflow::bridge::DeviceProfile profile;
    if (transport.getProfile(i, profile)) {
      m_profileCache[i] = profile;
    } else {
      std::memset(&profile, 0, sizeof(profile));
      m_profileCache[i] = profile;
    }
  }

  updateTabLabels();

  m_profileGroup->setEnabled(true);

  // Select the active profile tab by default
  {
    const QSignalBlocker blocker(m_profileTabBar);
    m_profileTabBar->setCurrentIndex(m_deviceActiveProfileIndex);
  }
  m_selectedProfileIndex = m_deviceActiveProfileIndex;
  updateProfileDetailUI(m_selectedProfileIndex);
}

void BridgeClientConfigDialog::onProfileTabChanged(int index)
{
  // Need to save the PREVIOUS tab's state to cache?
  // QTabBar doesn't give us the "previous" index easily in the signal,
  // but we have m_selectedProfileIndex which holds the OLD index before we update it.

  if (m_selectedProfileIndex >= 0 && m_selectedProfileIndex < m_totalProfiles) {
    saveUiToCache(m_selectedProfileIndex);
  }

  m_selectedProfileIndex = index;
  updateProfileDetailUI(index);
}

void BridgeClientConfigDialog::saveUiToCache(int index)
{
  if (!m_profileCache.contains(index))
    return;

  deskflow::bridge::DeviceProfile &p = m_profileCache[index];

  std::string hostname = m_editProfileName->text().toStdString();
  if (hostname.length() >= sizeof(p.hostname)) {
    hostname = hostname.substr(0, sizeof(p.hostname) - 1);
  }
  std::strncpy(p.hostname, hostname.c_str(), sizeof(p.hostname));
  p.screenWidth = static_cast<uint16_t>(m_spinProfileWidth->value());
  p.screenHeight = static_cast<uint16_t>(m_spinProfileHeight->value());
  p.rotation = m_radioProfilePortrait->isChecked() ? 0 : 1;
  p.hidMode = m_radioProfileMouseOnly->isChecked() ? 1 : 0;
  p.slot = index;
  p.invert = m_checkInvertScroll->isChecked() ? 1 : 0;
  p.speed = static_cast<uint8_t>(m_spinScrollSpeed->value());
}

void BridgeClientConfigDialog::updateTabLabels()
{
  for (int i = 0; i < m_profileTabBar->count(); ++i) {
    QString label;
    if (m_profileCache.contains(i)) {
      const auto &p = m_profileCache[i];
      QByteArray nameBytes(p.hostname, sizeof(p.hostname));
      int nullPos = nameBytes.indexOf('\0');
      if (nullPos >= 0)
        nameBytes.truncate(nullPos);
      label = QString::fromUtf8(nameBytes);
    }

    if (label.isEmpty()) {
      label = QString("Slot %1").arg(i);
    }

    if (i == m_deviceActiveProfileIndex) {
      label += " (Active)";
    }
    m_profileTabBar->setTabText(i, label);
  }
}

void BridgeClientConfigDialog::updateProfileDetailUI(int index)
{
  if (!m_profileCache.contains(index))
    return;
  const auto &p = m_profileCache[index];

  // Use QByteArray to safely handle non-null-terminated strings
  QByteArray nameBytes(p.hostname, sizeof(p.hostname));
  int nullPos = nameBytes.indexOf('\0');
  if (nullPos >= 0)
    nameBytes.truncate(nullPos);

  // Block signals to prevent potential loops if we had them (not critical here but good practice)
  const bool blocked = blockSignals(true);

  m_editProfileName->setText(QString::fromUtf8(nameBytes));
  m_spinProfileWidth->setValue(p.screenWidth);
  m_spinProfileHeight->setValue(p.screenHeight);

  if (p.rotation == 0)
    m_radioProfilePortrait->setChecked(true);
  else
    m_radioProfileLandscape->setChecked(true);

  if (p.hidMode == 1)
    m_radioProfileMouseOnly->setChecked(true);
  else
    m_radioProfileCombo->setChecked(true);

  m_spinScrollSpeed->setValue(p.speed);
  m_checkInvertScroll->setChecked(p.invert != 0);

  blockSignals(blocked);
}

void BridgeClientConfigDialog::onProfileToggled(int id, bool checked)
{
  // This method is no longer used with QTabBar, but kept for compilation if still referenced elsewhere.
  // The logic is now handled by onProfileTabChanged.
}

void BridgeClientConfigDialog::onProfileSaveClicked()
{
  if (m_selectedProfileIndex < 0 || m_selectedProfileIndex >= m_totalProfiles)
    return;

  // Ensure current UI state is captured
  saveUiToCache(m_selectedProfileIndex);

  const auto &p = m_profileCache[m_selectedProfileIndex];

  // Send to Device
  deskflow::bridge::CdcTransport transport(m_devicePath);
  if (!transport.open()) {
    QMessageBox::critical(this, tr("Error"), tr("Failed to open device"));
    return;
  }

  if (transport.setProfile(m_selectedProfileIndex, p)) {
    QMessageBox::information(this, tr("Success"), tr("Profile saved to device."));
  } else {
    QMessageBox::critical(
        this, tr("Error"), tr("Failed to save profile: %1").arg(QString::fromStdString(transport.lastError()))
    );
  }
}

void BridgeClientConfigDialog::onProfileActivateClicked()
{
  if (m_selectedProfileIndex < 0)
    return;

  if (m_selectedProfileIndex == m_deviceActiveProfileIndex) {
    return;
  }

  deskflow::bridge::CdcTransport transport(m_devicePath);
  if (!transport.open()) {
    QMessageBox::critical(this, tr("Error"), tr("Failed to open device"));
    return;
  }

  if (transport.switchProfile(m_selectedProfileIndex)) {
    m_deviceActiveProfileIndex = m_selectedProfileIndex;
    updateTabLabels();

    QString profileName = QString("Slot %1").arg(m_selectedProfileIndex);
    if (m_profileCache.contains(m_selectedProfileIndex)) {
      const auto &p = m_profileCache[m_selectedProfileIndex];
      QByteArray nameBytes(p.hostname, sizeof(p.hostname));
      int nullPos = nameBytes.indexOf('\0');
      if (nullPos >= 0)
        nameBytes.truncate(nullPos);
      QString name = QString::fromUtf8(nameBytes);
      if (!name.isEmpty()) {
        profileName = name;
      }
    }
    QMessageBox::information(this, tr("Success"), tr("Switched to %1").arg(profileName));
  } else {
    QMessageBox::critical(
        this, tr("Error"), tr("Failed to switch profile: %1").arg(QString::fromStdString(transport.lastError()))
    );
  }
}

void BridgeClientConfigDialog::onProfileResetClicked()
{
  if (m_selectedProfileIndex < 0)
    return;

  if (QMessageBox::question(this, tr("Confirm Reset"), tr("Are you sure you want to reset and erase this profile?")) !=
      QMessageBox::Yes) {
    return;
  }

  deskflow::bridge::CdcTransport transport(m_devicePath);
  if (!transport.open()) {
    QMessageBox::critical(this, tr("Error"), tr("Failed to open device"));
    return;
  }

  if (transport.eraseProfile(m_selectedProfileIndex)) {
    QMessageBox::information(this, tr("Success"), tr("Profile erased."));

    // Refresh just this profile
    deskflow::bridge::DeviceProfile profile;
    if (transport.getProfile(m_selectedProfileIndex, profile)) {
      m_profileCache[m_selectedProfileIndex] = profile;
      updateProfileDetailUI(m_selectedProfileIndex);
    }

  } else {
    QMessageBox::critical(
        this, tr("Error"), tr("Failed to erase profile: %1").arg(QString::fromStdString(transport.lastError()))
    );
  }
}

void BridgeClientConfigDialog::saveConfig()
{
  QSettings config(m_configPath, QSettings::IniFormat);

  config.setValue(Settings::Core::ScreenName, m_editScreenName->text());
  // Removed global resolution/orientation
  config.setValue(Settings::Bridge::DeviceName, deviceName());

  // Remove keys that are now managed solely by the device profile
  config.remove(Settings::Client::ScrollSpeed);
  config.remove(Settings::Client::InvertScrollDirection);
  if (m_deviceActiveProfileIndex >= 0 && m_profileCache.contains(m_deviceActiveProfileIndex)) {
    const auto &p = m_profileCache[m_deviceActiveProfileIndex];

    QByteArray nameBytes(p.hostname, sizeof(p.hostname));
    int nullPos = nameBytes.indexOf('\0');
    if (nullPos >= 0)
      nameBytes.truncate(nullPos);
    config.setValue(Settings::Bridge::ActiveProfileHostname, QString::fromUtf8(nameBytes));

    QString orientation = (p.rotation == 0) ? QStringLiteral("portrait") : QStringLiteral("landscape");
    config.setValue(Settings::Bridge::ActiveProfileOrientation, orientation);
  }

  config.setValue(Settings::Bridge::BluetoothKeepAlive, m_checkBluetoothKeepAlive->isChecked());
  config.setValue(Settings::Bridge::AutoConnect, m_checkAutoConnect->isChecked());

  config.sync();
}

QString BridgeClientConfigDialog::renameConfigFile(const QString &newScreenName)
{
  QString dir = BridgeClientConfigManager::bridgeClientsDir();
  QFileInfo oldFileInfo(m_configPath);

  // Generate new filename based on screen name
  QString baseFileName = newScreenName;
  QRegularExpression invalidChars("[^a-zA-Z0-9_-]");
  baseFileName.replace(invalidChars, "_");

  QString newFileName = baseFileName + ".conf";
  QString newFilePath = QDir(dir).absoluteFilePath(newFileName);

  if (QFile::exists(newFilePath) && oldFileInfo.absoluteFilePath() != newFilePath) {
    int suffix = 1;
    while (QFile::exists(newFilePath)) {
      newFileName = QString("%1_%2.conf").arg(baseFileName).arg(suffix);
      newFilePath = QDir(dir).absoluteFilePath(newFileName);
      suffix++;
    }
  }

  if (oldFileInfo.absoluteFilePath() != newFilePath) {
    if (!QFile::rename(m_configPath, newFilePath)) {
      QMessageBox::warning(
          this, tr("Rename Failed"),
          tr("Failed to rename config file from '%1' to '%2'").arg(m_configPath).arg(newFilePath)
      );
      return m_configPath;
    }
    return newFilePath;
  }

  return m_configPath;
}

void BridgeClientConfigDialog::onAccepted()
{
  QString oldConfigPath = m_configPath;
  QString newScreenName = m_editScreenName->text().trimmed();
  const QString newDeviceName = deviceName();

  if (newScreenName.isEmpty()) {
    QMessageBox::warning(this, tr("Invalid Input"), tr("Screen name cannot be empty."));
    return;
  }

  if (newDeviceName.isEmpty()) {
    QMessageBox::warning(this, tr("Invalid Input"), tr("Device name cannot be empty."));
    return;
  }

  // Duplicate Check
  QFileInfo currentFileInfo(m_configPath);
  QString currentSn = BridgeClientConfigManager::readSerialNumber(m_configPath);
  if (currentSn.isEmpty() && !m_devicePath.isEmpty()) {
    deskflow::bridge::CdcTransport transport(m_devicePath);
    if (transport.open()) {
      std::string sn;
      if (transport.fetchSerialNumber(sn)) {
        currentSn = QString::fromStdString(sn);
      }
      transport.close();
    }
  }

  if (!currentSn.isEmpty()) {
    QStringList allConfigs = BridgeClientConfigManager::getAllConfigFiles();
    for (const auto &path : allConfigs) {
      if (QFileInfo(path).absoluteFilePath() == currentFileInfo.absoluteFilePath())
        continue; // Skip self

      if (BridgeClientConfigManager::readSerialNumber(path) == currentSn) {
        QMessageBox::critical(
            this, tr("Duplicate Configuration"),
            tr("A configuration for this device serial number already exists.\nDuplicate: %1").arg(path)
        );
        return; // Abort save
      }
    }
  }

  saveConfig();

  if (newScreenName != m_originalScreenName) {
    QString newConfigPath = renameConfigFile(newScreenName);
    m_configPath = newConfigPath;
    Q_EMIT configChanged(oldConfigPath, newConfigPath);
  }

  accept();
}

int BridgeClientConfigDialog::screenWidth() const
{
  // Return from ACTIVE DEVICE profile if available (not currently selected one)
  if (m_deviceActiveProfileIndex >= 0 && m_profileCache.contains(m_deviceActiveProfileIndex)) {
    return m_profileCache[m_deviceActiveProfileIndex].screenWidth;
  }
  return 1920;
}

int BridgeClientConfigDialog::screenHeight() const
{
  if (m_deviceActiveProfileIndex >= 0 && m_profileCache.contains(m_deviceActiveProfileIndex)) {
    return m_profileCache[m_deviceActiveProfileIndex].screenHeight;
  }
  return 1080;
}

QString BridgeClientConfigDialog::screenOrientation() const
{
  if (m_deviceActiveProfileIndex >= 0 && m_profileCache.contains(m_deviceActiveProfileIndex)) {
    return (m_profileCache[m_deviceActiveProfileIndex].rotation == 0) ? QStringLiteral("portrait")
                                                                      : QStringLiteral("landscape");
  }
  return QStringLiteral("landscape");
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

bool BridgeClientConfigDialog::autoConnect() const
{
  return m_checkAutoConnect->isChecked();
}

QString BridgeClientConfigDialog::screenName() const
{
  return m_editScreenName->text();
}

bool BridgeClientConfigDialog::invertScroll() const
{
  return m_checkInvertScroll->isChecked();
}
