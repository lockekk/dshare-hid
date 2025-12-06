/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "Esp32HidToolsWidget.h"
#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QtConcurrent>

// Include the C++ API from the submodule
#include "devices/UsbDeviceHelper.h"
#include "flash_tool.h"

#include "platform/bridge/CdcTransport.h"

namespace deskflow::gui {

Esp32HidToolsWidget::Esp32HidToolsWidget(const QString &devicePath, QWidget *parent)
    : QDialog(parent),
      m_devicePath(devicePath),
      m_isTaskRunning(false)
{
  qInfo() << "GUI of tool widget is created for device:" << devicePath;
  setWindowTitle(tr("ESP32 HID Tools - %1").arg(devicePath));
  resize(600, 400);

  auto *mainLayout = new QVBoxLayout(this);

  // --- Port Selection ---
  auto *portLayout = new QHBoxLayout();
  m_portCombo = new QComboBox();
  m_portCombo->setEditable(true); // Allow manual entry if needed
  m_portCombo->setMinimumWidth(200);
  m_refreshPortsBtn = new QPushButton(tr("Refresh"));
  portLayout->addWidget(new QLabel(tr("Port:")));
  portLayout->addWidget(m_portCombo, 1);
  portLayout->addWidget(m_refreshPortsBtn);

  mainLayout->addLayout(portLayout);

  auto *tabWidget = new QTabWidget(this);

  // --- Factory Tab ---
  auto *factoryTab = new QWidget();
  auto *factoryLayout = new QVBoxLayout(factoryTab);

  auto *factoryInputLayout = new QHBoxLayout();
  m_factoryPathEdit = new QLineEdit();
  m_factoryPathEdit->setPlaceholderText(tr("Path to factory.enc"));
  m_factoryBrowseBtn = new QPushButton(tr("Browse..."));
  factoryInputLayout->addWidget(new QLabel(tr("Factory Package:")));
  factoryInputLayout->addWidget(m_factoryPathEdit);
  factoryInputLayout->addWidget(m_factoryBrowseBtn);

  m_factoryFlashBtn = new QPushButton(tr("Flash Factory Firmware"));
  m_copyInfoBtn = new QPushButton(tr("Copy Device Info"));
  m_copyInfoBtn->setEnabled(false); // Enabled after successful flash

  factoryLayout->addLayout(factoryInputLayout);
  factoryLayout->addWidget(m_factoryFlashBtn);
  factoryLayout->addWidget(m_copyInfoBtn);
  factoryLayout->addStretch();

  tabWidget->addTab(factoryTab, tr("Factory Mode"));

  // --- Upgrade Tab ---
  auto *upgradeTab = new QWidget();
  auto *upgradeLayout = new QVBoxLayout(upgradeTab);

  auto *upgradeInputLayout = new QHBoxLayout();
  m_upgradePathEdit = new QLineEdit();
  m_upgradePathEdit->setPlaceholderText(tr("Path to upgrade.bin"));
  m_upgradeBrowseBtn = new QPushButton(tr("Browse..."));
  upgradeInputLayout->addWidget(new QLabel(tr("Upgrade Firmware:")));
  upgradeInputLayout->addWidget(m_upgradePathEdit);
  upgradeInputLayout->addWidget(m_upgradeBrowseBtn);

  m_upgradeFlashBtn = new QPushButton(tr("Flash Application Upgrade"));

  upgradeLayout->addLayout(upgradeInputLayout);
  upgradeLayout->addWidget(m_upgradeFlashBtn);
  upgradeLayout->addStretch();

  tabWidget->addTab(upgradeTab, tr("Upgrade Mode"));

  // --- Activation Tab ---
  auto *activationTab = new QWidget();
  auto *activationLayout = new QVBoxLayout(activationTab);

  // Status Section
  auto *statusLayout = new QHBoxLayout();
  m_labelActivationState = new QLabel(tr("State: Unknown"));
  QFont stateFont = m_labelActivationState->font();
  stateFont.setBold(true);
  m_labelActivationState->setFont(stateFont);

  statusLayout->addWidget(m_labelActivationState);
  statusLayout->addStretch();
  statusLayout->addWidget(new QLabel(tr("Serial:")));
  m_lineSerial = new QLabel(tr("-"));
  m_lineSerial->setTextInteractionFlags(Qt::TextSelectableByMouse); // Allow manual selection
  QFont serialFont = m_lineSerial->font();
  serialFont.setFamily("Monospace"); // Use monospace for serial/MAC
  m_lineSerial->setFont(serialFont);
  statusLayout->addWidget(m_lineSerial);
  m_btnCopySerial = new QPushButton(tr("Copy Serial"));
  statusLayout->addWidget(m_btnCopySerial);

  // Activation Input Section
  m_groupActivationInput = new QWidget();
  auto *licenseLayout = new QHBoxLayout(m_groupActivationInput);
  licenseLayout->setContentsMargins(0, 0, 0, 0);
  m_lineActivationKey = new QLineEdit();
  m_lineActivationKey->setPlaceholderText(tr("Paste Activation Key Here"));
  m_btnActivate = new QPushButton(tr("Activate"));
  licenseLayout->addWidget(new QLabel(tr("Activation Key:")));
  licenseLayout->addWidget(m_lineActivationKey);
  licenseLayout->addWidget(m_btnActivate);

  activationLayout->addLayout(statusLayout);
  activationLayout->addWidget(m_groupActivationInput);
  activationLayout->addStretch();

  tabWidget->addTab(activationTab, tr("Activation"));

  // --- Common Output ---
  m_logOutput = new QTextEdit();
  m_logOutput->setReadOnly(true);

  mainLayout->addWidget(tabWidget);
  mainLayout->addWidget(new QLabel(tr("Log Output:")));
  mainLayout->addWidget(m_logOutput);

  // Connect Signals
  connect(tabWidget, &QTabWidget::currentChanged, this, &Esp32HidToolsWidget::onTabChanged);

  connect(m_factoryBrowseBtn, &QPushButton::clicked, this, &Esp32HidToolsWidget::onBrowseFactory);
  connect(m_factoryFlashBtn, &QPushButton::clicked, this, &Esp32HidToolsWidget::onFlashFactory);
  connect(m_copyInfoBtn, &QPushButton::clicked, this, &Esp32HidToolsWidget::onCopyInfo);

  connect(m_upgradeBrowseBtn, &QPushButton::clicked, this, &Esp32HidToolsWidget::onBrowseUpgrade);
  connect(m_upgradeFlashBtn, &QPushButton::clicked, this, &Esp32HidToolsWidget::onFlashUpgrade);

  connect(m_btnCopySerial, &QPushButton::clicked, this, &Esp32HidToolsWidget::onCopySerialClicked);
  connect(m_btnActivate, &QPushButton::clicked, this, &Esp32HidToolsWidget::onActivateClicked);

  connect(m_refreshPortsBtn, &QPushButton::clicked, this, &Esp32HidToolsWidget::refreshPorts);

  refreshPorts();
}

void Esp32HidToolsWidget::refreshPorts()
{
  m_portCombo->clear();
  qInfo() << "Refreshing ports...";
  // Only filter by VID/PID, don't try to read serial (avoids timeouts on stuck devices)
  auto devices = UsbDeviceHelper::getConnectedDevices(false);
  qInfo() << "Found" << devices.size() << "candidate devices.";

  if (devices.isEmpty()) {
    m_portCombo->addItem(tr("No devices found"));
    qWarning() << "No supported USB bridge devices found.";
    // logic to disable flash buttons?
  } else {
    for (auto it = devices.begin(); it != devices.end(); ++it) {
      QString path = it.key();
      QString serial = it.value();
      QString displayPath = path;
      if (displayPath.startsWith(QStringLiteral("\\\\.\\"))) {
        displayPath = displayPath.mid(4);
      }

      if (serial == "Unknown") {
        qInfo() << "Adding device:" << path;
        m_portCombo->addItem(displayPath, path);
      } else if (serial == displayPath) {
        qInfo() << "Adding device:" << path << "(fallback serial)";
        m_portCombo->addItem(displayPath, path);
      } else {
        qInfo() << "Adding device:" << path << "Serial:" << serial;
        m_portCombo->addItem(QString("%1 (%2)").arg(displayPath, serial), path);
      }
    }
  }

  // Try to select the device passed in constructor
  if (!m_devicePath.isEmpty()) {
    int index = m_portCombo->findData(m_devicePath);
    if (index >= 0) {
      m_portCombo->setCurrentIndex(index);
    } else {
      // Add it if not found (maybe it's not a "bridge" yet but user knows what they are doing)
      qInfo() << "Device path from bridge client not in detected list, adding manually:" << m_devicePath;
      m_portCombo->addItem(m_devicePath, m_devicePath);
      m_portCombo->setCurrentIndex(m_portCombo->count() - 1);
    }
  }
}

void Esp32HidToolsWidget::onBrowseFactory()
{
  QString path =
      QFileDialog::getOpenFileName(this, tr("Select Factory Package"), QString(), tr("Encrypted Package (*.enc)"));
  if (!path.isEmpty()) {
    m_factoryPathEdit->setText(path);
  }
}

void Esp32HidToolsWidget::onBrowseUpgrade()
{
  QString path =
      QFileDialog::getOpenFileName(this, tr("Select Firmware Binary"), QString(), tr("Binary Files (*.bin)"));
  if (!path.isEmpty()) {
    m_upgradePathEdit->setText(path);
  }
}

std::vector<uint8_t> Esp32HidToolsWidget::readFile(const QString &path)
{
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {};
  }
  QByteArray data = file.readAll();
  return std::vector<uint8_t>(data.begin(), data.end());
}

