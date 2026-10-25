/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#include "BridgeClientConfigDialog.h"

#include "common/Settings.h"
#include "gui/core/BridgeClientConfigManager.h"
#include "gui/core/BridgeClientProcess.h"
#include "gui/core/BridgeDeviceService.h"
#include "gui/core/BridgeProfileBackup.h"
#include "gui/devices/UsbDeviceHelper.h"

#include <QApplication>
#include <QDate>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTabBar>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cstring>

#include "gui/MainWindow.h"
#include "gui/config/ServerConfig.h"
#include "platform/bridge/CdcTransport.h"

using namespace deskflow::gui;

namespace {
constexpr auto kLandscapeIconPath = ":/bridge-client/client/orientation_landscape.svg";
constexpr auto kPortraitIconPath = ":/bridge-client/client/orientation_portrait.svg";

// the lines, one per bullet, to follow a message
QString bulletList(const QStringList &items)
{
  constexpr int kShown = 10;
  QStringList lines;
  for (qsizetype i = 0; i < items.size() && i < kShown; ++i) {
    lines.append(QStringLiteral("• ") + items.at(i));
  }
  if (items.size() > kShown) {
    lines.append(BridgeClientConfigDialog::tr("and %1 more").arg(items.size() - kShown));
  }
  return QStringLiteral("\n\n") + lines.join(QLatin1Char('\n'));
}

// select the file in Finder or Explorer; other desktops have no common way to, so open its folder
void showInFileManager(const QString &path)
{
  bool shown = false;
#if defined(Q_OS_MACOS)
  shown = QProcess::startDetached(QStringLiteral("open"), {QStringLiteral("-R"), path});
#elif defined(Q_OS_WIN)
  shown = QProcess::startDetached(
      QStringLiteral("explorer.exe"), {QStringLiteral("/select,"), QDir::toNativeSeparators(path)}
  );
#endif
  if (!shown) {
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
  }
}
// the profile's hostname, or "Slot N" for none, as its tab shows it
QString profileLabel(int index, const deskflow::bridge::DeviceProfile &profile)
{
  return BridgeProfileInfo{index, BridgeDeviceService::profileHostname(profile)}.label();
}
} // namespace

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
  m_editScreenName->setObjectName(QStringLiteral("screenName"));
  m_editScreenName->setMinimumHeight(30);
  // same rule as the computer name in MainWindow (m_nameRegEx)
  m_editScreenName->setValidator(
      new QRegularExpressionValidator(QRegularExpression(QStringLiteral("^[\\w\\-_\\.]{0,255}$")), m_editScreenName)
  );

  topLayout->addWidget(lblScreenName, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);
  topLayout->addWidget(m_editScreenName, 0, 1);

  // Device Name
  auto *lblDeviceName = new QLabel(tr("Device name:"), this);
  m_editDeviceName = new QLineEdit(this);
  m_editDeviceName->setObjectName(QStringLiteral("deviceName"));
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
  m_checkBluetoothKeepAlive->setToolTip(tr("Disconnects from the host device when you stop using DShare-HID."));
  advancedLayout->addWidget(m_checkBluetoothKeepAlive);

  // Auto-connect
  m_checkAutoConnect = new QCheckBox(tr("Auto-connect when USB device plugged in"), this);
  m_checkAutoConnect->setToolTip(tr("Automatically connect to the DShare-HID server when the device is plugged in."));

  advancedLayout->addWidget(m_checkAutoConnect);

  // Global Bonding Location (Moved from Profile Group)
  m_checkBondLocation = new QCheckBox(tr("Automatically restore screen location for the active profile"), this);
  m_checkBondLocation->setToolTip(
      tr("When enabled, the saved screen location for the active profile will be restored automatically.")
  );
  advancedLayout->addWidget(m_checkBondLocation);

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
  m_profileGroup = new QGroupBox(this);
  auto *groupLayout = new QVBoxLayout(m_profileGroup);

  // backup and restore act on all of the profiles, so they sit by the title rather than with
  // the buttons for the profile shown
  auto *titleLayout = new QHBoxLayout();
  m_lblProfileGroupTitle = new QLabel(tr("Profiles (Device)"), this);
  m_lblProfileGroupTitle->setObjectName(QStringLiteral("profileTitle"));
  m_btnProfileBackup = new QPushButton(tr("Backup..."), this);
  m_btnProfileBackup->setObjectName(QStringLiteral("profileBackup"));
  m_btnProfileBackup->setToolTip(tr("Save the device's profiles to a file."));
  m_btnProfileRestore = new QPushButton(tr("Restore..."), this);
  m_btnProfileRestore->setObjectName(QStringLiteral("profileRestore"));
  m_btnProfileRestore->setToolTip(tr("Load profiles from a backup file."));
  connect(m_btnProfileBackup, &QPushButton::clicked, this, &BridgeClientConfigDialog::onProfileBackupClicked);
  connect(m_btnProfileRestore, &QPushButton::clicked, this, &BridgeClientConfigDialog::onProfileRestoreClicked);

  titleLayout->addWidget(m_lblProfileGroupTitle);
  titleLayout->addStretch();
  titleLayout->addWidget(m_btnProfileBackup);
  titleLayout->addWidget(m_btnProfileRestore);
  groupLayout->addLayout(titleLayout);

  // Tab Bar for Profile Selection
  m_profileTabBar = new QTabBar(this);
  m_profileTabBar->setObjectName(QStringLiteral("profileTabs"));
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

  auto createHBox = []() {
    auto *l = new QHBoxLayout();
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(10); // Standard spacing between items
    return l;
  };

  auto addComplexRow = [&](const QString &label, QLayout *layout, int minHeight = 0) {
    auto *container = new QWidget(this);
    container->setLayout(layout);
    if (minHeight > 0) {
      container->setMinimumHeight(minHeight);
    }
    detailsLayout->addRow(label, container);
  };

  // Hostname
  {
    auto *l = createHBox();
    m_editProfileName = new QLineEdit(this);
    m_editProfileName->setObjectName(QStringLiteral("profileHostname"));
    m_editProfileName->setMaxLength(31); // 32 chars including null
    m_editProfileName->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^[A-Za-z0-9 _\\-\\.]{0,31}$")), m_editProfileName
    ));
    m_editProfileName->setPlaceholderText(tr("A-Z, 0-9, spaces, .-_ (max 31 chars)"));
    m_editProfileName->setMinimumHeight(30);
    m_editProfileName->setMinimumWidth(400);
    l->addWidget(m_editProfileName);
    l->addStretch();

    addComplexRow(tr("Hostname:"), l, 36);

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
    m_spinProfileWidth->setObjectName(QStringLiteral("profileWidth"));
    m_spinProfileWidth->setRange(100, 10000);
    m_spinProfileWidth->setMinimumWidth(100);
    l->addWidget(m_spinProfileWidth);

    l->addStretch();
    l->addWidget(new QLabel("x"));
    l->addStretch();

    resContainer->setLayout(l);
  }

  m_spinProfileHeight = new QSpinBox(this);
  m_spinProfileHeight->setObjectName(QStringLiteral("profileHeight"));
  m_spinProfileHeight->setRange(100, 10000);
  m_spinProfileHeight->setMinimumWidth(100);

  resLayout->addWidget(resContainer);
  resLayout->addSpacing(10);
  resLayout->addWidget(m_spinProfileHeight);
  resLayout->addStretch();

  addComplexRow(tr("Resolution:"), resLayout, 36);

  // Orientation
  m_profileOrientationGroup = new QButtonGroup(this);

  // Portrait (Left)
  m_radioProfilePortrait = new QRadioButton(this);
  m_radioProfilePortrait->setObjectName(QStringLiteral("profilePortrait"));
  m_radioProfilePortrait->setToolTip(tr("Portrait"));

  m_lblIconPortrait = new QLabel(this);
  m_lblIconPortrait->setPixmap(QIcon(kPortraitIconPath).pixmap(24, 24));
  m_lblIconPortrait->setFixedSize(24, 24);
  m_lblIconPortrait->setCursor(Qt::PointingHandCursor);
  m_lblIconPortrait->installEventFilter(this);

  // Landscape (Right)
  m_radioProfileLandscape = new QRadioButton(this);
  m_radioProfileLandscape->setObjectName(QStringLiteral("profileLandscape"));
  m_radioProfileLandscape->setToolTip(tr("Landscape"));

  m_lblIconLandscape = new QLabel(this);
  m_lblIconLandscape->setPixmap(QIcon(kLandscapeIconPath).pixmap(24, 24));
  m_lblIconLandscape->setFixedSize(24, 24);
  m_lblIconLandscape->setCursor(Qt::PointingHandCursor);
  m_lblIconLandscape->installEventFilter(this);

  m_profileOrientationGroup->addButton(m_radioProfilePortrait, 0);
  m_profileOrientationGroup->addButton(m_radioProfileLandscape, 1);

  {
    auto *l = createHBox();

    auto *pContainer = new QWidget(this);
    pContainer->setFixedWidth(180);
    auto *pl = createHBox();
    pl->addWidget(m_radioProfilePortrait);
    pl->addWidget(m_lblIconPortrait);
    pl->addStretch();
    pContainer->setLayout(pl);

    l->addWidget(pContainer);

    l->addSpacing(10);

    // Landscape Group (Right)
    l->addWidget(m_radioProfileLandscape);
    l->addWidget(m_lblIconLandscape);

    l->addStretch();

    auto *container = new QWidget(this);
    container->setLayout(l);
    container->setMinimumHeight(36); // Verify enough vertical space for the whole row

    detailsLayout->addRow(tr("Orientation:"), container);
  }

  // HID Mode
  m_profileHidModeGroup = new QButtonGroup(this);
  m_radioProfileCombo = new QRadioButton(tr("Keyboard + Mouse"), this);
  m_radioProfileCombo->setObjectName(QStringLiteral("profileCombo"));
  m_radioProfileCombo->setMinimumWidth(180); // Fix alignment for 2nd column

  m_radioProfileMouseOnly = new QRadioButton(tr("Mouse Only"), this);
  m_radioProfileMouseOnly->setObjectName(QStringLiteral("profileMouseOnly"));

  m_profileHidModeGroup->addButton(m_radioProfileCombo, 0);
  m_profileHidModeGroup->addButton(m_radioProfileMouseOnly, 1);

  {
    auto *l = createHBox();
    l->addWidget(m_radioProfileCombo);
    l->addSpacing(10);
    l->addWidget(m_radioProfileMouseOnly);
    l->addStretch();
    addComplexRow(tr("HID Mode:"), l, 36);
  }

  // Scroll Settings
  auto *scrollLayout = createHBox();

  // Column 1: SpinBox wrapped to taking 180px to match column alignment
  auto *spinContainer = new QWidget(this);
  spinContainer->setMinimumWidth(180);
  {
    auto *spinLayout = createHBox();
    m_spinScrollScale = new QDoubleSpinBox(this);
    m_spinScrollScale->setObjectName(QStringLiteral("profileScrollScale"));
    m_spinScrollScale->setRange(0.1, 10.0);
    m_spinScrollScale->setSingleStep(0.1);
    m_spinScrollScale->setDecimals(1);
    spinLayout->addWidget(m_spinScrollScale);
    spinLayout->addStretch();
    spinContainer->setLayout(spinLayout);
  }
  scrollLayout->addWidget(spinContainer);
  scrollLayout->addSpacing(10);

  m_checkInvertScroll = new QCheckBox(tr("Invert direction"), this);
  m_checkInvertScroll->setObjectName(QStringLiteral("profileInvertScroll"));
  m_checkInvertScroll->setMinimumWidth(150);
  scrollLayout->addWidget(m_checkInvertScroll);
  scrollLayout->addStretch();

  addComplexRow(tr("Vertical Scroll Scale:"), scrollLayout);

  groupLayout->addLayout(detailsLayout);

  // changes go to the device on OK, which also makes the profile shown active: note the
  // profiles the user edits, but not the form being filled in to show a profile
  const auto noteEdit = [this] {
    if (!m_showingProfile && m_selectedProfileIndex >= 0)
      m_editedProfiles.insert(m_selectedProfileIndex);
  };
  connect(m_editProfileName, &QLineEdit::textChanged, this, noteEdit);
  connect(m_spinProfileWidth, &QSpinBox::valueChanged, this, noteEdit);
  connect(m_spinProfileHeight, &QSpinBox::valueChanged, this, noteEdit);
  connect(m_profileOrientationGroup, &QButtonGroup::idToggled, this, noteEdit);
  connect(m_profileHidModeGroup, &QButtonGroup::idToggled, this, noteEdit);
  connect(m_spinScrollScale, &QDoubleSpinBox::valueChanged, this, noteEdit);
  connect(m_checkInvertScroll, &QCheckBox::toggled, this, noteEdit);

  // Buttons
  auto *btnLayout = new QHBoxLayout();
  m_btnProfileReset = new QPushButton(tr("Reset"), this);
  m_btnProfileReset->setObjectName(QStringLiteral("profileReset"));

  connect(m_btnProfileReset, &QPushButton::clicked, this, &BridgeClientConfigDialog::onProfileResetClicked);

  btnLayout->addWidget(m_btnProfileReset);
  btnLayout->addStretch();

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

  QString screenName = BridgeClientConfigManager::readScreenName(m_configPath);
  m_editScreenName->setText(screenName);
  m_originalScreenName = screenName;
  // a new screen name renames this config file, which a connected bridge core is using
  if (BridgeClientProcess::forDevice(m_devicePath) != nullptr) {
    m_editScreenName->setEnabled(false);
    m_editScreenName->setToolTip(tr("Disconnect to rename the screen."));
  }

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

  // Load global bonding setting
  bool bondLocation = config.value("BondScreenLocation", false).toBool();
  m_checkBondLocation->setChecked(bondLocation);
}

