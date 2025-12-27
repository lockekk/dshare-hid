/*
 * Deskflow-hid -- created by locke.huang@gmail.com
 */

#include "Esp32HidToolsWidget.h"
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QScrollBar>
#include <QUrl>
#include <QUrlQuery>
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

  m_tabWidget = new QTabWidget(this);

  // --- Factory Tab ---
  auto *factoryTab = new QWidget();
  auto *factoryLayout = new QVBoxLayout(factoryTab);

  // --- Online Factory Group ---
  auto *onlineFactoryGroup = new QGroupBox(tr("Online"));
  auto *onlineFactoryLayout = new QVBoxLayout(onlineFactoryGroup);

  m_downloadFlashBtn = new QPushButton(tr("Flash"));
  auto *onlineFactoryBtnLayout = new QHBoxLayout();
  onlineFactoryBtnLayout->addWidget(m_downloadFlashBtn);
  onlineFactoryBtnLayout->addStretch();
  onlineFactoryLayout->addLayout(onlineFactoryBtnLayout);

  // --- Manual Factory Group ---
  auto *manualFactoryGroup = new QGroupBox(tr("Manual"));
  auto *manualFactoryLayout = new QVBoxLayout(manualFactoryGroup);

  auto *factoryInputLayout = new QHBoxLayout();
  m_factoryPathEdit = new QLineEdit();
  m_factoryPathEdit->setPlaceholderText(tr("Path to factory.fzip"));
  m_factoryBrowseBtn = new QPushButton(tr("Browse..."));
  factoryInputLayout->addWidget(new QLabel(tr("File:")));
  factoryInputLayout->addWidget(m_factoryPathEdit);
  factoryInputLayout->addWidget(m_factoryBrowseBtn);

  m_factoryFlashBtn = new QPushButton(tr("Flash"));

  auto *manualBtnLayout = new QHBoxLayout();
  manualBtnLayout->addWidget(m_factoryFlashBtn);
  manualBtnLayout->addStretch();

  manualFactoryLayout->addLayout(factoryInputLayout);
  manualFactoryLayout->addLayout(manualBtnLayout);

  factoryLayout->addWidget(onlineFactoryGroup);
  factoryLayout->addWidget(manualFactoryGroup);
  factoryLayout->addStretch();

  // Common bottom controls
  m_copyInfoBtn = new QPushButton(tr("Copy Device Secret"));
  factoryLayout->addWidget(m_copyInfoBtn);

  m_tabWidget->addTab(factoryTab, tr("Factory Mode"));

  // --- Upgrade Tab ---
  auto *upgradeTab = new QWidget();
  auto *upgradeLayout = new QVBoxLayout(upgradeTab);

  // --- Online Upgrade Group ---
  auto *onlineGroup = new QGroupBox(tr("Online"));
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
  m_flashOnlineBtn = new QPushButton(tr("Flash"));
  m_flashOnlineBtn->setEnabled(false); // Enabled after check
  onlineBtnLayout->addWidget(m_checkUpgradeBtn);
  onlineBtnLayout->addWidget(m_flashOnlineBtn);
  onlineBtnLayout->addStretch();

  onlineLayout->addLayout(verLayout);
  onlineLayout->addLayout(onlineBtnLayout);

  // --- Manual Upgrade Group ---
  auto *manualGroup = new QGroupBox(tr("Manual"));
  auto *manualLayout = new QVBoxLayout(manualGroup);

  auto *manualInputLayout = new QHBoxLayout();
  m_upgradePathEdit = new QLineEdit();
  m_upgradePathEdit->setPlaceholderText(tr("Path to upgrade.uzip"));
  m_upgradeBrowseBtn = new QPushButton(tr("Browse..."));
  manualInputLayout->addWidget(new QLabel(tr("File:")));
  manualInputLayout->addWidget(m_upgradePathEdit);
  manualInputLayout->addWidget(m_upgradeBrowseBtn);

  auto *manualActionLayout = new QHBoxLayout();
  m_flashLocalBtn = new QPushButton(tr("Flash"));
  manualActionLayout->addWidget(m_flashLocalBtn);
  manualActionLayout->addStretch();

  manualLayout->addLayout(manualInputLayout);
  manualLayout->addLayout(manualActionLayout);

  upgradeLayout->addWidget(onlineGroup);
  upgradeLayout->addWidget(manualGroup);
  upgradeLayout->addStretch();

  m_tabWidget->addTab(upgradeTab, tr("Upgrade Mode"));

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

  m_tabWidget->addTab(activationTab, tr("Activation"));

  // --- Order Tab ---
  auto *orderTab = new QWidget();
  auto *orderLayout = new QVBoxLayout(orderTab);

  // User Info
  auto *userInfoGroup = new QGroupBox(tr("User Information"));
  auto *userInfoLayout = new QGridLayout(userInfoGroup);
  m_orderName = new QLineEdit();
  m_orderEmail = new QLineEdit();
  userInfoLayout->addWidget(new QLabel(tr("Name:")), 0, 0);
  userInfoLayout->addWidget(m_orderName, 0, 1);
  userInfoLayout->addWidget(new QLabel(tr("Email:")), 1, 0);
  userInfoLayout->addWidget(m_orderEmail, 1, 1);

  // Options
  auto *optionsGroup = new QGroupBox(tr("Order Options"));
  auto *optionsLayout = new QVBoxLayout(optionsGroup);
  m_orderOption1 = new QRadioButton(tr("Request 7-Day Free Trial"));
  m_orderOption2 = new QRadioButton(tr("Purchase Full License"));
  m_orderOption3 = new QRadioButton(tr("Skip Trial & Purchase Full License"));
  m_orderOption4 = new QRadioButton(tr("Upgrade Profile Capacity"));
  optionsLayout->addWidget(m_orderOption1);
  optionsLayout->addWidget(m_orderOption2);
  optionsLayout->addWidget(m_orderOption3);
  optionsLayout->addWidget(m_orderOption3);
  optionsLayout->addWidget(m_orderOption4);

  // Payments Group
  auto *paymentsGroup = new QGroupBox(tr("Payments"));
  auto *paymentsLayout = new QGridLayout(paymentsGroup);

  // Cost
  m_lblPaymentDetails = new QLabel(tr("Payment Details: Free Trial ($0.00)"));
  QFont fontPayment = m_lblPaymentDetails->font();
  fontPayment.setBold(true);
  m_lblPaymentDetails->setFont(fontPayment);

  // Reference NO
  m_paymentRefNo = new QLineEdit();
  m_paymentRefNo->setReadOnly(true);
  m_paymentRefNo->setToolTip(tr("Please put this Reference No. in your PayPal payment message/note."));

  // Transaction ID
  m_paymentTransId = new QLineEdit();
  m_paymentTransId->setPlaceholderText(tr("Paste your PayPal Transaction ID here"));

  // Configured via build system (CMake)