void Esp32HidToolsWidget::log(const QString &message)
{
  m_logOutput->append(message);
}

void Esp32HidToolsWidget::setControlsEnabled(bool enabled)
{
  m_factoryFlashBtn->setEnabled(enabled);
  m_upgradeFlashBtn->setEnabled(enabled);
  m_factoryBrowseBtn->setEnabled(enabled);
  m_upgradeBrowseBtn->setEnabled(enabled);
  m_refreshPortsBtn->setEnabled(enabled);
  m_portCombo->setEnabled(enabled);
}

// Helper to run tasks in background
template <typename Function> void Esp32HidToolsWidget::runBackgroundTask(Function func)
{
  setControlsEnabled(false);
  m_isTaskRunning = true;
  QApplication::setOverrideCursor(Qt::WaitCursor);

  auto *watcher = new QFutureWatcher<void>(this);
  connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher]() {
    QApplication::restoreOverrideCursor();
    setControlsEnabled(true);
    m_isTaskRunning = false;
    watcher->deleteLater();
  });

  QFuture<void> future = QtConcurrent::run(func);
  watcher->setFuture(future);
}

void Esp32HidToolsWidget::onFlashFactory()
{
  QString path = m_factoryPathEdit->text();
  QString portName = m_portCombo->currentData().toString();
  if (portName.isEmpty()) {
    portName = m_portCombo->currentText();
    if (portName.isEmpty() || portName == tr("No devices found")) {
      QMessageBox::warning(this, tr("Error"), tr("Please select a valid serial port."));
      return;
    }
  }

  if (path.isEmpty()) {
    QMessageBox::warning(this, tr("Error"), tr("Please select a factory package file."));
    return;
  }

  std::vector<uint8_t> data = readFile(path);
  if (data.empty()) {
    log(tr("Error: Failed to read file or file is empty: %1").arg(path));
    return;
  }

  std::string port = portName.toStdString();

  log(tr("Starting Factory Flash..."));

  // Create a struct to hold result so we can capture it by value
  struct Result
  {
    FlashResult res;
    std::string info;
  };

  auto task = [this, port, data]() {
    std::string info;

    // Callback for progress/logging to update UI
    auto log_cb = [this](const std::string &msg) {
      QMetaObject::invokeMethod(this, [this, msg]() { log(QString::fromStdString(msg)); });
    };

    // Optionally we can throttle progress updates if needed, but for "brief" check we can just log major steps
    // or we could implement a progress bar. For now, logging everything to UI is what the user seems to lack.
    auto progress_cb = [this](size_t written, size_t total, size_t address) {
      // Just log every 10%
      static int last_percent = -1;
      int percent = (int)(((float)written / total) * 100);
      if (percent % 10 == 0 && percent != last_percent) {
        last_percent = percent;
        QString msg = QString("Flashing... %1%").arg(percent);
        QMetaObject::invokeMethod(this, [this, msg]() { log(msg); });
      }
    };

    // Note: This calls the blocking C++ API
    FlashResult res = flash_factory(port, data, info, progress_cb, log_cb);

    // Update UI on main thread
    QMetaObject::invokeMethod(this, [this, res, info]() {
      if (res == FlashResult::OK) {
        log(tr("Factory Flash Success!"));
        log(tr("Device Info: %1").arg(QString::fromStdString(info)));
        m_copyInfoBtn->setEnabled(true);
        m_copyInfoBtn->setProperty("deviceInfo", QString::fromStdString(info));
      } else {
        log(tr("Factory Flash Failed! Error code: %1").arg(static_cast<int>(res)));
      }
    });
  };

  runBackgroundTask(task);
}