void BridgeClientConfigDialog::loadProfilesFromDevice()
{
  if (m_devicePath.isEmpty()) {
    m_lblProfileGroupTitle->setText(tr("Profiles (Device Not Connected)"));
    m_profileGroup->setEnabled(false);
    return;
  }

  // through the device's owner: a connected bridge core answers for it
  BridgeProfileSet profiles;
  QString error;
  if (!BridgeDeviceService::readProfiles(m_devicePath, profiles, error) || profiles.profiles.isEmpty()) {
    qWarning() << "Failed to read profiles from" << m_devicePath << ":" << error;
    m_lblProfileGroupTitle->setText(tr("Profiles (Failed to Open Device)"));
    m_profileGroup->setEnabled(false);
    return;
  }

  showProfiles(profiles);
}

void BridgeClientConfigDialog::showProfiles(const BridgeProfileSet &profiles)
{
  m_totalProfiles = static_cast<int>(profiles.profiles.size());
  m_deviceActiveProfileIndex = profiles.activeIndex;
  if (m_deviceActiveProfileIndex < 0 || m_deviceActiveProfileIndex >= m_totalProfiles)
    m_deviceActiveProfileIndex = 0; // Default fallback

  // Populate Tabs (blocked: changing tabs would save the form over the profiles being replaced)
  {
    const QSignalBlocker blocker(m_profileTabBar);
    while (m_profileTabBar->count() > 0)
      m_profileTabBar->removeTab(0);

    m_profileCache.clear();
    for (int i = 0; i < m_totalProfiles; ++i) {
      m_profileTabBar->addTab(QString("Slot %1").arg(i));
      m_profileCache[i] = profiles.profiles.at(i);
    }
    m_deviceProfiles.clear();
    rememberDeviceProfiles(profiles);
    m_editedProfiles.clear();
    m_resetProfiles.clear();

    // Select the active profile tab by default
    m_profileTabBar->setCurrentIndex(m_deviceActiveProfileIndex);
  }

  updateTabLabels();

  m_profileGroup->setEnabled(true);

  m_selectedProfileIndex = m_deviceActiveProfileIndex;
  updateProfileDetailUI(m_selectedProfileIndex);
}