#ifndef DESKFLOW_PAYPAL_ACCOUNT
#define DESKFLOW_PAYPAL_ACCOUNT "sb-uqhcf48362835@business.example.com"
#endif

  m_lblPaymentOwner =
      new QLabel(QString(tr("Paypal Seller: <b><font color='red'>%1</font></b>")).arg(DESKFLOW_PAYPAL_ACCOUNT));
  m_lblPaymentOwner->setTextInteractionFlags(Qt::TextSelectableByMouse); // Enable copy

  paymentsLayout->addWidget(m_lblPaymentDetails, 0, 0, 1, 2); // Span 2 cols
  paymentsLayout->addWidget(m_lblPaymentOwner, 1, 0, 1, 2);   // Span 2 cols
  paymentsLayout->addWidget(new QLabel(tr("Reference NO.:")), 2, 0);
  paymentsLayout->addWidget(m_paymentRefNo, 2, 1);

  paymentsLayout->addWidget(new QLabel(tr("PayPal Transaction ID:")), 3, 0);
  paymentsLayout->addWidget(m_paymentTransId, 3, 1);

  m_btnPayNow = new QPushButton(tr("Pay Now (Secure PayPal Link)"));
  paymentsLayout->addWidget(m_btnPayNow, 4, 0, 1, 2);

  // Device Info
  auto *deviceInfoGroup = new QGroupBox(tr("Device Information"));
  auto *deviceInfoLayout = new QGridLayout(deviceInfoGroup);
  m_orderDeviceSecret = new QLineEdit();
  m_orderDeviceSecret->setReadOnly(true);
  m_orderSerialLabel = new QLabel(tr("-"));
  m_orderSerialLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_orderTotalProfiles = new QComboBox();
  m_orderTotalProfiles->addItem("2", 2);
  m_orderTotalProfiles->addItem("4", 4);
  m_orderTotalProfiles->addItem("6", 6);

  deviceInfoLayout->addWidget(new QLabel(tr("Device Secret:")), 0, 0);
  deviceInfoLayout->addWidget(m_orderDeviceSecret, 0, 1);
  deviceInfoLayout->addWidget(new QLabel(tr("Serial Number:")), 1, 0);
  deviceInfoLayout->addWidget(m_orderSerialLabel, 1, 1);
  deviceInfoLayout->addWidget(new QLabel(tr("Total Profiles:")), 2, 0);
  deviceInfoLayout->addWidget(m_orderTotalProfiles, 2, 1);

  m_btnGenerateOrder = new QPushButton(tr("Generate Request File"));
  m_btnCopyOrder = new QPushButton(tr("Copy content"));
  m_btnEmailOrder = new QPushButton(tr("Email"));

  auto *orderButtonLayout = new QHBoxLayout();
  orderButtonLayout->addWidget(m_btnGenerateOrder);
  orderButtonLayout->addWidget(m_btnCopyOrder);
  orderButtonLayout->addWidget(m_btnEmailOrder);

  orderLayout->addWidget(userInfoGroup);
  orderLayout->addWidget(optionsGroup);
  orderLayout->addWidget(deviceInfoGroup);
  orderLayout->addWidget(paymentsGroup);
  orderLayout->addLayout(orderButtonLayout);
  orderLayout->addStretch();

  m_tabWidget->addTab(orderTab, tr("Order"));

  // --- Common Output ---
  m_logOutput = new QTextEdit();
  m_logOutput->setReadOnly(true);

  mainLayout->addWidget(m_tabWidget);
  mainLayout->addWidget(new QLabel(tr("Log Output:")));
  mainLayout->addWidget(m_logOutput);

  // Connect Signals
  connect(m_tabWidget, &QTabWidget::currentChanged, this, &Esp32HidToolsWidget::onTabChanged);

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
  connect(m_btnGenerateOrder, &QPushButton::clicked, this, &Esp32HidToolsWidget::onGenerateOrder);
  connect(m_btnCopyOrder, &QPushButton::clicked, this, &Esp32HidToolsWidget::onCopyOrderContent);
  connect(m_btnEmailOrder, &QPushButton::clicked, this, &Esp32HidToolsWidget::onEmailOrder);

  connect(m_refreshPortsBtn, &QPushButton::clicked, this, &Esp32HidToolsWidget::refreshPorts);
  connect(m_portCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Esp32HidToolsWidget::onPortChanged);

  connect(m_orderOption1, &QRadioButton::toggled, this, &Esp32HidToolsWidget::updatePaymentDetails);
  connect(m_orderOption2, &QRadioButton::toggled, this, &Esp32HidToolsWidget::updatePaymentDetails);
  connect(m_orderOption3, &QRadioButton::toggled, this, &Esp32HidToolsWidget::updatePaymentDetails);
  connect(m_orderOption4, &QRadioButton::toggled, this, &Esp32HidToolsWidget::updatePaymentDetails);
  connect(
      m_orderTotalProfiles, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
      &Esp32HidToolsWidget::updatePaymentDetails
  );

  // Update Reference No (Timestamp) - Call once or on specific triggers
  updatePaymentReference();

  connect(m_btnPayNow, &QPushButton::clicked, this, &Esp32HidToolsWidget::onPayNowClicked);

  updatePaymentDetails();
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
  QScrollBar *sb = m_logOutput->verticalScrollBar();
  sb->setValue(sb->maximum());
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
    log(tr("Reading factory firmware from: %1").arg(path));
    data = readFile(path);
    if (data.empty()) {
      log(tr("Error: Failed to read file or file is empty: %1").arg(path));
    }
  }

  if (!confirmFactoryFlash()) {
    return;
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
      int percent = total > 0 ? (int)(((float)written / total) * 100) : 0;
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

        showFactoryFlashSuccess();
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

  if (!confirmFactoryFlash()) {
    return;
  }

  log(tr("Starting Download & Flash process..."));
  std::string port = portName.toStdString();

  auto task = [this, port]() {
    // 1. Download
    GithubDownloader downloader("deskflow-hid", "deskflow-hid-release");
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
        showFactoryFlashSuccess();
      } else {
        log(tr("Flash Failed! Error code: %1").arg(static_cast<int>(res)));
        QMessageBox::critical(this, tr("Error"), tr("Flashing failed. Error code: %1").arg(static_cast<int>(res)));
      }
    });
  };

  runBackgroundTask(task);
}