void Esp32HidToolsWidget::onCopyInfo()
{
  QString info = m_copyInfoBtn->property("deviceInfo").toString();
  QApplication::clipboard()->setText(info);
  log(tr("Device Info copied to clipboard."));
}

void Esp32HidToolsWidget::onFlashUpgrade()
{
  QString path = m_upgradePathEdit->text();
  QString portName = m_portCombo->currentData().toString();
  if (portName.isEmpty()) {
    portName = m_portCombo->currentText();
    if (portName.isEmpty() || portName == tr("No devices found")) {
      QMessageBox::warning(this, tr("Error"), tr("Please select a valid serial port."));
      return;
    }
  }

  if (path.isEmpty()) {
    QMessageBox::warning(this, tr("Error"), tr("Please select an upgrade binary file."));
    return;
  }

  std::vector<uint8_t> data = readFile(path);
  if (data.empty()) {
    log(tr("Error: Failed to read file or file is empty: %1").arg(path));
    return;
  }

  std::string port = portName.toStdString();

  log(tr("Starting Firmware Upgrade..."));

  auto task = [this, port, data]() {
    // Callback for logging
    auto log_cb = [this](const std::string &msg) {
      QMetaObject::invokeMethod(this, [this, msg]() { log(QString::fromStdString(msg)); });
    };

    // Callback for progress (if supported by upgrade)
    auto progress_cb = [this](size_t written, size_t total, size_t address) {
      // Just log every 10%
      static int last_percent = -1;
      int percent = (int)(((float)written / total) * 100);
      if (percent % 10 == 0 && percent != last_percent) {
        last_percent = percent;
        QString msg = QString("Flashing... %1%").arg(percent);
        QMetaObject::invokeMethod(this, [this, msg]() { log(msg); });
      }
    };

    // Note: This calls the blocking C++ API
    FlashResult res = flash_upgrade(port, data, progress_cb, log_cb);

    // Update UI on main thread
    QMetaObject::invokeMethod(this, [this, res]() {
      if (res == FlashResult::OK) {
        log(tr("Upgrade Success!"));
      } else {
        log(tr("Upgrade Failed! Error code: %1").arg(static_cast<int>(res)));
      }
    });
  };

  runBackgroundTask(task);
}

