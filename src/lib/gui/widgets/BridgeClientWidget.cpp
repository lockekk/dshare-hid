/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "BridgeClientWidget.h"

#include <QHBoxLayout>

namespace deskflow::gui {

BridgeClientWidget::BridgeClientWidget(
    const QString &screenName, const QString &devicePath, const QString &configPath, QWidget *parent
) :
    QGroupBox(screenName, parent),
    m_screenName(screenName),
    m_devicePath(devicePath),
    m_configPath(configPath)
{
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

  // Add buttons to layout
  layout->addWidget(m_btnConnect);
  layout->addWidget(m_btnConfigure);
  layout->addStretch(); // Push buttons to the left

  // Connect signals
  connect(m_btnConnect, &QPushButton::toggled, this, &BridgeClientWidget::onConnectToggled);
  connect(m_btnConfigure, &QPushButton::clicked, this, &BridgeClientWidget::onConfigureClicked);
}

void BridgeClientWidget::setConnected(bool connected)
{
  m_isConnected = connected;
  m_btnConnect->setChecked(connected);
  m_btnConnect->setText(connected ? tr("Disconnect") : tr("Connect"));
}

void BridgeClientWidget::updateConfig(const QString &screenName, const QString &configPath)
{
  m_screenName = screenName;
  m_configPath = configPath;
  setTitle(screenName); // Update the group box title
}

void BridgeClientWidget::onConnectToggled(bool checked)
{
  m_isConnected = checked;
  m_btnConnect->setText(checked ? tr("Disconnect") : tr("Connect"));
  Q_EMIT connectToggled(m_devicePath, checked);
}

void BridgeClientWidget::onConfigureClicked()
{
  Q_EMIT configureClicked(m_devicePath, m_configPath);
}

} // namespace deskflow::gui