void BridgeClientConfigDialog::rememberDeviceProfiles(const BridgeProfileSet &profiles)
{
  for (qsizetype i = 0; i < profiles.profiles.size(); ++i) {
    m_deviceProfiles[static_cast<int>(i)] = profiles.profiles.at(i);
  }
}

bool BridgeClientConfigDialog::applyProfiles()
{
  if (m_selectedProfileIndex < 0 || m_selectedProfileIndex >= m_totalProfiles)
    return true; // the device's profiles weren't read

  saveUiToCache(m_selectedProfileIndex);

  // the profiles the user changed, if they differ from what the device has (a reset profile's
  // default once erased); the one shown becomes the active one
  BridgeProfileBackup edits;
  edits.activeProfile = m_selectedProfileIndex;
  for (int index : std::as_const(m_editedProfiles)) {
    const auto edited = m_profileCache.value(index);
    const auto stored =
        m_resetProfiles.contains(index) ? BridgeProfileBackup::unsetProfile(index) : m_deviceProfiles.value(index);
    if (std::memcmp(&edited, &stored, sizeof(edited)) != 0)
      edits.profiles.insert(index, edited);
  }
  QList<int> resets = m_resetProfiles.values();
  std::sort(resets.begin(), resets.end());

  // the checks of a restore, so a profile is never saved that a backup couldn't hold
  BridgeProfileBackup checked;
  QStringList problems;
  if (!edits.profiles.isEmpty() && !edits.check(checked, problems)) {
    QMessageBox::warning(this, tr("Invalid Input"), tr("Please fix these profiles first:") + bulletList(problems));
    return false;
  }

  // in the order they are made: erase, save, switch, then the rename (DeskflowHidExtension)
  QStringList changes;
  for (int index : std::as_const(resets)) {
    changes.append(
        tr("Reset %1. Its host must pair again manually.").arg(profileLabel(index, m_deviceProfiles.value(index)))
    );
  }
  QStringList saved;
  for (auto it = edits.profiles.cbegin(); it != edits.profiles.cend(); ++it) {
    saved.append(profileLabel(it.key(), it.value()));
  }
  if (!saved.isEmpty())
    changes.append(tr("Save %1.").arg(saved.join(QStringLiteral(", "))));
  if (m_selectedProfileIndex != m_deviceActiveProfileIndex) {
    changes.append(tr("Active profile changes to %1.")
                       .arg(profileLabel(m_selectedProfileIndex, m_profileCache.value(m_selectedProfileIndex))));
  }
  if (deviceNameChanged())
    changes.append(tr("Device name changes to %1.").arg(deviceName()));

  if (changes.isEmpty())
    return true;
  if (QMessageBox::question(
          this, tr("Apply Changes"), tr("Apply these changes to the device?") + bulletList(changes),
          QMessageBox::Apply | QMessageBox::Cancel, QMessageBox::Apply
      ) != QMessageBox::Apply) {
    return false;
  }

  // a connected bridge core takes on the active profile at once
  BridgeProfileSet profiles;
  QString error;
  bool ok = true;
  QApplication::setOverrideCursor(Qt::WaitCursor);
  for (int index : std::as_const(resets)) {
    ok = BridgeDeviceService::eraseProfile(m_devicePath, index, profiles, error);
    if (!ok)
      break;
    m_resetProfiles.remove(index);
    rememberDeviceProfiles(profiles);
    m_deviceActiveProfileIndex = profiles.activeIndex;
  }
  if (ok && !edits.profiles.isEmpty()) {
    QList<int> written;
    ok = BridgeDeviceService::writeProfiles(m_devicePath, edits, written, profiles, error);
  } else if (ok && m_selectedProfileIndex != m_deviceActiveProfileIndex) {
    ok = BridgeDeviceService::switchProfile(m_devicePath, m_selectedProfileIndex, profiles, error);
  }
  QApplication::restoreOverrideCursor();
  if (!ok) {
    QMessageBox::critical(this, tr("Error"), tr("Failed to update the device: %1").arg(error));
    return false;
  }

  if (!profiles.profiles.isEmpty()) {
    rememberDeviceProfiles(profiles);
    m_deviceActiveProfileIndex = profiles.activeIndex;
  }
  m_editedProfiles.clear();
  return true;
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
  // e.g. 0.3 * 10 is 2.999...; 0, the firmware's default, is shown as 1.0, so keep it as it is
  const auto scale = static_cast<uint8_t>(qRound(m_spinScrollScale->value() * 10));
  if (!(p.yScrollScale == 0 && scale == 10))
    p.yScrollScale = scale;
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
      label = tr("%1 (Active)").arg(label);
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
  m_showingProfile = true; // filling in the form isn't an edit

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

  // 0 is the firmware's default and means 1.0 (see BridgePlatformScreen::applyClientScrollModifier)
  double scale = (p.yScrollScale == 0) ? 1.0 : static_cast<double>(p.yScrollScale) / 10.0;
  m_spinScrollScale->setValue(scale);
  m_checkInvertScroll->setChecked(p.invert != 0);

  m_showingProfile = false;
  blockSignals(blocked);
}