void Esp32HidToolsWidget::reject()
{
  if (m_isTaskRunning) {
    return;
  }
  QDialog::reject();
}

} // namespace deskflow::gui

void deskflow::gui::Esp32HidToolsWidget::setupUI()
{
}

void deskflow::gui::Esp32HidToolsWidget::onTabChanged(int index)
{
  QTabWidget *tabs = qobject_cast<QTabWidget *>(sender());
  if (tabs && tabs->tabText(index) == tr("Activation")) {
    refreshDeviceState();
  }
}

void deskflow::gui::Esp32HidToolsWidget::refreshDeviceState()
{
  QString portName = m_portCombo->currentData().toString();
  if (portName.isEmpty()) {
    portName = m_portCombo->currentText();
    if (portName.isEmpty() || portName == tr("No devices found")) {
      return;
    }
  }

  log(tr("Refreshing device state..."));
  m_lineSerial->clear();
  m_labelActivationState->setText(tr("State: Checking..."));
  setControlsEnabled(false); // Disable while checking

  auto task = [this, portName]() {
    deskflow::bridge::CdcTransport cdc(portName);
    std::string serial;
    bool openSuccess = cdc.open();

    struct State
    {
      QString serial;
      QString activationState;
      bool isActivated;
      bool success;
      QString error;
    } result;

    if (openSuccess) {
      if (cdc.fetchSerialNumber(serial)) {
        result.serial = QString::fromStdString(serial);
      }

      const auto &config = cdc.deviceConfig();
      result.activationState = QString::fromLatin1(config.activationStateString());
      result.isActivated = (config.activationState == deskflow::bridge::ActivationState::Activated);
      result.success = true;
    } else {
      result.success = false;
      result.error = QString::fromStdString(cdc.lastError());
    }

    QMetaObject::invokeMethod(this, [this, result]() {
      setControlsEnabled(true);
      if (result.success) {
        m_lineSerial->setText(result.serial);
        m_labelActivationState->setText(tr("State: %1").arg(result.activationState));

        // Conditional UI: Hide activation input if already activated
        m_groupActivationInput->setVisible(!result.isActivated);

        log(tr("Device State Refreshed. Serial: %1, State: %2").arg(result.serial, result.activationState));
      } else {
        m_labelActivationState->setText(tr("State: Error"));
        log(tr("Failed to refresh state: %1").arg(result.error));
      }
    });
  };
  m_isTaskRunning = true;
  QApplication::setOverrideCursor(Qt::WaitCursor);
  auto *watcher = new QFutureWatcher<void>(this);
  connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher]() {
    QApplication::restoreOverrideCursor();
    m_isTaskRunning = false;
    watcher->deleteLater();
  });
  QFuture<void> future = QtConcurrent::run(task);
  watcher->setFuture(future);
}

