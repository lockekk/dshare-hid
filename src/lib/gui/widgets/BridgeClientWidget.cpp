/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#include "BridgeClientWidget.h"
#include <QEvent>

#include "common/Settings.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QSettings>
#include <QSignalBlocker>
#include <QToolButton>

namespace {
constexpr auto kLandscapeIconPath = ":/bridge-client/client/orientation_landscape.svg";
constexpr auto kPortraitIconPath = ":/bridge-client/client/orientation_portrait.svg";

constexpr int kProfileIconSize = 16;
constexpr int kProfileNameMaxWidth = 120;
constexpr auto kProfileStyle = "QToolButton { border: 1px solid palette(mid); border-radius: 4px; padding: 2px 6px; }"
                               "QToolButton:hover { border-color: palette(highlight); }";
constexpr auto kActiveProfileStyle =
    "QToolButton { border: 1px solid palette(highlight); border-radius: 4px; padding: 2px 6px;"
    " background-color: palette(highlight); color: palette(highlighted-text); }";

// the orientation icons are drawn in a single dark color, so repaint them in the button's text
// color: otherwise they vanish on the highlighted button and in dark mode
QIcon tintedIcon(const QString &path, const QColor &color, qreal devicePixelRatio)
{
  QPixmap pixmap = QIcon(path).pixmap(QSize(kProfileIconSize, kProfileIconSize), devicePixelRatio);
  QPainter painter(&pixmap);
  painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
  painter.fillRect(pixmap.rect(), color);
  painter.end();
  return QIcon(pixmap);
}
} // namespace