void BridgeClientConfigDialog::onProfileResetClicked()
{
  if (m_selectedProfileIndex < 0 || m_selectedProfileIndex >= m_totalProfiles)
    return;

  // erased on OK; until then the form shows the profile as it will be
  m_resetProfiles.insert(m_selectedProfileIndex);
  m_editedProfiles.remove(m_selectedProfileIndex);
  m_profileCache[m_selectedProfileIndex] = BridgeProfileBackup::unsetProfile(m_selectedProfileIndex);
  updateProfileDetailUI(m_selectedProfileIndex);
  updateTabLabels();
}

void BridgeClientConfigDialog::onProfileBackupClicked()
{
  BridgeProfileBackup backup;
  QString error;
  if (!BridgeDeviceService::backupProfiles(m_devicePath, backup, error)) {
    QMessageBox::critical(this, tr("Backup Profiles"), tr("Failed to read the profiles: %1").arg(error));
    return;
  }
  if (backup.profiles.isEmpty()) {
    QMessageBox::information(this, tr("Backup Profiles"), tr("This device has no profiles to backup."));
    return;
  }

  // only save a file that restore will take
  const QByteArray json = backup.toJson();
  BridgeProfileBackup check;
  QStringList problems;
  if (!BridgeProfileBackup::fromJson(json, check, problems)) {
    QMessageBox::warning(this, tr("Backup Profiles"), tr("Please fix these profiles first:") + bulletList(problems));
    return;
  }

  const QString suggested = BridgeProfileBackup::unusedFileName(
      QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)), m_editScreenName->text(),
      QDate::currentDate()
  );
  const QString path =
      QFileDialog::getSaveFileName(this, tr("Backup Profiles"), suggested, tr("Profile backups (*.json)"));
  if (path.isEmpty())
    return;

  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly) || file.write(json) != json.size() || !file.commit()) {
    QMessageBox::critical(
        this, tr("Backup Profiles"), tr("Failed to save %1: %2").arg(QDir::toNativeSeparators(path), file.errorString())
    );
    return;
  }

  // the path is a link to the file in the file manager
  const QString folderUrl = QUrl::fromLocalFile(QFileInfo(path).absolutePath()).toString(QUrl::FullyEncoded);
  QMessageBox box(
      QMessageBox::Information, tr("Backup Profiles"),
      tr("Backup saved to:").toHtmlEscaped() +
          QStringLiteral("<br><a href=\"%1\">%2</a>").arg(folderUrl, QDir::toNativeSeparators(path).toHtmlEscaped()),
      QMessageBox::Ok, this
  );
  box.setTextFormat(Qt::RichText);
  box.setTextInteractionFlags(Qt::TextBrowserInteraction);
  box.setOption(QMessageBox::Option::DontUseNativeDialog); // a native alert shows no links
  // select the file rather than only open its folder, as the label would by itself
  if (auto *label = box.findChild<QLabel *>(QStringLiteral("qt_msgbox_label"))) {
    label->setOpenExternalLinks(false);
    connect(label, &QLabel::linkActivated, &box, [path] { showInFileManager(path); });
  }
  box.exec();
}