void deskflow::gui::Esp32HidToolsWidget::onCopySerialClicked()
{
  QString serial = m_lineSerial->text();
  if (!serial.isEmpty()) {
    QApplication::clipboard()->setText(serial);
    log(tr("Serial number copied to clipboard."));
  }
}

void deskflow::gui::Esp32HidToolsWidget::onActivateClicked()
{
  QString portName = m_portCombo->currentData().toString();
  if (portName.isEmpty()) {
    portName = m_portCombo->currentText();
    if (portName.isEmpty() || portName == tr("No devices found")) {
      QMessageBox::warning(this, tr("Error"), tr("Please select a valid serial port."));
      return;
    }
  }

  QString key = m_lineActivationKey->text().trimmed();
  if (key.isEmpty()) {
    QMessageBox::warning(this, tr("Error"), tr("Please enter an activation key."));
    return;
  }

  log(tr("Activating device..."));

  auto task = [this, portName, key]() {
    deskflow::bridge::CdcTransport cdc(portName);
    if (cdc.open() && cdc.activateDevice(key.toStdString())) {
      QMetaObject::invokeMethod(this, [this]() {
        log(tr("Activation successful!"));
        QMessageBox::information(this, tr("Success"), tr("Device activated successfully."));
        refreshDeviceState(); // Refresh to update UI
      });
    } else {
      // User requested minimal error message
      QString detailedError = QString::fromStdString(cdc.lastError());
      qWarning() << "Activation detailed error:" << detailedError;

      QMetaObject::invokeMethod(this, [this]() {
        log(tr("Activation failed."));
        QMessageBox::critical(this, tr("Error"), tr("Activation failed."));
      });
    }
  };
  runBackgroundTask(task);
}