bool Esp32HidToolsWidget::confirmFactoryFlash()
{
  const QString message = tr("This process permanently converts your ESP32 into a Deskflow-HID device. "
                             "This is irreversible and blocks non-Deskflow firmware.\n\n"
                             "Do you want to proceed?");

  QMessageBox::StandardButton reply =
      QMessageBox::question(this, tr("Confirm Factory Flash"), message, QMessageBox::Yes | QMessageBox::No);

  return (reply == QMessageBox::Yes);
}

void Esp32HidToolsWidget::showFactoryFlashSuccess()
{
  QMessageBox::information(
      this, tr("Success"),
      tr("Factory firmware flashed successfully.\n\n"
         "Next step: You need to flash the per-device firmware to use the device. "
         "Please switch to the 'Order' tab to request it.")
  );
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
    // Check for "deskflow-hid" / "deskflow-hid-release"
    GithubDownloader downloader("deskflow-hid", "deskflow-hid-release");
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

  log(tr("Reading local firmware from: %1").arg(path));
  log(tr("Flashing local file: %1").arg(path));
  flashFirmware(data);
}

void Esp32HidToolsWidget::onFlashOnline()
{
  log(tr("Downloading upgrade firmware..."));

  // We need to fetch data first, then flash.
  // We can reuse runBackgroundTask for download + flash chain.

  auto task = [this]() {
    GithubDownloader downloader("deskflow-hid", "deskflow-hid-release");
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
void Esp32HidToolsWidget::changeEvent(QEvent *event)
{
  QDialog::changeEvent(event);
  if (event->type() == QEvent::LanguageChange) {
    updateText();
  }
}

void Esp32HidToolsWidget::updateText()
{
  if (m_devicePath.isEmpty()) {
    setWindowTitle(tr("Firmware Flash Tool"));
  } else {
    setWindowTitle(tr("Firmware Flash Tool - %1").arg(m_devicePath));
  }

  m_refreshPortsBtn->setText(tr("Refresh"));

  m_tabWidget->setTabText(0, tr("Factory Mode"));
  m_tabWidget->setTabText(1, tr("Upgrade Mode"));
  m_tabWidget->setTabText(2, tr("Activation"));
  m_tabWidget->setTabText(3, tr("Order"));

  m_factoryPathEdit->setPlaceholderText(tr("Path to factory.fzip"));
  m_factoryBrowseBtn->setText(tr("Browse..."));
  m_factoryFlashBtn->setText(tr("Flash"));
  m_downloadFlashBtn->setText(tr("Flash"));
  m_copyInfoBtn->setText(tr("Copy Device Secret"));

  m_checkUpgradeBtn->setText(tr("Check for Updates"));
  m_flashOnlineBtn->setText(tr("Flash"));
  m_upgradePathEdit->setPlaceholderText(tr("Path to upgrade.uzip"));
  m_upgradeBrowseBtn->setText(tr("Browse..."));
  m_flashLocalBtn->setText(tr("Flash"));

  m_btnCopySerial->setText(tr("Copy Serial"));
  m_lineActivationKey->setPlaceholderText(tr("Paste Activation Key Here"));
  m_btnActivate->setText(tr("Activate"));

  m_orderOption1->setText(tr("Free trial for 7 days"));
  m_orderOption2->setText(tr("I am ok with free trial and want to buy full license"));
  m_orderOption3->setText(tr("Skip trial and buy Full licensed version"));
  m_orderOption4->setText(tr("Already licensed, but want bump profiles"));

  m_btnGenerateOrder->setText(tr("Generate Request File"));
  m_btnCopyOrder->setText(tr("Copy content"));
  m_btnEmailOrder->setText(tr("Email"));

  // Update dynamic labels that might have "Unknown" or similar markers
  m_lblCurrentVersion->setText(tr("Current Version: %1").arg(m_lblCurrentVersion->text().split(": ").last()));
  m_lblLatestVersion->setText(tr("Latest Version: %1").arg(m_lblLatestVersion->text().split(": ").last()));
  m_labelActivationState->setText(tr("State: %1").arg(m_labelActivationState->text().split(": ").last()));
}

} // namespace deskflow::gui

void deskflow::gui::Esp32HidToolsWidget::setupUI()
{
}

void deskflow::gui::Esp32HidToolsWidget::onTabChanged(int index)
{
  QTabWidget *tabs = qobject_cast<QTabWidget *>(sender());
  if (tabs && (tabs->tabText(index) == tr("Activation") || tabs->tabText(index) == tr("Order"))) {
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
  m_orderSerialLabel->setText(tr("-"));
  m_orderDeviceSecret->clear();
  m_labelActivationState->setText(tr("State: Checking..."));
  setControlsEnabled(false); // Disable while checking

  auto task = [this, portName]() {
    deskflow::bridge::CdcTransport cdc(portName);
    std::string serial;
    std::string pdek;
    // Use permissive open for status check (Factory FW might not support auth)
    bool openSuccess = cdc.open(true);

    struct State
    {
      QString serial;
      QString pdek;
      QString activationState;
      deskflow::bridge::ActivationState stateEnum;
      bool isActivated;
      bool isFactoryMode;
      uint8_t totalProfiles;
      bool success;
      QString error;
    } result;

    if (openSuccess) {
      if (cdc.fetchSerialNumber(serial)) {
        result.serial = QString::fromStdString(serial);
      }

      const auto &config = cdc.deviceConfig();
      result.activationState = QString::fromLatin1(config.activationStateString());
      result.stateEnum = config.activationState;
      result.isActivated = (config.activationState == deskflow::bridge::ActivationState::Activated);
      result.isFactoryMode = (config.firmwareMode == deskflow::bridge::FirmwareMode::Factory);
      result.totalProfiles = config.totalProfiles;
      result.success = true;

      if (result.isFactoryMode) {
        // Automatically attempt to fetch PDEK if in factory mode
        cdc.close(); // Close to allow tool to access
        std::string pdekInfo;
        auto log_cb = [this](const std::string &msg) {
          QMetaObject::invokeMethod(this, [this, msg]() { log(QString::fromStdString(msg)); });
        };
        FlashResult res = copy_pdek(portName.toStdString(), pdekInfo, log_cb);
        if (res == FlashResult::OK) {
          result.pdek = QString::fromStdString(pdekInfo);
        }
      }
    } else {
      result.success = false;
      result.error = QString::fromStdString(cdc.lastError());
    }

    QMetaObject::invokeMethod(this, [this, result]() {
      setControlsEnabled(true);
      if (result.success) {
        m_lineSerial->setText(result.serial);
        m_orderSerialLabel->setText(result.serial);
        m_labelActivationState->setText(tr("State: %1").arg(result.activationState));
        m_orderDeviceSecret->setText(result.pdek);

        // Update Order Options
        bool hasSecret = !result.pdek.isEmpty();
        bool isLicensed = result.isActivated;
        bool canBuyFull = !isLicensed && !hasSecret;

        m_orderOption1->setEnabled(hasSecret);
        m_orderOption3->setEnabled(hasSecret);
        m_orderOption2->setEnabled(canBuyFull);
        m_orderOption4->setEnabled(isLicensed && result.totalProfiles == 2);

        // Check the first valid (enabled) option
        if (m_orderOption1->isEnabled()) {
          m_orderOption1->setChecked(true);
        } else if (m_orderOption2->isEnabled()) {
          m_orderOption2->setChecked(true);
        } else if (m_orderOption3->isEnabled()) {
          m_orderOption3->setChecked(true);
        } else if (m_orderOption4->isEnabled()) {
          m_orderOption4->setChecked(true);
        }

        // Total Profiles selection logic
        m_orderTotalProfiles->setEnabled(result.totalProfiles == 2);

        if (result.isFactoryMode) {
          m_labelActivationState->setText(tr("State: Factory Mode (Cannot Activate)"));
          m_groupActivationInput->setVisible(false);
          log(tr("Device State Refreshed. Serial: %1, Mode: Factory, Secret: %2")
                  .arg(result.serial, result.pdek.isEmpty() ? "Unknown" : "Fetched"));
        } else {
          // Conditional UI: Hide activation input if already activated
          m_groupActivationInput->setVisible(!result.isActivated);
          log(tr("Device State Refreshed. Serial: %1, State: %2, Profiles: %3")
                  .arg(result.serial, result.activationState, QString::number(result.totalProfiles)));
        }
      } else {
        m_labelActivationState->setText(tr("State: Error"));
        log(tr("Failed to refresh state: %1").arg(result.error));

        // Erased Device Case: If on Order tab and handshake failed, warn user
        QTabWidget *tabs = findChild<QTabWidget *>();
        if (tabs && tabs->tabText(tabs->currentIndex()) == tr("Order")) {
          QMessageBox::warning(
              this, tr("Erased Device Detected"),
              tr("No valid firmware detected on the device. Please go to the 'Factory Mode' tab and flash 'Online' "
                 "first.")
          );
        }
        // Reset Order Options
        m_orderOption1->setEnabled(false);
        m_orderOption2->setEnabled(false);
        m_orderOption3->setEnabled(false);
        m_orderOption4->setEnabled(false);
        m_orderTotalProfiles->setEnabled(false);
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

QString deskflow::gui::Esp32HidToolsWidget::composeOrderContent(QString &outPrefix, int &outOption)
{
  QString name = m_orderName->text().trimmed();
  QString email = m_orderEmail->text().trimmed();

  if (name.isEmpty() || email.isEmpty()) {
    QMessageBox::warning(this, tr("Input Required"), tr("Please enter your name and email address."));
    return QString();
  }

  QString serial = m_orderSerialLabel->text();
  QString secret = m_orderDeviceSecret->text();
  int totalProfiles = m_orderTotalProfiles->currentData().toInt();

  if (serial == tr("-") || serial.isEmpty()) {
    QMessageBox::warning(this, tr("Device Error"), tr("Serial number missing. Please check device connection."));
    return QString();
  }

  outOption = -1;
  QString content;

  if (m_orderOption1->isChecked()) {
    outOption = 1;
    outPrefix = "free_trial_";
    content = QString("Name: %1\nEmail: %2\nSerial: %3\nDevice Secret: %4\nRequest: Free trial for 7 days\n")
                  .arg(name, email, serial, secret);
  } else if (m_orderOption2->isChecked()) {
    outOption = 2;
    outPrefix = "full_license_";
    content =
        QString("Name: %1\nEmail: %2\nSerial: %3\nTotal Profiles: %4\nRequest: Buy full license (Trial upgrade)\n")
            .arg(name, email, serial, QString::number(totalProfiles));
  } else if (m_orderOption3->isChecked()) {
    outOption = 3;
    outPrefix = "full_license_";
    content = QString("Name: %1\nEmail: %2\nSerial: %3\nDevice Secret: %4\nTotal Profiles: %5\nRequest: Skip trial "
                      "and buy full license\n")
                  .arg(name, email, serial, secret, QString::number(totalProfiles));
  } else if (m_orderOption4->isChecked()) {
    outOption = 4;
    outPrefix = "profile_";
    content =
        QString("Name: %1\nEmail: %2\nSerial: %3\nTotal Profiles: %4\nRequest: Bump profiles (Already licensed)\n")
            .arg(name, email, serial, QString::number(totalProfiles));
  } else {
    QMessageBox::warning(this, tr("Selection Required"), tr("Please select one of the order options."));
    return QString();
  }

  // Calculate Price using helper
  OrderPrice priceInfo = calculateOrderPrice(outOption, totalProfiles);
  double price = priceInfo.price;
  QString priceDesc = priceInfo.desc;

  // Append Payment Info
  content.append(QString("Payment Details: %1 = Total $%2 USD\n").arg(priceDesc, QString::number(price, 'f', 2)));
  content.append(QString("Paypal Seller: %1\n").arg(DESKFLOW_PAYPAL_ACCOUNT));

  // Validation: If price > 0, Transaction ID is required
  if (price > 0.01) {
    QString transId = m_paymentTransId->text().trimmed();
    if (transId.isEmpty()) {
      QMessageBox::warning(
          this, tr("Missing Transaction ID"), tr("Please enter your PayPal Transaction ID for verification.")
      );
      return QString();
    }
    content.append(QString("PayPal Transaction ID: %1\n").arg(transId));
  }

  content.append(QString("Reference No: %1\n").arg(m_paymentRefNo->text()));

  // Validate Secret for options 1 and 3
  if ((outOption == 1 || outOption == 3) && secret.isEmpty()) {
    QMessageBox::critical(
        this, tr("Missing Secret"),
        tr("Device Secret (PDEK) is required for this option. Please ensure the device is in Factory Mode and the "
           "secret has been fetched correctly.")
    );
    return QString();
  }

  return content;
}

deskflow::gui::OrderPrice deskflow::gui::Esp32HidToolsWidget::calculateOrderPrice(int option, int totalProfiles)
{
  double price = 0.0;
  QString priceDesc;

  if (option == 1) { // Free Trial
    price = 0.0;
    priceDesc = "Free Trial";
  } else if (option == 2 || option == 3) { // Full License
    price += DESKFLOW_PRICE_LICENSE;
    // Add-ons for profiles
    if (totalProfiles == 4)
      price += DESKFLOW_PRICE_PROFILE_4;
    else if (totalProfiles == 6)
      price += DESKFLOW_PRICE_PROFILE_6;

    priceDesc =
        QString("License($%1) + %2 Profiles(%3)")
            .arg(QString::number(DESKFLOW_PRICE_LICENSE, 'f', 2))
            .arg(totalProfiles)
            .arg(
                totalProfiles == 2
                    ? "$0.00"
                    : (totalProfiles == 4 ? QString("$%1").arg(QString::number(DESKFLOW_PRICE_PROFILE_4, 'f', 2))
                                          : QString("$%1").arg(QString::number(DESKFLOW_PRICE_PROFILE_6, 'f', 2)))
            );
  } else if (option == 4) { // Only Bump Profiles
    // Base price 0, only profile cost
    if (totalProfiles == 4)
      price += DESKFLOW_PRICE_PROFILE_4;
    else if (totalProfiles == 6)
      price += DESKFLOW_PRICE_PROFILE_6;

    priceDesc =
        QString("Bump Profiles to %1 (%2)")
            .arg(totalProfiles)
            .arg(
                totalProfiles == 2
                    ? "$0.00"
                    : (totalProfiles == 4 ? QString("$%1").arg(QString::number(DESKFLOW_PRICE_PROFILE_4, 'f', 2))
                                          : QString("$%1").arg(QString::number(DESKFLOW_PRICE_PROFILE_6, 'f', 2)))
            );
  }

  return {price, priceDesc};
}

void deskflow::gui::Esp32HidToolsWidget::updatePaymentDetails()
{
  int option = -1;
  if (m_orderOption1->isChecked())
    option = 1;
  else if (m_orderOption2->isChecked())
    option = 2;
  else if (m_orderOption3->isChecked())
    option = 3;
  else if (m_orderOption4->isChecked())
    option = 4;

  int totalProfiles = m_orderTotalProfiles->currentData().toInt();
  OrderPrice priceInfo = calculateOrderPrice(option, totalProfiles);

  if (option == -1) {
    m_lblPaymentDetails->setText("Select an option");
    return;
  }

  m_lblPaymentDetails->setText(
      QString("Payment Details: %1 = Total $%2 USD").arg(priceInfo.desc, QString::number(priceInfo.price, 'f', 2))
  );
}

void deskflow::gui::Esp32HidToolsWidget::updatePaymentReference()
{
  // Use UTC timestamp as requested (Cross-platform)
  qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
  m_paymentRefNo->setText(QString::number(timestamp));
}

void deskflow::gui::Esp32HidToolsWidget::onPayNowClicked()
{
  int option = -1;
  if (m_orderOption1->isChecked())
    option = 1;
  else if (m_orderOption2->isChecked())
    option = 2;
  else if (m_orderOption3->isChecked())
    option = 3;
  else if (m_orderOption4->isChecked())
    option = 4;

  int totalProfiles = m_orderTotalProfiles->currentData().toInt();
  OrderPrice priceInfo = calculateOrderPrice(option, totalProfiles);
  double price = priceInfo.price;
  QString priceDesc = priceInfo.desc;

  if (price <= 0.01) {
    QMessageBox::information(this, tr("Free"), tr("No payment is required for this option."));
    return;
  }

  // Construct PayPal URL
  // Configured via build system (CMake)
#ifndef DESKFLOW_PAYPAL_URL
#define DESKFLOW_PAYPAL_URL "https://www.sandbox.paypal.com/cgi-bin/webscr"
#endif

#ifndef DESKFLOW_PAYPAL_ACCOUNT
#define DESKFLOW_PAYPAL_ACCOUNT "sb-uqhcf48362835@business.example.com"
#endif

  QString baseUrl = DESKFLOW_PAYPAL_URL;
  QString business = DESKFLOW_PAYPAL_ACCOUNT;

  QString refNo = m_paymentRefNo->text();
  if (refNo.isEmpty()) {
    updatePaymentReference();
    refNo = m_paymentRefNo->text();
  }

  QString itemName = QString("Deskflow-HID: %1").arg(priceDesc);

  QUrlQuery query;
  query.addQueryItem("cmd", "_xclick");
  query.addQueryItem("business", business);
  query.addQueryItem("currency_code", "USD");
  query.addQueryItem("amount", QString::number(price, 'f', 2));
  query.addQueryItem("item_name", itemName);
  query.addQueryItem("custom", refNo);

  QUrl url(baseUrl);
  url.setQuery(query);

  // Confirmation Dialog
  QString msg = tr("You are about to open PayPal to pay <b>$%1 USD</b> to <b>%2</b>.<br><br>"
                   "Product: %3<br>"
                   "Reference: %4<br><br>"
                   "Please confirm to proceed to PayPal.")
                    .arg(QString::number(price, 'f', 2))
                    .arg(business)
                    .arg(itemName)
                    .arg(refNo);

  if (QMessageBox::question(this, tr("Confirm Payment"), msg) != QMessageBox::Yes) {
    return;
  }

  if (!QDesktopServices::openUrl(url)) {
    QMessageBox::warning(this, tr("Error"), tr("Failed to open web browser. Please visit PayPal manually."));
  }
}

void deskflow::gui::Esp32HidToolsWidget::onGenerateOrder()
{
  QString prefix;
  int option;
  QString content = composeOrderContent(prefix, option);
  if (content.isEmpty()) {
    return;
  }

  QString serial = m_orderSerialLabel->text();
  // Filename: hex serial only
  QString cleanSerial = serial;
  cleanSerial.remove(':');
  QString fileName = QString("%1%2.txt").arg(prefix, cleanSerial);

  QString path = QFileDialog::getSaveFileName(this, tr("Save Request File"), fileName, tr("Text Files (*.txt)"));
  if (path.isEmpty()) {
    return;
  }

  QFile file(path);
  if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QTextStream out(&file);
    out << content;
    file.close();

    log(tr("Request file generated: %1").arg(path));
    QMessageBox::information(
        this, tr("Success"),
        tr("Request file generated successfully!\n\nPlease send this file or its content (with payment if "
           "applicable) to deskflow.hid@gmail.com.\nYour firmware will be back in 24 hours.")
    );
  } else {
    log(tr("Failed to save request file: %1").arg(path));
    QMessageBox::critical(this, tr("Error"), tr("Failed to save request file."));
  }
}

void deskflow::gui::Esp32HidToolsWidget::onCopyOrderContent()
{
  QString prefix;
  int option;
  QString content = composeOrderContent(prefix, option);
  if (content.isEmpty()) {
    return;
  }

  QApplication::clipboard()->setText(content);
  log(tr("Order content copied to clipboard."));
  QMessageBox::information(this, tr("Copied"), tr("Order content successfully copied to clipboard."));
}

void deskflow::gui::Esp32HidToolsWidget::onEmailOrder()
{
  QString prefix;
  int option;
  QString content = composeOrderContent(prefix, option);
  if (content.isEmpty()) {
    return;
  }

  QString serial = m_orderSerialLabel->text();
  QString subject = QString("Deskflow Order: %1 %2").arg(prefix, serial);

  // URL encode content and subject
  QUrl url(QString("mailto:deskflow.hid@gmail.com"));
  QUrlQuery query;
  query.addQueryItem("subject", subject);
  query.addQueryItem("body", content);
  url.setQuery(query);

  if (QDesktopServices::openUrl(url)) {
    log(tr("Email client opened."));
  } else {
    log(tr("Failed to open email client."));
    QMessageBox::critical(this, tr("Error"), tr("Failed to open your default email client."));
  }
}

void deskflow::gui::Esp32HidToolsWidget::onPortChanged(int index)
{
  (void)index;
  QString portName = m_portCombo->currentData().toString();
  if (portName.isEmpty()) {
    portName = m_portCombo->currentText();
  }

  qInfo() << "Port selection changed to:" << portName;

  // Reset UI metadata
  m_copyInfoBtn->setProperty("deviceInfo", QVariant());
  m_lblCurrentVersion->setText(tr("Current Version: Unknown"));
  m_lblLatestVersion->setText(tr("Latest Version: Unknown"));
  m_flashOnlineBtn->setEnabled(false);
  m_lineSerial->clear();
  m_orderSerialLabel->setText(tr("-"));
  m_orderDeviceSecret->clear();
  m_labelActivationState->setText(tr("State: Unknown"));

  // Reset Order Options
  m_orderOption1->setEnabled(false);
  m_orderOption2->setEnabled(false);
  m_orderOption3->setEnabled(false);
  m_orderOption4->setEnabled(false);
  m_orderTotalProfiles->setEnabled(false);

  // If on Activation or Order tab, auto-refresh state
  QTabWidget *tabs = findChild<QTabWidget *>();
  if (tabs &&
      (tabs->tabText(tabs->currentIndex()) == tr("Activation") || tabs->tabText(tabs->currentIndex()) == tr("Order"))) {
    refreshDeviceState();
  }
}