void BridgeClientConfigDialog::onProfileRestoreClicked()
{
  const QString path = QFileDialog::getOpenFileName(
      this, tr("Restore Profiles"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
      tr("Profile backups (*.json);;All files (*)")
  );
  if (path.isEmpty())
    return;
  const QString fileName = QDir::toNativeSeparators(path);

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    QMessageBox::critical(this, tr("Restore Profiles"), tr("Failed to open %1: %2").arg(fileName, file.errorString()));
    return;
  }
  const QByteArray json = file.read(BridgeProfileBackup::kMaxFileSize + 1);
  file.close();

  // nothing reaches the form unless every value in the file is valid and fits the device
  BridgeProfileBackup backup;
  QStringList problems;
  if (!BridgeProfileBackup::fromJson(json, backup, problems) || !backup.fitsDevice(m_totalProfiles, problems)) {
    QMessageBox::warning(this, tr("Restore Profiles"), tr("This file can't be restored:") + bulletList(problems));
    return;
  }

  // into the form, for OK to save; profiles not in the backup stay as they are
  saveUiToCache(m_selectedProfileIndex);
  for (auto it = backup.profiles.cbegin(); it != backup.profiles.cend(); ++it) {
    m_profileCache[it.key()] = it.value();
    m_editedProfiles.insert(it.key());
    m_resetProfiles.remove(it.key());
  }

  // the backup's active profile is shown, so OK makes it active
  {
    const QSignalBlocker blocker(m_profileTabBar);
    m_profileTabBar->setCurrentIndex(backup.activeProfile);
  }
  m_selectedProfileIndex = backup.activeProfile;
  updateProfileDetailUI(m_selectedProfileIndex);
  updateTabLabels();

  // the name too, set on OK like a typed one; a device that isn't activated keeps its name
  if (!backup.deviceName.isEmpty() && m_editDeviceName->isEnabled())
    m_editDeviceName->setText(backup.deviceName);

  QMessageBox::information(this, tr("Restore Profiles"), tr("Backup loaded. Click OK to apply."));
}