namespace deskflow::gui {

BridgeClientWidget::BridgeClientWidget(
    const QString &screenName, const QString &serialNumber, const QString &devicePath, const QString &configPath,
    QWidget *parent
)
    : QGroupBox(screenName, parent),
      m_screenName(screenName),
      m_serialNumber(serialNumber),
      m_devicePath(devicePath),
      m_configPath(configPath),
      m_deviceAvailable(false),
      m_serverReady(false)
{
  setMinimumWidth(100);
  setMaximumWidth(16777215); // QWIDGETSIZE_MAX

  // Create horizontal layout for buttons
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(8);

  // Create Connect button (toggleable)
  m_btnConnect = new QPushButton(tr("Connect"), this);
  m_btnConnect->setCheckable(true);
  m_btnConnect->setChecked(false); // Default disconnected
  m_btnConnect->setMinimumSize(80, 32);
  m_btnConnect->setToolTip(tr("Connect/disconnect bridge client"));

  // Create Configure button
  m_btnConfigure = new QPushButton(tr("Configure"), this);
  m_btnConfigure->setMinimumSize(80, 32);
  m_btnConfigure->setToolTip(tr("Configure bridge client settings"));

  m_deviceNameLabel = new QLabel(tr("--"), this);
  m_deviceNameLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  m_deviceNameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  m_deviceNameLabel->setMinimumWidth(100);

  m_activeHostnameLabel = new QLabel(this);
  m_activeHostnameLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  m_activeHostnameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  m_activeHostnameLabel->setStyleSheet("color: gray;"
  ); // Make it distinct, maybe? Or just normal. user said "next to", didn't specify style.

  // Activation state and orientation labels
  m_activationStateLabel = new QLabel(tr("unknown"), this);
  m_activationStateLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  m_activationStateLabel->setMinimumWidth(90);

  m_activationStateLabel->setMinimumWidth(90);

  m_orientationLabel = new QLabel(this);
  m_orientationLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  m_orientationLabel->setFixedSize(40, 30);

  // Add buttons and label to layout
  layout->addWidget(m_btnConnect);
  layout->addWidget(m_btnConfigure);

  // Profile buttons, right of the device name: filled in by setProfiles() once connected
  m_profileRow = new QWidget(this);
  m_profileLayout = new QHBoxLayout(m_profileRow);
  m_profileLayout->setContentsMargins(0, 0, 0, 0);
  m_profileLayout->setSpacing(6);
  m_profileLayout->addStretch();
  m_profileRow->setVisible(false);

  layout->addSpacing(12);
  layout->addWidget(m_deviceNameLabel);        // Removed stretch, will share with hostname
  layout->addWidget(m_profileRow, 1);          // replaces the hostname while shown, so takes its stretch
  layout->addWidget(m_activeHostnameLabel, 1); // Give stretch to hostname
  layout->addWidget(m_activationStateLabel);
  layout->addWidget(m_orientationLabel);

  // Connect signals
  connect(m_btnConnect, &QPushButton::toggled, this, &BridgeClientWidget::onConnectToggled);
  connect(m_btnConfigure, &QPushButton::clicked, this, &BridgeClientWidget::onConfigureClicked);

  refreshDeviceNameLabel();
  refreshActivationStateLabel();
  refreshOrientationLabel();
  refreshButtonStates();
}

void BridgeClientWidget::setConnected(bool connected)
{
  if (m_isConnected == connected)
    return;

  m_isConnected = connected;
  {
    const QSignalBlocker blocker(m_btnConnect);
    m_btnConnect->setChecked(connected);
  }
  m_btnConnect->setText(connected ? tr("Disconnect") : tr("Connect"));

  if (!connected) {
    m_profiles.clear();
  }

  updateActivationVisibility();
  setActiveHostname(m_activeHostname);
  refreshButtonStates();
  refreshProfileButtons();
}

void BridgeClientWidget::resetConnectionStatus()
{
  setActivationState("");
  refreshDeviceNameLabel(); // the saved name, as shown before connecting
  updateActivationVisibility();
}

void BridgeClientWidget::setServerReady(bool ready)
{
  if (m_serverReady == ready)
    return;

  m_serverReady = ready;
  refreshButtonStates();
}

void BridgeClientWidget::updateConfig(const QString &screenName, const QString &configPath)
{
  m_screenName = screenName;
  m_configPath = configPath;
  setTitle(screenName); // Update the group box title

  refreshActivationStateLabel();
  refreshDeviceNameLabel();
  refreshOrientationLabel();
}

void BridgeClientWidget::setActivationState(const QString &activationState)
{
  QString normalized = activationState.trimmed();
  if (normalized.isEmpty()) {
    normalized = Settings::defaultValue(Settings::Bridge::ActivationState).toString();
  }
  m_activationState = normalized;
  m_activationStateLabel->setText(normalized);
  m_activationStateLabel->setToolTip(normalized);

  m_activationStateLabel->setStyleSheet(QStringLiteral(""));

  updateActivationVisibility();
}

void BridgeClientWidget::setDeviceName(const QString &deviceName)
{
  const QString trimmed = deviceName.trimmed();
  m_deviceName = trimmed;
  const QString display = trimmed.isEmpty() ? tr("--") : trimmed;
  m_deviceNameLabel->setText(display);
}

void BridgeClientWidget::setActiveHostname(const QString &hostname)
{
  m_activeHostname = hostname.trimmed();
  m_activeHostnameLabel->setText(m_activeHostname);

  if (m_isBleConnected && m_isConnected) {
    // Only red if BOTH bridge is connected AND BLE is connected
    m_activeHostnameLabel->setStyleSheet(QStringLiteral("color: red; font-weight: normal;"));
  } else if (m_isConnected) {
    m_activeHostnameLabel->setStyleSheet(QStringLiteral("")); // Default color
  } else {
    m_activeHostnameLabel->setStyleSheet(QStringLiteral("color: gray; font-weight: normal;"));
  }
}

void BridgeClientWidget::setBleConnected(bool connected)
{
  if (m_isBleConnected == connected)
    return;

  m_isBleConnected = connected;
  setActiveHostname(m_activeHostname);
}

void BridgeClientWidget::setProfiles(const QList<BridgeProfileInfo> &profiles, int activeIndex)
{
  m_profiles = profiles;
  m_activeProfileIndex = activeIndex;
  refreshProfileButtons();
}

void BridgeClientWidget::refreshProfileButtons()
{
  // buttons are reused rather than recreated: switching disconnects this widget, which clears
  // the profiles while the clicked button is still emitting
  while (m_profileButtons.size() < m_profiles.size()) {
    auto *button = new QToolButton(m_profileRow);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setIconSize(QSize(kProfileIconSize, kProfileIconSize));
    const int position = m_profileButtons.size();
    connect(button, &QToolButton::clicked, this, [this, position]() {
      if (position < m_profiles.size() && m_profiles.at(position).index != m_activeProfileIndex) {
        Q_EMIT profileSwitchRequested(m_devicePath, m_configPath, m_profiles.at(position).index);
      }
    });
    m_profileLayout->insertWidget(m_profileLayout->count() - 1, button); // before the stretch
    m_profileButtons.append(button);
  }

  for (int i = 0; i < m_profileButtons.size(); ++i) {
    QToolButton *button = m_profileButtons.at(i);
    if (i >= m_profiles.size()) {
      button->hide();
      continue;
    }

    const BridgeProfileInfo &profile = m_profiles.at(i);
    const bool active = (profile.index == m_activeProfileIndex);
    // same fallback as the Configure dialog's profile tabs
    const QString name = profile.hostname.isEmpty() ? QStringLiteral("Slot %1").arg(profile.index) : profile.hostname;

    button->setText(button->fontMetrics().elidedText(name, Qt::ElideRight, kProfileNameMaxWidth));
    button->setToolTip(active ? tr("Active profile: %1").arg(name) : tr("Switch to profile: %1").arg(name));
    button->setIcon(tintedIcon(
        QString::fromLatin1(profile.portrait ? kPortraitIconPath : kLandscapeIconPath),
        palette().color(active ? QPalette::HighlightedText : QPalette::ButtonText), devicePixelRatioF()
    ));
    button->setStyleSheet(QString::fromLatin1(active ? kActiveProfileStyle : kProfileStyle));
    button->setCursor(active ? Qt::ArrowCursor : Qt::PointingHandCursor);
    button->show();
  }

  // the highlighted button names the active profile, so the hostname label would only repeat it
  const bool showProfiles = m_isConnected && !m_profiles.isEmpty();
  m_profileRow->setVisible(showProfiles);
  m_activeHostnameLabel->setVisible(!showProfiles);
}

void BridgeClientWidget::refreshActivationStateLabel()
{
  QString activationState = Settings::defaultValue(Settings::Bridge::ActivationState).toString();
  if (!m_configPath.isEmpty()) {
    QSettings config(m_configPath, QSettings::IniFormat);
    activationState = config.value(Settings::Bridge::ActivationState, activationState).toString();
  }
  setActivationState(activationState);
}

void BridgeClientWidget::refreshDeviceNameLabel()
{
  QString name = Settings::defaultValue(Settings::Bridge::DeviceName).toString();
  QString hostname = Settings::defaultValue(Settings::Bridge::ActiveProfileHostname).toString();

  if (!m_configPath.isEmpty()) {
    QSettings config(m_configPath, QSettings::IniFormat);
    name = config.value(Settings::Bridge::DeviceName, name).toString();
    hostname = config.value(Settings::Bridge::ActiveProfileHostname, hostname).toString();
  }
  setDeviceName(name);
  setActiveHostname(hostname);
}

void BridgeClientWidget::refreshOrientationLabel()
{
  if (m_configPath.isEmpty()) {
    m_orientationLabel->setPixmap(QPixmap(QString::fromLatin1(kLandscapeIconPath)));
    m_orientationLabel->setToolTip(tr("Landscape"));
    return;
  }

  QSettings config(m_configPath, QSettings::IniFormat);
  const QString orientation = config.value(Settings::Bridge::ActiveProfileOrientation, "landscape").toString();
  m_orientation = orientation;
  const QString normalized = orientation.trimmed().toLower();
  QString display;
  const bool portrait = (normalized == QStringLiteral("portrait"));
  display = portrait ? tr("Portrait") : tr("Landscape");
  const auto iconPath = portrait ? kPortraitIconPath : kLandscapeIconPath;
  m_orientationLabel->setPixmap(QPixmap(QString::fromLatin1(iconPath)));
  m_orientationLabel->setToolTip(display);
}

void BridgeClientWidget::setDeviceAvailable(const QString &devicePath, bool available)
{
  m_deviceAvailable = available;
  m_devicePath = available ? devicePath : QString();

  refreshButtonStates();

  m_deviceNameLabel->setEnabled(available);
  m_activeHostnameLabel->setEnabled(available);
  m_orientationLabel->setEnabled(available);
  updateActivationVisibility();

  if (!available) {
    setStyleSheet("QGroupBox { color: gray; }");
  } else {
    setStyleSheet("");
  }
}

void BridgeClientWidget::updateActivationVisibility()
{
  m_activationStateLabel->setVisible(
      m_deviceAvailable && m_isConnected && !m_activationState.isEmpty() && m_activationState != "unknown"
  );
}

void BridgeClientWidget::setGroupLocked(bool locked, const QString &reason)
{
  m_groupLocked = locked;
  m_groupLockReason = reason;
  refreshButtonStates();
}

void BridgeClientWidget::refreshButtonStates()
{
  const bool connectEnabled =
      m_deviceAvailable && (!m_groupLocked || m_isConnected) && (m_serverReady || m_isConnected);
  m_btnConnect->setEnabled(connectEnabled);

  QString connectTooltip;
  if (!m_deviceAvailable) {
    connectTooltip = tr("Device not connected");
  } else if (!m_serverReady && !m_isConnected) {
    connectTooltip = tr("Waiting for server to start...");
  } else if (m_groupLocked && !m_isConnected) {
    connectTooltip =
        m_groupLockReason.isEmpty() ? tr("Another profile for this device is already connected") : m_groupLockReason;
  } else {
    connectTooltip = tr("Connect/disconnect bridge client");
  }
  m_btnConnect->setToolTip(connectTooltip);

  const bool configureEnabled = m_deviceAvailable && !m_isConnected && !m_groupLocked;
  m_btnConfigure->setEnabled(configureEnabled);

  QString configureTooltip;
  if (!m_deviceAvailable) {
    configureTooltip = tr("Device not connected");
  } else if (m_isConnected) {
    configureTooltip = tr("Disconnect before configuring");
  } else if (m_groupLocked) {
    configureTooltip =
        m_groupLockReason.isEmpty() ? tr("This profile is locked because another one is connected") : m_groupLockReason;
  } else {
    configureTooltip = tr("Configure bridge client settings");
  }
  m_btnConfigure->setToolTip(configureTooltip);
}

void BridgeClientWidget::onConnectToggled(bool checked)
{
  m_isConnected = checked;
  m_btnConnect->setText(checked ? tr("Disconnect") : tr("Connect"));
  if (!checked) {
    m_profiles.clear();
  }
  refreshButtonStates();
  refreshProfileButtons();

  Q_EMIT connectToggled(m_devicePath, m_configPath, checked);
}

void BridgeClientWidget::onConfigureClicked()
{
  Q_EMIT configureClicked(m_devicePath, m_configPath);
}

void BridgeClientWidget::changeEvent(QEvent *event)
{
  QGroupBox::changeEvent(event);
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  } else if (event->type() == QEvent::PaletteChange) {
    refreshProfileButtons(); // icon colors follow the palette (e.g. a dark mode switch)
  }
}

void BridgeClientWidget::retranslateUi()
{
  m_btnConnect->setText(m_isConnected ? tr("Disconnect") : tr("Connect"));
  m_btnConfigure->setText(tr("Configure"));

  refreshDeviceNameLabel();
  refreshActivationStateLabel();
  refreshButtonStates();
  refreshOrientationLabel();
  refreshProfileButtons();
}

} // namespace deskflow::gui
