/*
 * Deskflow-hid -- created by locke.huang@gmail.com
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
#include "components/downloader/github_downloader.h"
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
  if (devicePath.isEmpty()) {
    setWindowTitle(tr("Firmware Flash Tool"));
  } else {
    setWindowTitle(tr("Firmware Flash Tool - %1").arg(devicePath));
  }
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
  m_factoryPathEdit->setPlaceholderText(tr("Path to factory.fzip"));
  m_factoryBrowseBtn = new QPushButton(tr("Browse..."));
  factoryInputLayout->addWidget(new QLabel(tr("Factory Package:")));
  factoryInputLayout->addWidget(m_factoryPathEdit);
  factoryInputLayout->addWidget(m_factoryBrowseBtn);

  m_factoryFlashBtn = new QPushButton(tr("Flash Factory Firmware"));
  m_downloadFlashBtn = new QPushButton(tr("Download and Flash"));
  m_copyInfoBtn = new QPushButton(tr("Copy Device Info"));

  factoryLayout->addLayout(factoryInputLayout);
  factoryLayout->addWidget(m_factoryFlashBtn);
  factoryLayout->addWidget(m_downloadFlashBtn);
  factoryLayout->addWidget(m_copyInfoBtn);
  factoryLayout->addStretch();

  tabWidget->addTab(factoryTab, tr("Factory Mode"));

  // --- Upgrade Tab ---
  auto *upgradeTab = new QWidget();
  auto *upgradeLayout = new QVBoxLayout(upgradeTab);

  // --- Online Upgrade Group ---
  auto *onlineGroup = new QGroupBox(tr("Online Upgrade"));
  auto *onlineLayout = new QVBoxLayout(onlineGroup);

  auto *verLayout = new QHBoxLayout();
  m_lblCurrentVersion = new QLabel(tr("Current Version: Unknown"));
  m_lblLatestVersion = new QLabel(tr("Latest Version: Unknown"));
  verLayout->addWidget(m_lblCurrentVersion);
  verLayout->addSpacing(20);
  verLayout->addWidget(m_lblLatestVersion);
  verLayout->addStretch();

  auto *onlineBtnLayout = new QHBoxLayout();
  m_checkUpgradeBtn = new QPushButton(tr("Check for Updates"));
  m_flashOnlineBtn = new QPushButton(tr("Download & Flash"));
  m_flashOnlineBtn->setEnabled(false); // Enabled after check
  onlineBtnLayout->addWidget(m_checkUpgradeBtn);
  onlineBtnLayout->addWidget(m_flashOnlineBtn);
  onlineBtnLayout->addStretch();

  onlineLayout->addLayout(verLayout);
  onlineLayout->addLayout(onlineBtnLayout);

  // --- Manual Upgrade Group ---
  auto *manualGroup = new QGroupBox(tr("Manual Upgrade"));
  auto *manualLayout = new QVBoxLayout(manualGroup);

  auto *manualInputLayout = new QHBoxLayout();
  m_upgradePathEdit = new QLineEdit();
  m_upgradePathEdit->setPlaceholderText(tr("Path to upgrade.uzip"));
  m_upgradeBrowseBtn = new QPushButton(tr("Browse..."));
  manualInputLayout->addWidget(new QLabel(tr("File:")));
  manualInputLayout->addWidget(m_upgradePathEdit);
  manualInputLayout->addWidget(m_upgradeBrowseBtn);

  auto *manualActionLayout = new QHBoxLayout();
  m_flashLocalBtn = new QPushButton(tr("Flash Local File"));
  manualActionLayout->addWidget(m_flashLocalBtn);
  manualActionLayout->addStretch();

  manualLayout->addLayout(manualInputLayout);
  manualLayout->addLayout(manualActionLayout);

  upgradeLayout->addWidget(onlineGroup);
  upgradeLayout->addWidget(manualGroup);
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
  connect(m_downloadFlashBtn, &QPushButton::clicked, this, &Esp32HidToolsWidget::onDownloadAndFlashFactory);
  connect(m_copyInfoBtn, &QPushButton::clicked, this, &Esp32HidToolsWidget::onCopyInfo);

  connect(m_upgradeBrowseBtn, &QPushButton::clicked, this, &Esp32HidToolsWidget::onBrowseUpgrade);
  connect(m_checkUpgradeBtn, &QPushButton::clicked, this, &Esp32HidToolsWidget::onCheckUpgrade);
  connect(m_flashOnlineBtn, &QPushButton::clicked, this, &Esp32HidToolsWidget::onFlashOnline);
  connect(m_flashLocalBtn, &QPushButton::clicked, this, &Esp32HidToolsWidget::onFlashLocal);

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
      QFileDialog::getOpenFileName(this, tr("Select Factory Package"), QString(), tr("Factory Package (*.fzip)"));
  if (!path.isEmpty()) {
    m_factoryPathEdit->setText(path);
  }
}

void Esp32HidToolsWidget::onBrowseUpgrade()
{
  QString path =
      QFileDialog::getOpenFileName(this, tr("Select Upgrade Package"), QString(), tr("Upgrade Package (*.uzip)"));
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
  m_downloadFlashBtn->setEnabled(enabled);
  m_flashOnlineBtn->setEnabled(enabled);
  m_flashLocalBtn->setEnabled(enabled);
  m_factoryBrowseBtn->setEnabled(enabled);
  m_upgradeBrowseBtn->setEnabled(enabled);
  m_checkUpgradeBtn->setEnabled(enabled);
  m_refreshPortsBtn->setEnabled(enabled);
  m_portCombo->setEnabled(enabled);
  m_copyInfoBtn->setEnabled(enabled);
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

  std::vector<uint8_t> data;
  if (!path.isEmpty()) {
    data = readFile(path);
    if (data.empty()) {
      log(tr("Error: Failed to read file or file is empty: %1").arg(path));
      // We don't return here yet, because we might not need the file if we are just verifying factory mode.
      // But if we do need it later, we will check data.empty().
    }
  }

  std::string port = portName.toStdString();

  log(tr("Checking device status..."));

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

    // Pre-check: Handshake must FAIL for Factory Flash (Device must be in valid Bootloader mode, not Firmware mode)
    {
      deskflow::bridge::CdcTransport cdc(QString::fromStdString(port));
      // Use permissive open to detect if device is in firmware mode (any FW)
      if (cdc.open(true)) {
        // Handshake succeeded -> We are in Firmware Mode!
        // Check if it is Factory Mode
        const auto &config = cdc.deviceConfig();
        if (config.firmwareMode == deskflow::bridge::FirmwareMode::Factory) {
          QMetaObject::invokeMethod(this, [this]() {
            log(tr("Device is already running with factory firmware."));
            log(tr("Attempting to fetch PDEK..."));
          });

          // Attempt to fetch PDEK using the new tool command
          std::string pdekInfo;
          // IMPORTANT: Close the local handle before asking the tool to open it again!
          cdc.close();
          FlashResult res = copy_pdek(port.c_str(), pdekInfo, log_cb);

          QMetaObject::invokeMethod(this, [this, res, pdekInfo]() {
            if (res == FlashResult::OK) {
              log(tr("PDEK fetched successfully."));
              log(tr("Device Info: %1").arg(QString::fromStdString(pdekInfo)));
              m_copyInfoBtn->setEnabled(true);
              m_copyInfoBtn->setProperty("deviceInfo", QString::fromStdString(pdekInfo));
              QMessageBox::information(
                  this, tr("Info"), tr("Device is already running with factory firmware. Device Info has been fetched.")
              );
            } else {
              log(tr("Failed to fetch PDEK from factory mode device."));
              QMessageBox::warning(
                  this, tr("Info"), tr("Device is already running with factory firmware, but failed to fetch PDEK.")
              );
            }
          });
          return;
        } else {
          // App Mode
          QMetaObject::invokeMethod(this, [this]() {
            log(tr("Device is running Application Firmware."));
            QMessageBox::warning(
                this, tr("Wrong Mode"),
                tr("Device is running Application Firmware. Please enter Bootloader mode to flash Factory Firmware.")
            );
          });
          return;
        }
      }
      // If open() failed, we assume it's because the device is in Bootloader mode (no handshake support)
      // and proceed to try flashing.
    }

    // Note: This calls the blocking C++ API
    if (data.empty()) {
      QMetaObject::invokeMethod(this, [this]() {
        log(tr("Error: No factory package selected."));
        QMessageBox::warning(this, tr("Error"), tr("Please select a factory package file to flash."));
      });
      return;
    }
    log(tr("Starting Factory Flash..."));
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

void Esp32HidToolsWidget::onDownloadAndFlashFactory()
{
  QString portName = m_portCombo->currentData().toString();
  if (portName.isEmpty()) {
    portName = m_portCombo->currentText();
    if (portName.isEmpty() || portName == tr("No devices found")) {
      QMessageBox::warning(this, tr("Error"), tr("Please select a valid serial port."));
      return;
    }
  }

  log(tr("Starting Download & Flash process..."));
  std::string port = portName.toStdString();

  auto task = [this, port]() {
    // 1. Download
    GithubDownloader downloader("fs34a", "deskflow-hid-release");
    QMetaObject::invokeMethod(this, [this]() { log(tr("Downloading flash_payloads.fzip...")); });

    // Attempt download (in-memory)
    auto dataOpt = downloader.download_asset_to_memory("flash_payloads.fzip");

    if (!dataOpt.has_value()) {
      QMetaObject::invokeMethod(this, [this]() {
        log(tr("Download failed."));
        QMessageBox::critical(this, tr("Error"), tr("Failed to download firmware. Check internet connection."));
      });
      return;
    }

    std::vector<uint8_t> data = dataOpt.value();
    QMetaObject::invokeMethod(this, [this, size = data.size()]() {
      log(tr("Download complete. Size: %1 bytes").arg(size));
      log(tr("Starting flashing process..."));
    });

    // 2. Flash
    std::string info;
    auto log_cb = [this](const std::string &msg) {
      QMetaObject::invokeMethod(this, [this, msg]() { log(QString::fromStdString(msg)); });
    };

    auto progress_cb = [this](size_t written, size_t total, size_t address) {
      static int last_percent = -1;
      int percent = (int)(((float)written / total) * 100);
      if (percent % 10 == 0 && percent != last_percent) {
        last_percent = percent;
        QString msg = QString("Flashing... %1%").arg(percent);
        QMetaObject::invokeMethod(this, [this, msg]() { log(msg); });
      }
    };

    // Pre-check handshake
    {
      deskflow::bridge::CdcTransport cdc(QString::fromStdString(port));
      if (cdc.open(true)) {
        const auto &config = cdc.deviceConfig();
        if (config.firmwareMode == deskflow::bridge::FirmwareMode::Factory) {
          QMetaObject::invokeMethod(this, [this]() {
            log(tr("Device is already in Factory Mode."));
            QMessageBox::warning(this, tr("Info"), tr("Device is already running factory firmware."));
          });
          return;
        } else {
          QMetaObject::invokeMethod(this, [this]() {
            log(tr("Device is in App Mode. Please enter Bootloader Mode."));
            QMessageBox::warning(this, tr("Wrong Mode"), tr("Please enter Bootloader mode to flash Factory Firmware."));
          });
          return;
        }
      }
    }

    FlashResult res = flash_factory(port, data, info, progress_cb, log_cb);

    QMetaObject::invokeMethod(this, [this, res, info]() {
      if (res == FlashResult::OK) {
        log(tr("Flash Success!"));
        log(tr("Device Info: %1").arg(QString::fromStdString(info)));
        m_copyInfoBtn->setEnabled(true);
        m_copyInfoBtn->setProperty("deviceInfo", QString::fromStdString(info));
        QMessageBox::information(this, tr("Success"), tr("Firmware downloaded and flashed successfully!"));
      } else {
        log(tr("Flash Failed! Error code: %1").arg(static_cast<int>(res)));
        QMessageBox::critical(this, tr("Error"), tr("Flashing failed. Error code: %1").arg(static_cast<int>(res)));
      }
    });
  };

  runBackgroundTask(task);
}

void Esp32HidToolsWidget::onCopyInfo()
{
  QString info = m_copyInfoBtn->property("deviceInfo").toString();
  if (!info.isEmpty()) {
    QApplication::clipboard()->setText(info);
    log(tr("Device Info copied to clipboard."));
    return;
  }

  // Info not in property, try to fetch it
  QString portName = m_portCombo->currentData().toString();
  if (portName.isEmpty()) {
    portName = m_portCombo->currentText();
    if (portName.isEmpty() || portName == tr("No devices found")) {
      QMessageBox::warning(this, tr("Error"), tr("Please select a valid serial port."));
      return;
    }
  }

  std::string port = portName.toStdString();
  log(tr("Checking device info on %1...").arg(portName));

  // 1. Handshake to check mode
  deskflow::bridge::CdcTransport cdc(portName);
  if (cdc.open(true)) {
    if (cdc.hasDeviceConfig()) {
      const auto &config = cdc.deviceConfig();
      if (config.firmwareMode == deskflow::bridge::FirmwareMode::Factory) {
        log(tr("Device is in Factory Mode. Fetching PDEK..."));
        cdc.close(); // Close to allow tool to access

        std::string pdekInfo;
        auto log_cb = [this](const std::string &msg) { log(QString::fromStdString(msg)); };

        FlashResult res = copy_pdek(port, pdekInfo, log_cb);
        if (res == FlashResult::OK) {
          m_copyInfoBtn->setProperty("deviceInfo", QString::fromStdString(pdekInfo));
          QApplication::clipboard()->setText(QString::fromStdString(pdekInfo));
          log(tr("Device Info fetched and copied to clipboard."));
          QMessageBox::information(this, tr("Success"), tr("Device Info copied to clipboard."));
        } else {
          log(tr("Failed to fetch PDEK."));
          QMessageBox::critical(this, tr("Error"), tr("Failed to fetch Device Info."));
        }
      } else {
        log(tr("Device is not in Factory Mode (Mode: %1).").arg((int)config.firmwareMode));
        QMessageBox::warning(this, tr("Wrong Mode"), tr("Device must be in Factory Mode to copy Device Info."));
      }
    } else {
      log(tr("Handshake complete but no config received."));
      QMessageBox::warning(this, tr("Error"), tr("Device handshake incomplete."));
    }
  } else {
    log(tr("Failed to open device or handshake failed."));
    QMessageBox::warning(this, tr("Connection Error"), tr("Failed to connect to device."));
  }
}

// Check Upgrade
void Esp32HidToolsWidget::onCheckUpgrade()
{
  QString portName = m_portCombo->currentData().toString();
  if (portName.isEmpty()) {
    portName = m_portCombo->currentText();
    if (portName.isEmpty() || portName == tr("No devices found")) {
      QMessageBox::warning(this, tr("Error"), tr("Please select a valid serial port."));
      return;
    }
  }

  log(tr("Checking for upgrades..."));
  std::string port = portName.toStdString();

  auto task = [this, port]() {
    // Check for "fs34a" / "deskflow-hid-release"
    GithubDownloader downloader("fs34a", "deskflow-hid-release");
    auto latestTag = downloader.get_latest_tag();
    if (latestTag.empty()) {
      QMetaObject::invokeMethod(this, [this]() {
        log(tr("Failed to fetch latest version tag from GitHub."));
        QMessageBox::warning(this, tr("Network Error"), tr("Could not check for updates."));
      });
      return;
    }

    // Connect to device to get current version
    QString portName = m_portCombo->currentData().toString();
    if (portName.isEmpty()) {
      portName = m_portCombo->currentText();
    }

    QString deviceVer = "Unknown";
    bool deviceAvailable = false;
    uint8_t rawBcd = 0;

    deskflow::bridge::CdcTransport cdc(portName);
    if (cdc.open()) {
      if (cdc.hasDeviceConfig()) {
        auto config = cdc.deviceConfig();
        rawBcd = config.firmwareVersionBcd;
        // Interpret as decimal: 101 -> 1.0.1, 200 -> 2.0.0
        int major = rawBcd / 100;
        int minor = (rawBcd % 100) / 10;
        int patch = rawBcd % 10;
        deviceVer = QString("v%1.%2.%3").arg(major).arg(minor).arg(patch);
        deviceAvailable = true;
      } else {
        QMetaObject::invokeMethod(this, [this]() {
          log(tr("Device handshake failed or no config received. Assuming factory/unknown state."));
        });
      }
    } else {
      QMetaObject::invokeMethod(this, [this]() { log(tr("Could not open device. Assuming factory/unknown state.")); });
    }

    // Log info for debugging
    QMetaObject::invokeMethod(this, [this, latestTag, deviceVer, rawBcd]() {
      log(tr("Version Check: Remote Tag='%1', Device Version='%2' (Raw BCD=%3)")
              .arg(QString::fromStdString(latestTag))
              .arg(deviceVer)
              .arg(rawBcd));
    });

    // Parse versions
    auto parseVersion = [](const std::string &v) {
      int major = 0, minor = 0, patch = 0;
      if (v.empty() || v == "Unknown")
        return std::make_tuple(0, 0, 0);

      size_t start = (v[0] == 'v') ? 1 : 0;
      // Find first dot
      size_t dot1 = v.find('.', start);
      if (dot1 != std::string::npos) {
        try {
          major = std::stoi(v.substr(start, dot1 - start));
        } catch (...) {
        }
        size_t dot2 = v.find('.', dot1 + 1);
        if (dot2 != std::string::npos) {
          try {
            minor = std::stoi(v.substr(dot1 + 1, dot2 - (dot1 + 1)));
          } catch (...) {
          }
          try {
            patch = std::stoi(v.substr(dot2 + 1));
          } catch (...) {
          }
        } else {
          try {
            minor = std::stoi(v.substr(dot1 + 1));
          } catch (...) {
          }
        }
      } else {
        try {
          major = std::stoi(v.substr(start));
        } catch (...) {
        }
      }
      return std::make_tuple(major, minor, patch);
    };

    auto [tagMajor, tagMinor, tagPatch] = parseVersion(latestTag);
    auto [devMajor, devMinor, devPatch] = parseVersion(deviceVer.toStdString());

    QMetaObject::invokeMethod(this, [=, this]() {
      m_lblCurrentVersion->setText(tr("Current Version: %1").arg(deviceVer));
      m_lblLatestVersion->setText(tr("Latest Version: %1").arg(QString::fromStdString(latestTag)));

      bool updateAvailable = false;

      // If device is not available or unknown, we assume update is possible/needed
      if (!deviceAvailable || deviceVer == "Unknown") {
        updateAvailable = true;
      } else {
        if (tagMajor > devMajor)
          updateAvailable = true;
        else if (tagMajor == devMajor && tagMinor > devMinor)
          updateAvailable = true;
        else if (tagMajor == devMajor && tagMinor == devMinor && tagPatch > devPatch)
          updateAvailable = true;
      }

      if (updateAvailable) {
        if (deviceVer != "Unknown") {
          log(tr("Update available (%1 > %2).").arg(QString::fromStdString(latestTag)).arg(deviceVer));
          QMessageBox::information(
              this, tr("Update Available"),
              tr("A new version (%1) is available.").arg(QString::fromStdString(latestTag))
          );
        } else {
          log(tr("Update available (Device version unknown)."));
          QMessageBox::information(
              this, tr("Update Available"),
              tr("A new version (%1) is available. Device version unknown.").arg(QString::fromStdString(latestTag))
          );
        }
        m_flashOnlineBtn->setEnabled(true);
      } else {
        log(tr("Device is up to date."));
        QMessageBox::information(this, tr("Up to date"), tr("Device is already running the latest version."));
        m_flashOnlineBtn->setEnabled(true); // DEBUG: Allow upgrade even if up-to-date
      }
    });
  };

  runBackgroundTask(task);
}

void Esp32HidToolsWidget::onFlashLocal()
{
  QString path = m_upgradePathEdit->text();
  if (path.isEmpty()) {
    QMessageBox::warning(this, tr("Error"), tr("Please select a firmware file."));
    return;
  }

  std::vector<uint8_t> data = readFile(path);
  if (data.empty()) {
    QMessageBox::critical(this, tr("Error"), tr("Failed to read firmware file."));
    return;
  }

  log(tr("Flashing local file: %1").arg(path));
  flashFirmware(data);
}

void Esp32HidToolsWidget::onFlashOnline()
{
  log(tr("Downloading upgrade firmware..."));

  // We need to fetch data first, then flash.
  // We can reuse runBackgroundTask for download + flash chain.

  auto task = [this]() {
    GithubDownloader downloader("fs34a", "deskflow-hid-release");
    auto dl = downloader.download_first_asset_by_suffix_to_memory(".uzip");

    QMetaObject::invokeMethod(this, [this, dl]() {
      if (!dl.has_value()) {
        log(tr("Download failed. No .uzip asset found."));
        QMessageBox::critical(this, tr("Error"), tr("Failed to download firmware."));
        return;
      }
      log(tr("Downloaded available firmware. Starting flash..."));
      // Since flashFirmware expects data, we need to pass it.
      // But flashFirmware starts another BG task. We are already in one?
      // Actually runBackgroundTask just starts a future.
      // Ideally we shouldn't nest runBackgroundTask calls from within existing task context *if* it blocks UI.
      // But here we are invoking back to main thread.
      flashFirmware(dl.value());
    });
  };

  runBackgroundTask(task);
}

void Esp32HidToolsWidget::flashFirmware(const std::vector<uint8_t> &firmwareData)
{
  QString portName = m_portCombo->currentData().toString();
  if (portName.isEmpty()) {
    portName = m_portCombo->currentText();
  }

  // Local copy to capture by value
  std::vector<uint8_t> dataCopy = firmwareData;

  auto task = [this, portName, dataCopy]() mutable {
    auto log_cb_func = [this](const std::string &msg) {
      QMetaObject::invokeMethod(this, [this, msg]() { this->log(QString::fromStdString(msg)); });
    };

    std::string portStd = portName.toStdString();
    deskflow::bridge::CdcTransport cdc(portName);

    // 2. Switch to Factory if needed
    if (!cdc.open()) {
      // Proceed or fail?
      // If we can't open, we might fail to check mode.
      // But maybe we can proceed with upgrade assuming it might be in download mode?
      // No, upgrade requires app or factory mode.
    }

    // Check mode
    // ... reused logic ...
    bool inFactory = false;
    if (cdc.isOpen()) {
      if (cdc.hasDeviceConfig() && cdc.deviceConfig().firmwareMode == deskflow::bridge::FirmwareMode::Factory) {
        inFactory = true;
      } else {
        // App mode, try switching
        if (cdc.gotoFactory()) {
          log_cb_func("Sent gotoFactory command. Waiting for device to reboot...");
          cdc.close(); // Close to allow re-enumeration

          // Poll for reconnection
          bool reconnected = false;
          for (int i = 0; i < 30; i++) { // 15 seconds
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            // Try open - probe needs QString
            deskflow::bridge::CdcTransport probe(portName);
            if (probe.open(true)) {
              if (probe.hasDeviceConfig() &&
                  probe.deviceConfig().firmwareMode == deskflow::bridge::FirmwareMode::Factory) {
                reconnected = true;
                break;
              }
            }
          }

          if (reconnected) {
            log_cb_func("Device re-connected in Factory Mode.");
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
          } else {
            log_cb_func("Warning: Timed out waiting for device to reconnect.");
          }
        }
      }
    }

    cdc.close(); // Validate flash tool opens it fresh

    // 3. Flash
    auto progress_cb = [this](size_t written, size_t total, size_t address) {
      static int last_percent = -1;
      int percent = total > 0 ? (int)(((float)written / total) * 100) : 0;
      if (percent != last_percent && percent % 10 == 0) {
        QMetaObject::invokeMethod(this, [this, percent]() { log(tr("Flashing... %1%").arg(percent)); });
        last_percent = percent;
      }
    };

    try {
      FlashResult res = flash_upgrade(portStd, dataCopy, progress_cb, log_cb_func);
      if (res == FlashResult::OK) {
        log_cb_func("Upgrade flash complete.");
        QMetaObject::invokeMethod(this, [this]() {
          QMessageBox::information(this, tr("Success"), tr("Firmware upgrade successful."));
        });
      } else {
        log_cb_func(std::string("Flash failed with error code: ") + std::to_string((int)res));
        QMetaObject::invokeMethod(this, [this, res]() {
          QMessageBox::critical(this, tr("Error"), tr("Flash failed. Error code: %1").arg((int)res));
        });
      }
    } catch (const std::exception &e) {
      log_cb_func(std::string("Flash failed: ") + e.what());
      QMetaObject::invokeMethod(this, [this, e]() {
        QMessageBox::critical(this, tr("Error"), tr("Flash failed: %1").arg(e.what()));
      });
    }
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
    // Use permissive open for status check (Factory FW might not support auth)
    bool openSuccess = cdc.open(true);

    struct State
    {
      QString serial;
      QString activationState;
      bool isActivated;
      bool isFactoryMode;
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
      result.isFactoryMode = (config.firmwareMode == deskflow::bridge::FirmwareMode::Factory);
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

        if (result.isFactoryMode) {
          m_labelActivationState->setText(tr("State: Factory Mode (Cannot Activate)"));
          m_groupActivationInput->setVisible(false);
          log(tr("Device State Refreshed. Serial: %1, Mode: Factory").arg(result.serial));
        } else {
          // Conditional UI: Hide activation input if already activated
          m_groupActivationInput->setVisible(!result.isActivated);
          log(tr("Device State Refreshed. Serial: %1, State: %2").arg(result.serial, result.activationState));
        }
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
    // Activation REQUIRES secure connection.
    if (cdc.open(false) && cdc.activateDevice(key.toStdString())) {
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