void BridgeClientConfigDialog::saveConfig()
{
  QSettings config(m_configPath, QSettings::IniFormat);

  config.setValue(Settings::Core::ComputerName, m_editScreenName->text());
  // Removed global resolution/orientation
  config.setValue(Settings::Bridge::DeviceName, deviceName());

  // Remove keys that are now managed solely by the device profile
  config.remove(QStringLiteral("client/scrollSpeed"));
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
  config.setValue("BondScreenLocation", m_checkBondLocation->isChecked());

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

  // the bridge core cleans this name (Settings::cleanComputerName) before sending it to the
  // server, and the server's computer list comes from computer_<name> settings groups, so
  // only accept names that survive both unchanged (a '/' or '\' splits the group and the
  // server won't start).  the regex mirrors cleanComputerName.
  static const QRegularExpression validScreenName(QStringLiteral("^(?![-_.])[\\w\\-.]{1,255}(?<![-_.])$"));
  if (!validScreenName.match(m_editScreenName->text()).hasMatch()) {
    QMessageBox::warning(
        this, tr("Invalid Input"),
        tr("Screen name can only contain letters, numbers, '-', '_' and '.', and cannot start or end with "
           "'-', '_' or '.'.")
    );
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
    currentSn = UsbDeviceHelper::readSerialNumber(m_devicePath);
  }

  if (!currentSn.isEmpty()) {
    QStringList allConfigs = BridgeClientConfigManager::getAllConfigFiles();
    // Check for duplicates
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

  if (!applyProfiles()) {
    return;
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

bool BridgeClientConfigDialog::eventFilter(QObject *watched, QEvent *event)
{
  if (event->type() == QEvent::MouseButtonRelease) {
    if (watched == m_lblIconLandscape) {
      m_radioProfileLandscape->setChecked(true);
      return true;
    }
    if (watched == m_lblIconPortrait) {
      m_radioProfilePortrait->setChecked(true);
      return true;
    }
  }
  return QDialog::eventFilter(watched, event);
}
