/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "BridgeClientWidget.h"

#include "common/Settings.h"

#include <QHBoxLayout>
#include <QSettings>

namespace {
constexpr auto kLandscapeIconPath = ":/bridge-client/bridge_client_icon_landspace.png";
constexpr auto kPortraitIconPath = ":/bridge-client/bridge_client_icon_portrait.png";
} // namespace

namespace deskflow::gui {

BridgeClientWidget::BridgeClientWidget(
    const QString &screenName, const QString &devicePath, const QString &configPath, QWidget *parent
) :
    QGroupBox(screenName, parent),
    m_screenName(screenName),
    m_devicePath(devicePath),
    m_configPath(configPath)
{
  setMinimumWidth(360);
  setMaximumWidth(360);

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

  // Orientation label placeholder
  m_orientationLabel = new QLabel(this);
  m_orientationLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  m_orientationLabel->setFixedSize(40, 30);

  // Add buttons and label to layout
  layout->addWidget(m_btnConnect);
  layout->addWidget(m_btnConfigure);
  layout->addStretch(); // Push buttons to the left
  layout->addWidget(m_orientationLabel);

  // Connect signals
  connect(m_btnConnect, &QPushButton::toggled, this, &BridgeClientWidget::onConnectToggled);
  connect(m_btnConfigure, &QPushButton::clicked, this, &BridgeClientWidget::onConfigureClicked);

  refreshOrientationLabel();
}

void BridgeClientWidget::setConnected(bool connected)
{
  m_isConnected = connected;
  m_btnConnect->setChecked(connected);
  m_btnConnect->setText(connected ? tr("Disconnect") : tr("Connect"));

  // Disable Configure button when connected
  m_btnConfigure->setEnabled(!connected);
}

void BridgeClientWidget::updateConfig(const QString &screenName, const QString &configPath)
{
  m_screenName = screenName;
  m_configPath = configPath;
  setTitle(screenName); // Update the group box title
  refreshOrientationLabel();
}

void BridgeClientWidget::refreshOrientationLabel()
{
  if (m_configPath.isEmpty()) {
    m_orientationLabel->setPixmap(QPixmap(QString::fromLatin1(kLandscapeIconPath)));
    m_orientationLabel->setToolTip(tr("Landscape"));
    return;
  }

  QSettings config(m_configPath, QSettings::IniFormat);
  const QString orientation = config
                                  .value(
                                      Settings::Bridge::ScreenOrientation,
                                      Settings::defaultValue(Settings::Bridge::ScreenOrientation))
                                  .toString();
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

  // Enable/disable the connect button based on device availability
  m_btnConnect->setEnabled(available);

  // Configure button is enabled when device is not connected
  // (can configure even if device not plugged in, but not while connected)
  m_btnConfigure->setEnabled(!m_isConnected);

  // Update tooltip to indicate device status
  if (!available) {
    m_btnConnect->setToolTip(tr("Device not connected"));
    setStyleSheet("QGroupBox { color: gray; }");
  } else {
    m_btnConnect->setToolTip(tr("Connect/disconnect bridge client"));
    setStyleSheet("");
  }
}

void BridgeClientWidget::onConnectToggled(bool checked)
{
  m_isConnected = checked;
  m_btnConnect->setText(checked ? tr("Disconnect") : tr("Connect"));

  // Disable Configure button when connected
  m_btnConfigure->setEnabled(!checked);

  Q_EMIT connectToggled(m_devicePath, checked);
}

void BridgeClientWidget::onConfigureClicked()
{
  Q_EMIT configureClicked(m_devicePath, m_configPath);
}

} // namespace deskflow::gui
