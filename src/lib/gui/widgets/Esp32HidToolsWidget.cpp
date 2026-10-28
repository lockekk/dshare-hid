/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#include "Esp32HidToolsWidget.h"
#include "BridgeClientWidget.h"
#include "base/Log.h"
#include "gui/core/BridgeFirmwareNotice.h"
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStandardPaths>
#include <QTabBar>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QtConcurrent>

// Include the C++ API from the submodule
#include "components/downloader/github_downloader.h"
#include "devices/UsbDeviceHelper.h"
#include "flash_tool.h"

#include "platform/bridge/CdcTransport.h"

#include <chrono>
#include <map>
#include <thread>
#include <tuple>

namespace {

const auto kSupportEmail = QStringLiteral("dshare.hid@gmail.com");

// The firmware packages come from the latest release of this GitHub repository
constexpr auto kReleaseOwner = "lockekk";
constexpr auto kReleaseRepo = "esp32-hid-release";

// The release carries one package of each kind per chip, named after the chip's ESP-IDF target:
// esp32-hid-<chip>-free-trial-<version>.fzip and esp32-hid-<chip>-upgrade-<version>.uzip. Only the
// free-trial factory package boots into the application by itself; the others leave the device
// waiting for per-device firmware.
std::string releasePackagePrefix(const std::string &chip, const char *kind)
{
  return "esp32-hid-" + chip + "-" + kind + "-";
}

std::string findReleaseAsset(const std::vector<ReleaseAsset> &assets, const std::string &prefix, const char *suffix)
{
  for (const auto &asset : assets) {
    if (asset.name.starts_with(prefix) && asset.name.ends_with(suffix)) {
      return asset.name;
    }
  }
  return {};
}

// The ESP-IDF target of the chip a dongle reported, for its package names; nullptr for a chip this
// app can't flash
const char *dongleChip(uint8_t chipId)
{
  // firmware 1.2.1 and older runs only on the ESP32-C3 and reports no chip
  constexpr uint8_t kEsp32C3ChipId = 5;
  return chip_target_name(chipId != 0 ? chipId : kEsp32C3ChipId);
}

// A development build flashes a board in any state: the dongle checks only log what they found
#ifdef DESKFLOW_DEV_BUILD
constexpr bool kDevBuild = true;
#else
constexpr bool kDevBuild = false;
#endif
const auto kDevBuildContinues = QStringLiteral("Development build: continuing anyway.");

// The firmware's version byte as text: 121 -> v1.2.1
QString firmwareVersionText(uint8_t bcd)
{
  return QStringLiteral("v%1.%2.%3").arg(bcd / 100).arg((bcd % 100) / 10).arg(bcd % 10);
}

QString activationStateText(const deskflow::bridge::FirmwareConfig &config)
{
  return deskflow::gui::BridgeClientWidget::activationStateText(QString::fromLatin1(config.activationStateString()));
}

QString kilobytes(size_t bytes)
{
  return QString::number((bytes + 1023) / 1024);
}

// The port as the port list shows it: without the Windows "\\.\" prefix
QString displayPort(const std::string &port)
{
  QString text = QString::fromStdString(port);
  if (text.startsWith(QStringLiteral("\\\\.\\"))) {
    text = text.mid(4);
  }
  return text;
}

std::tuple<int, int, int> parseVersion(const std::string &v)
{
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
}

} // namespace

namespace deskflow::gui {

Esp32HidToolsWidget::Esp32HidToolsWidget(const QString &devicePath, QWidget *parent)
    : QDialog(parent),
      m_devicePath(devicePath),
      m_isTaskRunning(false),
      // Default Fallback Prices (Hardcoded)
      m_prices{9.99, 3.99, 4.99, 1.99},
      m_network(new QNetworkAccessManager(this))
{
  // We can't log "this" easily unless we cast or format it, but usually devicePath suffices
  LOG_INFO("GUI of tool widget is created for device: %s", qPrintable(devicePath));
  QString title = tr("Firmware Flash Tool");
#ifdef DESKFLOW_USE_GENERATED_PUBLIC_KEY
  title += " (Dev build)";
#endif

  if (devicePath.isEmpty()) {
    setWindowTitle(title);
  } else {
    setWindowTitle(QString("%1 - %2").arg(title, devicePath));
  }
  resize(750, 1000);

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

  m_tabWidget = new QTabWidget();
  m_tabWidget->setMinimumHeight(100); // Allow shrinking below content hint
  m_tabWidget->setParent(this);

  // --- New Dongle Setup Tab ---
  auto *factoryTab = new QWidget();
  auto *factoryTabLayout = new QVBoxLayout(factoryTab);

  // shown instead of the setup when the dongle already runs DShare-HID firmware
  m_alreadySetUpLbl = new QLabel(tr("This dongle is already set up."));
  m_alreadySetUpLbl->setWordWrap(true);
  m_alreadySetUpLbl->hide();
  factoryTabLayout->addWidget(m_alreadySetUpLbl);

  m_newDongleSetupPanel = new QWidget();
  auto *factoryLayout = new QVBoxLayout(m_newDongleSetupPanel);
  factoryLayout->setContentsMargins(0, 0, 0, 0);
  factoryTabLayout->addWidget(m_newDongleSetupPanel);

  m_newDongleIntroLbl = new QLabel(
      tr("Installs DShare-HID firmware on a new dongle. You only need to do this once; after that, use the Upgrade tab "
         "for updates.")
  );
  m_newDongleIntroLbl->setWordWrap(true);
  factoryLayout->addWidget(m_newDongleIntroLbl);

#ifdef _WIN32
  auto *bootReminderLbl = new QLabel(tr(
      "Reminder: Press 'Boot' button then plugin your dongle. Click Refresh button to ensure the device is detected."
  ));
  bootReminderLbl->setStyleSheet("color: blue; font-weight: bold;");
  bootReminderLbl->setWordWrap(true);
  factoryLayout->addWidget(bootReminderLbl);
#endif

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
#ifndef DESKFLOW_DEV_BUILD
  // users only take the package published online; a package file is for development boards
  manualFactoryGroup->hide();
#endif

  factoryTabLayout->addStretch();

  m_tabWidget->addTab(factoryTab, tr("New Dongle Setup"));

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

  m_tabWidget->addTab(upgradeTab, tr("Upgrade"));

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
  orderTab->setMinimumHeight(100); // Allow shrinking below content hint
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
  m_orderOption2 = new QRadioButton(tr("Purchase Full License"));
  m_orderOption4 = new QRadioButton(tr("Upgrade Profile Capacity"));
  optionsLayout->addWidget(m_orderOption2);
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

  m_paymentTransId = new QLineEdit();
  m_paymentTransId->setPlaceholderText(tr("Paste your PayPal Transaction ID here"));

  m_chkManualPayment = new QCheckBox(tr("I will arrange payment via email"));
  connect(m_chkManualPayment, &QCheckBox::toggled, this, [this](bool checked) {
    m_paymentTransId->setEnabled(!checked);
    m_btnPayNow->setEnabled(!checked);
    if (checked) {
      m_paymentTransId->setPlaceholderText(tr("Not required for manual payment"));
    } else {
      m_paymentTransId->setPlaceholderText(tr("Paste your PayPal Transaction ID here"));
    }
  });

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
  paymentsLayout->addWidget(m_chkManualPayment, 4, 0, 1, 2); // Span 2 cols

  m_btnPayNow = new QPushButton(tr("Pay Now (Secure PayPal Link)"));
  paymentsLayout->addWidget(m_btnPayNow, 5, 0, 1, 2);

  auto *deviceInfoGroup = new QGroupBox(tr("Device Information"));
  auto *deviceInfoLayout = new QGridLayout(deviceInfoGroup);
  m_orderSerialLabel = new QLabel(tr("-"));
  m_orderSerialLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_orderTotalProfiles = new QComboBox();
  m_orderTotalProfiles->addItem("2", 2);
  m_orderTotalProfiles->addItem("4", 4);
  m_orderTotalProfiles->addItem("6", 6);

  deviceInfoLayout->addWidget(new QLabel(tr("Serial Number:")), 0, 0);
  deviceInfoLayout->addWidget(m_orderSerialLabel, 0, 1);
  deviceInfoLayout->addWidget(new QLabel(tr("Total Profiles:")), 1, 0);
  deviceInfoLayout->addWidget(m_orderTotalProfiles, 1, 1);

  m_btnCopyOrder = new QPushButton(tr("Copy content"));
  m_btnEmailOrder = new QPushButton(tr("Email"));

  auto *orderButtonLayout = new QHBoxLayout();
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

  m_splitter = new QSplitter(Qt::Vertical, this);
  m_splitter->addWidget(m_tabWidget);

  auto *logContainer = new QWidget();
  auto *logLayout = new QVBoxLayout(logContainer);
  logLayout->setContentsMargins(0, 0, 0, 0);
  m_progressBar = new QProgressBar();
  m_progressBar->setRange(0, 100);
  m_progressBar->hide();
  logLayout->addWidget(m_progressBar);
  logLayout->addWidget(new QLabel(tr("Log Output:")));
  logLayout->addWidget(m_logOutput);
  m_splitter->addWidget(logContainer);

  mainLayout->addWidget(m_splitter, 1);

  // Set initial sizes (top tabs small, bottom logs large)
  m_splitter->setSizes({300, 700});
  m_splitter->setStretchFactor(0, 0);
  m_splitter->setStretchFactor(1, 1);

  // Trigger initial layout adjustment
  onTabChanged(0);

  // Connect Signals
  connect(m_tabWidget, &QTabWidget::currentChanged, this, &Esp32HidToolsWidget::onTabChanged);

  connect(m_factoryBrowseBtn, &QPushButton::clicked, this, &Esp32HidToolsWidget::onBrowseFactory);
  connect(m_factoryFlashBtn, &QPushButton::clicked, this, &Esp32HidToolsWidget::onFlashFactory);
  connect(m_downloadFlashBtn, &QPushButton::clicked, this, &Esp32HidToolsWidget::onDownloadAndFlashFactory);

  connect(m_upgradeBrowseBtn, &QPushButton::clicked, this, &Esp32HidToolsWidget::onBrowseUpgrade);
  connect(m_checkUpgradeBtn, &QPushButton::clicked, this, &Esp32HidToolsWidget::onCheckUpgrade);
  connect(m_flashOnlineBtn, &QPushButton::clicked, this, &Esp32HidToolsWidget::onFlashOnline);
  connect(m_flashLocalBtn, &QPushButton::clicked, this, &Esp32HidToolsWidget::onFlashLocal);

  connect(m_btnCopySerial, &QPushButton::clicked, this, &Esp32HidToolsWidget::onCopySerialClicked);
  connect(m_btnActivate, &QPushButton::clicked, this, &Esp32HidToolsWidget::onActivateClicked);
  connect(m_btnCopyOrder, &QPushButton::clicked, this, &Esp32HidToolsWidget::onCopyOrderContent);
  connect(m_btnEmailOrder, &QPushButton::clicked, this, &Esp32HidToolsWidget::onEmailOrder);

  connect(m_refreshPortsBtn, &QPushButton::clicked, this, &Esp32HidToolsWidget::refreshPorts);
  connect(m_portCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Esp32HidToolsWidget::onPortChanged);

  connect(m_orderOption2, &QRadioButton::toggled, this, &Esp32HidToolsWidget::updatePaymentDetails);
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

  // Fetch prices
  fetchPrices();
}

void Esp32HidToolsWidget::fetchPrices()
{
  // 1. Check local override
  QString localPath =
      QStandardPaths::locate(QStandardPaths::AppConfigLocation, "prices.json", QStandardPaths::LocateFile);
  if (!localPath.isEmpty()) {
    QFile f(localPath);
    if (f.open(QIODevice::ReadOnly)) {
      QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
      if (!doc.isNull() && doc.isObject()) {
        QJsonObject obj = doc.object();
        if (obj.contains("license"))
          m_prices.license = obj["license"].toDouble();
        if (obj.contains("profile_4"))
          m_prices.profile_4 = obj["profile_4"].toDouble();
        if (obj.contains("profile_6"))
          m_prices.profile_6 = obj["profile_6"].toDouble();
        if (obj.contains("combo_discount"))
          m_prices.combo_discount = obj["combo_discount"].toDouble();
        LOG_INFO("Loaded local prices override: %s", qPrintable(localPath));
        updatePaymentDetails();
        return; // Local override takes precedence
      }
    }
  }

  // 2. Fetch online
#ifdef DESKFLOW_PRICES_URL
  QString urlStr = DESKFLOW_PRICES_URL;
#else
  QString urlStr = "https://raw.githubusercontent.com/lockekk/esp32-hid-release/main/prices.json";
#endif

  LOG_INFO("Fetching prices from: %s", qPrintable(urlStr));
  QNetworkRequest req(urlStr);
  QNetworkReply *reply = m_network->get(req);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() { onPriceResponse(reply); });
}

void Esp32HidToolsWidget::onPriceResponse(QNetworkReply *reply)
{
  reply->deleteLater();
  if (reply->error() != QNetworkReply::NoError) {
    LOG_WARN("Failed to fetch prices: %s", qPrintable(reply->errorString()));
    return;
  }

  QByteArray data = reply->readAll();
  QJsonDocument doc = QJsonDocument::fromJson(data);
  if (doc.isNull() || !doc.isObject()) {
    LOG_WARN("Invalid JSON prices response");
    return;
  }

  QJsonObject obj = doc.object();
  // Only update if value is present and valid
  if (obj.contains("license"))
    m_prices.license = obj["license"].toDouble();
  if (obj.contains("profile_4"))
    m_prices.profile_4 = obj["profile_4"].toDouble();
  if (obj.contains("profile_6"))
    m_prices.profile_6 = obj["profile_6"].toDouble();
  if (obj.contains("combo_discount"))
    m_prices.combo_discount = obj["combo_discount"].toDouble();

  LOG_INFO("Updated prices online: License=$%f, ComboDiscount=$%f", m_prices.license, m_prices.combo_discount);
  updatePaymentDetails(); // Refresh UI
}

void Esp32HidToolsWidget::refreshPorts()
{
  // one port change once the list is filled, not one per item: each change can ask the dongle
  QSignalBlocker blocker(m_portCombo);
  m_portCombo->clear();
  LOG_INFO("Refreshing ports...");
  // Only filter by VID/PID, don't try to read serial (avoids timeouts on stuck devices)
  auto devices = UsbDeviceHelper::getConnectedDevices(false);
  LOG_INFO("Found %d candidate devices.", devices.size());

  if (devices.isEmpty()) {
    m_portCombo->addItem(tr("No devices found"));
    LOG_WARN("No supported USB bridge devices found.");
    // logic to disable flash buttons?
  } else {
    for (auto it = devices.begin(); it != devices.end(); ++it) {
      QString path = it.key();
      QString serial = it.value();
      QString displayPath = path;
      if (displayPath.startsWith(QStringLiteral("\\\\.\\"))) {
        displayPath = displayPath.mid(4);
      }

      if (serial.isEmpty()) {
        LOG_INFO("Adding device: %s", qPrintable(path));
        m_portCombo->addItem(displayPath, path);
      } else if (serial == displayPath) {
        LOG_INFO("Adding device: %s (fallback serial)", qPrintable(path));
        m_portCombo->addItem(displayPath, path);
      } else {
        LOG_INFO("Adding device: %s Serial: %s", qPrintable(path), qPrintable(serial));
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
      LOG_INFO("Device path from bridge client not in detected list, adding manually: %s", qPrintable(m_devicePath));
      m_portCombo->addItem(m_devicePath, m_devicePath);
      m_portCombo->setCurrentIndex(m_portCombo->count() - 1);
    }
  }

  blocker.unblock();
  onPortChanged(m_portCombo->currentIndex());
}

void Esp32HidToolsWidget::onBrowseFactory()
{
  QString path =
      QFileDialog::getOpenFileName(this, tr("Select Factory Package"), QString(), tr("Factory Package (*.fzip)"));
  if (!path.isEmpty()) {
    m_factoryPathEdit->setText(path);
    log(tr("Selected factory firmware: %1").arg(QFileInfo(path).fileName()));
  }
}

void Esp32HidToolsWidget::onBrowseUpgrade()
{
  QString path =
      QFileDialog::getOpenFileName(this, tr("Select Upgrade Package"), QString(), tr("Upgrade Package (*.uzip)"));
  if (!path.isEmpty()) {
    m_upgradePathEdit->setText(path);
    log(tr("Selected upgrade firmware: %1").arg(QFileInfo(path).fileName()));
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
  // The window's lines also go to the app log, so a log the user sends shows each step
  LOG_INFO("firmware tool: %s", qPrintable(message));

  // workers log too: the text box is only touched on the GUI thread
  QMetaObject::invokeMethod(this, [this, message]() {
    m_logOutput->append(message);
    QScrollBar *sb = m_logOutput->verticalScrollBar();
    sb->setValue(sb->maximum());
  });
}

// The flash tool's own steps and failure reasons: the app log only, so they don't fill the window.
// A failure shows the last error in the window too (logFlashFailure()).
void Esp32HidToolsWidget::logDetail(const std::string &message)
{
  LOG_INFO("firmware tool detail: %s", message.c_str());
  if (message.starts_with("Error")) {
    m_lastFlashError = message;
  }
}

// Runs on the worker thread: the result, with the flash tool's reason, to find the problem from the window's log
void Esp32HidToolsWidget::logFlashFailure(FlashResult result)
{
  QString line = tr("Flash failed, %1").arg(QString::fromStdString(flashResultToString(result)));
  if (!m_lastFlashError.empty()) {
    line += QStringLiteral(" (%1)").arg(QString::fromStdString(m_lastFlashError));
  }
  log(line);
}

// The Activation and Order tabs read the state each time they open: only a change is worth a line
void Esp32HidToolsWidget::logDeviceState(const QString &state)
{
  if (state != m_lastDeviceState) {
    m_lastDeviceState = state;
    log(state);
  }
}

// Runs on the worker thread
void Esp32HidToolsWidget::setProgress(size_t done, size_t total)
{
  const int percent = total > 0 ? static_cast<int>(done * 100 / total) : 0;
  if (percent == m_progressPercent) {
    return;
  }
  m_progressPercent = percent;
  QMetaObject::invokeMethod(this, [this, percent]() {
    m_progressBar->setValue(percent);
    m_progressBar->show();
  });
}

void Esp32HidToolsWidget::setControlsEnabled(bool enabled)
{
  m_factoryFlashBtn->setEnabled(enabled);
  m_downloadFlashBtn->setEnabled(enabled);
  m_flashOnlineBtn->setEnabled(enabled && m_onlineFlashAllowed);
  m_flashLocalBtn->setEnabled(enabled);
  m_factoryBrowseBtn->setEnabled(enabled);
  m_upgradeBrowseBtn->setEnabled(enabled);
  m_checkUpgradeBtn->setEnabled(enabled);
  m_refreshPortsBtn->setEnabled(enabled);
  m_portCombo->setEnabled(enabled);
  m_factoryPathEdit->setEnabled(enabled);
  m_upgradePathEdit->setEnabled(enabled);
  // another tab would talk to the dongle while a task uses it
  m_tabWidget->tabBar()->setEnabled(enabled);
}

// Helper to run tasks in background
template <typename Function> void Esp32HidToolsWidget::runBackgroundTask(Function func)
{
  setControlsEnabled(false);
  m_isTaskRunning = true;
  m_progressPercent = -1;
  m_lastFlashError.clear();
  QApplication::setOverrideCursor(Qt::WaitCursor);

  auto *watcher = new QFutureWatcher<void>(this);
  connect(watcher, &QFutureWatcher<void>::finished, this, [this, watcher]() {
    QApplication::restoreOverrideCursor();
    setControlsEnabled(true);
    m_isTaskRunning = false;
    m_progressBar->hide();
    watcher->deleteLater();
  });

  QFuture<void> future = QtConcurrent::run(func);
  watcher->setFuture(future);
}

// The port to work on, or empty after asking the user to pick one
QString Esp32HidToolsWidget::selectedPort()
{
  QString portName = m_portCombo->currentData().toString();
  if (portName.isEmpty()) {
    portName = m_portCombo->currentText();
    if (portName.isEmpty() || portName == tr("No devices found")) {
      showWideMessageBox(QMessageBox::Warning, tr("Error"), tr("Please select a valid serial port."));
      return QString();
    }
  }
  return portName;
}

void Esp32HidToolsWidget::onFlashFactory()
{
  const QString portName = selectedPort();
  if (portName.isEmpty()) {
    return;
  }

  const QString path = m_factoryPathEdit->text();
  if (path.isEmpty()) {
    showWideMessageBox(QMessageBox::Warning, tr("Error"), tr("Please select a factory package file to flash."));
    return;
  }
  std::vector<uint8_t> data = readFile(path);
  if (data.empty()) {
    showWideMessageBox(QMessageBox::Critical, tr("Error"), tr("Failed to read firmware file."));
    return;
  }

  if (!confirmNewDongleSetup()) {
    return;
  }

  log(tr("Setting up a new dongle with %1 (%2 KB).").arg(path, kilobytes(data.size())));
  runBackgroundTask([this, port = portName.toStdString(), data]() {
    if (dongleNeedsSetup(port)) {
      setUpNewDongle(port, [&data](const std::string &) { return data; });
    }
  });
}

void Esp32HidToolsWidget::onDownloadAndFlashFactory()
{
  const QString portName = selectedPort();
  if (portName.isEmpty() || !confirmNewDongleSetup()) {
    return;
  }

  log(tr("Setting up a new dongle with the latest firmware."));
  runBackgroundTask([this, port = portName.toStdString()]() {
    if (!dongleNeedsSetup(port)) {
      return;
    }

    GithubDownloader downloader(kReleaseOwner, kReleaseRepo);
    const auto assets = downloader.get_latest_assets();
    if (assets.empty()) {
      logDetail(std::string("no assets in the latest ") + kReleaseOwner + "/" + kReleaseRepo + " release");
      const QString text = tr("Could not reach the download server. Check your internet connection and try again.");
      log(text);
      QMetaObject::invokeMethod(this, [this, text]() {
        showWideMessageBox(QMessageBox::Warning, tr("New Dongle Setup"), text);
      });
      return;
    }

    // Only the dongle's ROM tells its chip, once the flash has connected to it. Download the setup
    // firmware of every chip first, so the dongle never waits in download mode on the network.
    std::map<std::string, std::vector<uint8_t>> packages;
    for (const auto &asset : assets) {
      if (asset.name.find("-free-trial-") == std::string::npos || !asset.name.ends_with(".fzip")) {
        continue;
      }
      log(tr("Downloading %1...").arg(QString::fromStdString(asset.name)));
      auto data = downloader.download_asset_to_memory(asset.name);
      if (!data.has_value()) {
        log(tr("Download failed."));
        QMetaObject::invokeMethod(this, [this]() {
          showWideMessageBox(
              QMessageBox::Critical, tr("Error"), tr("Failed to download firmware. Check internet connection.")
          );
        });
        return;
      }
      log(tr("Downloaded (%1 KB).").arg(kilobytes(data->size())));
      packages.emplace(asset.name, std::move(data.value()));
    }

    setUpNewDongle(port, [this, &assets, &packages](const std::string &chip) -> std::vector<uint8_t> {
      const std::string prefix = releasePackagePrefix(chip, "free-trial");
      const std::string assetName = findReleaseAsset(assets, prefix, ".fzip");
      if (assetName.empty()) {
        logDetail(
            "no " + prefix + "*.fzip among the " + std::to_string(assets.size()) + " assets of the latest " +
            kReleaseOwner + "/" + kReleaseRepo + " release"
        );
        const QString text =
            tr("The setup firmware for this dongle is not available for download yet. Please try again later.");
        log(text);
        QMetaObject::invokeMethod(this, [this, text]() {
          showWideMessageBox(QMessageBox::Information, tr("New Dongle Setup"), text);
        });
        return {};
      }
      return packages.at(assetName);
    });
  });
}

std::function<void()> Esp32HidToolsWidget::createReplugCallback()
{
  return [this]() {
    QMetaObject::invokeMethod(
        this,
        [this]() {
          log(tr("Waiting for user to reconnect device..."));
          QMessageBox msgBox(
              QMessageBox::Warning, tr("Reconnect Device"), tr("Please disconnect and reconnect the device now."),
              QMessageBox::NoButton, this
          );
          msgBox.addButton(tr("I have reconnected the device"), QMessageBox::AcceptRole);
          msgBox.exec();
          log(tr("User confirmed reconnection."));
        },
        Qt::BlockingQueuedConnection
    );
  };
}

int Esp32HidToolsWidget::showWideMessageBox(
    QMessageBox::Icon icon, const QString &title, const QString &text, QMessageBox::StandardButtons buttons
)
{
  QMessageBox msgBox(icon, title, text, buttons, this);
  // Force the dialog to be 500px wide for readability.
  // We use a stylesheet to prevent macOS native override of the layout.
  msgBox.setStyleSheet("QMessageBox { min-width: 500px; }");
  if (auto *layout = qobject_cast<QGridLayout *>(msgBox.layout())) {
    auto *spacer = new QSpacerItem(500, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
    layout->addItem(spacer, layout->rowCount(), 0, 1, layout->columnCount());
  }
  return msgBox.exec();
}

bool Esp32HidToolsWidget::confirmNewDongleSetup()
{
  return showWideMessageBox(
             QMessageBox::Question, tr("Set Up New Dongle"), tr("Set up this dongle? This is permanent."),
             QMessageBox::Yes | QMessageBox::No
         ) == QMessageBox::Yes;
}

// Runs on the worker thread: asks the dongle on the port which firmware it runs
Esp32HidToolsWidget::DongleCheck Esp32HidToolsWidget::checkDongle(const std::string &port, VersionNote note)
{
  using deskflow::bridge::FirmwareMode;

  log(tr("Checking the dongle on %1...").arg(displayPort(port)));

  DongleCheck check;
  deskflow::bridge::CdcTransport cdc(QString::fromStdString(port));
  // permissive: this only asks which firmware answers
  const bool answered = cdc.open(true) && cdc.hasDeviceConfig();
  check.portOpened = cdc.isOpen();
  if (!answered) {
    logDetail(
        "no DShare-HID handshake on " + port + (check.portOpened ? "" : " (port not opened)") + ": " + cdc.lastError()
    );
    return check;
  }

  const auto &config = cdc.deviceConfig();
  check.chipId = config.chipId;
  if (config.firmwareMode == FirmwareMode::App) {
    check.setup = DongleSetup::App;
    check.version = firmwareVersionText(config.firmwareVersionBcd);
    std::string serial;
    cdc.fetchSerialNumber(serial);
    check.serial = QString::fromStdString(serial);
    // for the notice to pair the hosts again after 1.2.1
    if (!check.serial.isEmpty()) {
      if (note == VersionNote::BeforeUpgrade) {
        check.pairingNotice = BridgeFirmwareNotice::noteHandshake(check.serial, config);
      } else if (note == VersionNote::ConfirmUpgrade) {
        check.upgradeConfirmed = BridgeFirmwareNotice::confirmUpgrade(check.serial, config, check.pairingNotice);
      }
    }
    log(tr("Dongle found: firmware %1, %2, serial %3.")
            .arg(
                check.version, activationStateText(config),
                serial.empty() ? QStringLiteral("-") : QString::fromStdString(serial)
            ));
  } else if (config.firmwareMode == FirmwareMode::Factory) {
    check.setup = config.hasOtaPartition ? DongleSetup::Factory : DongleSetup::FactoryWithoutApp;
    log(config.hasOtaPartition ? tr("Dongle found in update mode.")
                               : tr("Dongle found in update mode, without application firmware."));
  } else {
    logDetail("unknown firmware mode " + std::to_string(static_cast<unsigned>(config.firmwareMode)) + " on " + port);
  }
  return check;
}

// Runs on the worker thread: a dongle that already runs DShare-HID firmware is updated on the Upgrade tab
bool Esp32HidToolsWidget::dongleNeedsSetup(const std::string &port)
{
  const auto check = checkDongle(port, VersionNote::None);
  if (check.setup == DongleSetup::NoAnswer) {
    log(tr("No DShare-HID firmware found on the dongle."));
    return true;
  }

  const QString text =
      check.setup == DongleSetup::FactoryWithoutApp
          ? tr("This dongle is already set up, but its last update did not finish. Use the Upgrade tab to install "
               "the firmware again.")
          : tr("This dongle is already set up. Use the Upgrade tab to update its firmware.");
  log(text);
  if (kDevBuild) {
    log(kDevBuildContinues);
    return true;
  }
  QMetaObject::invokeMethod(this, [this, text]() {
    showDongleSetUp(true);
    showWideMessageBox(QMessageBox::Information, tr("Already Set Up"), text);
    m_tabWidget->setCurrentIndex(1);
  });
  return false;
}

// Opening the New Dongle Setup tab asks the dongle first, so a set-up dongle isn't offered the setup
void Esp32HidToolsWidget::checkNewDongleSetup()
{
  QString portName = m_portCombo->currentData().toString();
  if (portName.isEmpty()) {
    portName = m_portCombo->currentText();
    if (portName.isEmpty() || portName == tr("No devices found")) {
      showDongleSetUp(false);
      return;
    }
  }

  runBackgroundTask([this, port = portName.toStdString()]() {
    // the factory or the application firmware answers: the dongle was set up before
    const bool setUp = checkDongle(port, VersionNote::None).setup != DongleSetup::NoAnswer;
    log(setUp ? tr("This dongle is already set up.") : tr("No DShare-HID firmware found on the dongle."));
    QMetaObject::invokeMethod(this, [this, setUp]() { showDongleSetUp(setUp); });
  });
}

void Esp32HidToolsWidget::showDongleSetUp(bool setUp)
{
  m_alreadySetUpLbl->setVisible(setUp);
  // a development build sets up a board in any state
  m_newDongleSetupPanel->setVisible(!setUp || kDevBuild);
}

// Runs on the worker thread: writes a new dongle's firmware package and waits for the application
void Esp32HidToolsWidget::setUpNewDongle(const std::string &port, const FactoryPackageProvider &packageForChip)
{
  log(tr("Writing the firmware..."));
  bool written = false;
  auto progress_cb = [this, &written](size_t done, size_t total, size_t) {
    setProgress(done, total);
    if (done == total && !written) {
      written = true;
      log(tr("Firmware written. Restarting the dongle..."));
    }
  };
  auto log_cb = [this](const std::string &msg) { logDetail(msg); };

  const FlashResult res = flash_factory(port, packageForChip, progress_cb, log_cb, createReplugCallback());
  if (res == FlashResult::ERROR_EMPTY_DATA) {
    return; // packageForChip told why it has no package
  }
  if (res != FlashResult::OK) {
    logFlashFailure(res);
    const QString text = flashFailureText(res, true);
    QMetaObject::invokeMethod(this, [this, text]() { showWideMessageBox(QMessageBox::Critical, tr("Error"), text); });
    return;
  }

  const auto check = waitForAppAfterFlash(port);
  QMetaObject::invokeMethod(this, [this, check]() { showFactoryFlashResult(check); });
}

// Runs on the worker thread: polls the device until it reports the application firmware.
Esp32HidToolsWidget::FactoryFlashCheck Esp32HidToolsWidget::waitForAppAfterFlash(const std::string &port)
{
  using namespace std::chrono_literals;
  using deskflow::bridge::FirmwareMode;

  log(tr("Waiting for the dongle to start..."));

  // Any byte the factory receives restarts its 10 s idle timer before it boots OTA_0,
  // so back off after seeing the factory instead of probing it every second.
  constexpr auto kTimeout = 60s;
  constexpr auto kRetryDelay = 1s;
  constexpr auto kFactoryBootDelay = 12s;

  const auto deadline = std::chrono::steady_clock::now() + kTimeout;
  while (std::chrono::steady_clock::now() < deadline) {
    auto delay = kRetryDelay;
    {
      deskflow::bridge::CdcTransport cdc(QString::fromStdString(port));
      if (cdc.open(true) && cdc.hasDeviceConfig()) {
        const auto &config = cdc.deviceConfig();
        if (config.firmwareMode == FirmwareMode::App) {
          // the version it runs now, for the notice to pair the hosts again after 1.2.1
          std::string serial;
          QString notice;
          if (cdc.fetchSerialNumber(serial)) {
            notice = BridgeFirmwareNotice::noteHandshake(QString::fromStdString(serial), config);
          }
          const QString version = firmwareVersionText(config.firmwareVersionBcd);
          log(tr("Dongle ready: firmware %1, %2.").arg(version, activationStateText(config)));
          return {FactoryFlashOutcome::AppStarted, config.activationState, notice, version};
        }
        if (config.firmwareMode == FirmwareMode::Factory) {
          if (!config.hasOtaPartition) {
            log(tr("The dongle stayed in update mode, without application firmware."));
            return {FactoryFlashOutcome::FactoryWithoutApp, config.activationState, QString(), QString()};
          }
          delay = kFactoryBootDelay;
        }
      }
    }
    std::this_thread::sleep_for(delay);
  }

  log(tr("The dongle did not report back within 60 seconds."));
  return {FactoryFlashOutcome::NotDetected, deskflow::bridge::ActivationState::Unknown, QString(), QString()};
}

void Esp32HidToolsWidget::showFactoryFlashResult(const FactoryFlashCheck &check)
{
  using deskflow::bridge::ActivationState;

  if (check.outcome != FactoryFlashOutcome::NotDetected) {
    showDongleSetUp(true);
  }

  switch (check.outcome) {
  case FactoryFlashOutcome::AppStarted:
    if (check.activationState == ActivationState::FreeTrial) {
      showWideMessageBox(
          QMessageBox::Information, tr("Success"),
          tr("Firmware flashed successfully.\n\n"
             "The device is running a 7-day free trial. "
             "After the trial expires, use the Activation tab to activate with a license key.")
      );
    } else {
      showWideMessageBox(
          QMessageBox::Information, tr("Success"),
          tr("Firmware flashed successfully.\n\n"
             "The device is running the application firmware. Check its license on the Activation tab.")
      );
    }
    break;

  case FactoryFlashOutcome::FactoryWithoutApp:
    showWideMessageBox(QMessageBox::Warning, tr("No Application Firmware"), noApplicationFirmwareMessage());
    break;

  case FactoryFlashOutcome::NotDetected:
    showWideMessageBox(
        QMessageBox::Warning, tr("Device Not Detected"),
        tr("The firmware was written, but the device did not start the application within 60 seconds.\n\n"
           "Disconnect and reconnect the device, then check its status on the Activation tab.")
    );
    break;
  }
}

QString Esp32HidToolsWidget::noApplicationFirmwareMessage() const
{
  return tr("The device is in factory mode without application firmware.\n\n"
            "Install it from the Upgrade tab (Check for Updates, then Flash). "
            "If that fails, contact %1 with the device serial number.")
      .arg(kSupportEmail);
}

QString Esp32HidToolsWidget::flashFailureText(FlashResult result, bool newDongle) const
{
  switch (result) {
  case FlashResult::ERROR_CONNECTION:
  case FlashResult::ERROR_TIMEOUT:
    if (!newDongle) {
      return tr("Lost the connection to the dongle. Reconnect it and try again.");
    }
#ifdef _WIN32
    return tr(
        "Could not connect to the dongle. Hold the Boot button while you plug it in, then try again. "
        "If it was set up before, use the Upgrade tab instead."
    );
#else
    return tr(
        "Could not connect to the dongle. Reconnect it and try again. "
        "If it was set up before, use the Upgrade tab instead."
    );
#endif
  case FlashResult::ERROR_FIRMWARE_CHECK:
    return tr("The dongle did not accept this firmware.");
  case FlashResult::ERROR_ARGS:
  case FlashResult::ERROR_FILE_IO:
  case FlashResult::ERROR_DECRYPTION:
  case FlashResult::ERROR_EMPTY_DATA:
    return tr("This is not a valid firmware file.");
  case FlashResult::ERROR_FLASH_WRITE:
    return tr("Writing the firmware failed. Reconnect the dongle and try again.");
  default:
    return tr("Flash failed, %1").arg(QString::fromStdString(flashResultToString(result)));
  }
}

// Runs on the worker thread: only a dongle that runs DShare-HID firmware takes an upgrade
bool Esp32HidToolsWidget::readyForUpgrade(const DongleCheck &check)
{
  if (check.setup == DongleSetup::NoAnswer) {
    const QString title = check.portOpened ? tr("Not Set Up") : tr("Error");
    const QString text = check.portOpened ? tr("This dongle is not set up yet. Use the New Dongle Setup tab first.")
                                          : tr("Could not open the port. Reconnect the dongle and try again.");
    log(text);
    if (kDevBuild) {
      log(kDevBuildContinues);
      return true;
    }
    QMetaObject::invokeMethod(this, [this, title, text]() { showWideMessageBox(QMessageBox::Warning, title, text); });
    return false;
  }

  // Still upgraded: on a release dongle this is the only repair, since its first boot turned off
  // the ROM download mode a new dongle setup needs
  if (check.setup == DongleSetup::FactoryWithoutApp) {
    log(tr("The last update did not finish. The firmware will be installed again."));
  }
  return true;
}

// Check Upgrade
void Esp32HidToolsWidget::onCheckUpgrade()
{
  const QString portName = selectedPort();
  if (portName.isEmpty()) {
    return;
  }

  // Online flash is only allowed for the port that this check finds an update for
  m_onlineFlashAllowed = false;
  m_flashOnlineBtn->setEnabled(false);

  runBackgroundTask([this, port = portName.toStdString()]() {
    const auto check = checkDongle(port, VersionNote::ConfirmUpgrade);
    if (!readyForUpgrade(check)) {
      return;
    }
    // an upgrade that didn't report back in time worked: finish it as the upgrade would have
    if (check.upgradeConfirmed) {
      logDetail("the dongle runs " + check.version.toStdString() + " after an upgrade that did not report back");
      QMetaObject::invokeMethod(this, [this, notice = check.pairingNotice]() { showUpgradeSuccess(notice); });
    }

    log(tr("Checking for upgrades..."));
    GithubDownloader downloader(kReleaseOwner, kReleaseRepo);
    const auto latestTag = downloader.get_latest_tag();
    if (latestTag.empty()) {
      log(tr("Could not check for updates."));
      QMetaObject::invokeMethod(this, [this]() {
        showWideMessageBox(QMessageBox::Warning, tr("Network Error"), tr("Could not check for updates."));
      });
      return;
    }

    // only the application firmware reports its version: a dongle in update mode takes the update
    const QString deviceVer = check.version;
    logDetail(
        "version check: latest tag " + latestTag + ", device " +
        (deviceVer.isEmpty() ? std::string("Unknown") : deviceVer.toStdString())
    );
    bool updateAvailable = deviceVer.isEmpty();
    if (!updateAvailable) {
      updateAvailable = parseVersion(latestTag) > parseVersion(deviceVer.toStdString());
    }

    QMetaObject::invokeMethod(this, [this, deviceVer, latestTag, updateAvailable]() {
      const QString latest = QString::fromStdString(latestTag);
      m_lblCurrentVersion->setText(tr("Current Version: %1").arg(deviceVer.isEmpty() ? tr("Unknown") : deviceVer));
      m_lblLatestVersion->setText(tr("Latest Version: %1").arg(latest));

      if (!updateAvailable) {
        log(tr("Device is up to date."));
        if (!kDevBuild) {
          return;
        }
        log(kDevBuildContinues);
      } else if (!deviceVer.isEmpty()) {
        log(tr("Update available (%1 > %2).").arg(latest).arg(deviceVer));
      } else {
        log(tr("Update available (Device version unknown)."));
      }
      m_onlineFlashAllowed = true;
      m_flashOnlineBtn->setEnabled(true);
      m_flashOnlineBtn->setToolTip("");
    });
  });
}

void Esp32HidToolsWidget::onFlashLocal()
{
  const QString portName = selectedPort();
  if (portName.isEmpty()) {
    return;
  }

  QString path = m_upgradePathEdit->text();
  if (path.isEmpty()) {
    showWideMessageBox(QMessageBox::Warning, tr("Error"), tr("Please select a firmware file."));
    return;
  }

  std::vector<uint8_t> data = readFile(path);
  if (data.empty()) {
    showWideMessageBox(QMessageBox::Critical, tr("Error"), tr("Failed to read firmware file."));
    return;
  }

  log(tr("Flashing local file: %1").arg(path));
  runBackgroundTask([this, port = portName.toStdString(), data]() {
    const auto check = checkDongle(port, VersionNote::BeforeUpgrade);
    if (readyForUpgrade(check)) {
      upgradeDongle(port, data, check);
    }
  });
}

void Esp32HidToolsWidget::onFlashOnline()
{
  const QString portName = selectedPort();
  if (portName.isEmpty()) {
    return;
  }

  runBackgroundTask([this, port = portName.toStdString()]() {
    const auto check = checkDongle(port, VersionNote::BeforeUpgrade);
    if (!readyForUpgrade(check)) {
      return;
    }

    if (check.chipId == 0) {
      logDetail("the dongle reports no chip: firmware 1.2.1 or older, which runs only on the ESP32-C3");
    }
    const char *chip = dongleChip(check.chipId);
    if (chip == nullptr) {
      logDetail("unknown chip id " + std::to_string(check.chipId));
      const QString text = tr("This version of DShare-HID can't update this dongle. Update DShare-HID and try again.");
      log(text);
      QMetaObject::invokeMethod(this, [this, text]() { showWideMessageBox(QMessageBox::Critical, tr("Error"), text); });
      return;
    }

    GithubDownloader downloader(kReleaseOwner, kReleaseRepo);
    const auto assets = downloader.get_latest_assets();
    const std::string prefix = releasePackagePrefix(chip, "upgrade");
    const std::string assetName = findReleaseAsset(assets, prefix, ".uzip");
    if (assetName.empty()) {
      logDetail(
          "no " + prefix + "*.uzip among the " + std::to_string(assets.size()) + " assets of the latest " +
          kReleaseOwner + "/" + kReleaseRepo + " release"
      );
      log(tr("Download failed. No .uzip asset found."));
      QMetaObject::invokeMethod(this, [this]() {
        showWideMessageBox(
            QMessageBox::Critical, tr("Error"), tr("Failed to find upgrade firmware in the latest release.")
        );
      });
      return;
    }

    log(tr("Downloading %1...").arg(QString::fromStdString(assetName)));
    const auto data = downloader.download_asset_to_memory(assetName);
    if (!data.has_value()) {
      log(tr("Download failed."));
      QMetaObject::invokeMethod(this, [this]() {
        showWideMessageBox(QMessageBox::Critical, tr("Error"), tr("Failed to download firmware."));
      });
      return;
    }
    log(tr("Downloaded (%1 KB).").arg(kilobytes(data->size())));

    upgradeDongle(port, data.value(), check);
  });
}

// Runs on the worker thread: sends an upgrade package to a dongle that readyForUpgrade() accepted
void Esp32HidToolsWidget::upgradeDongle(
    const std::string &port, const std::vector<uint8_t> &package, const DongleCheck &check
)
{
  using namespace std::chrono_literals;
  using deskflow::bridge::FirmwareMode;

  const QString portName = QString::fromStdString(port);

  // The factory firmware receives the update: the application firmware restarts into it
  if (check.setup == DongleSetup::App) {
    log(tr("Preparing the dongle for the update..."));
    deskflow::bridge::CdcTransport cdc(portName);
    if (!cdc.open() || !cdc.gotoFactory()) {
      logDetail("switch to the factory firmware failed: " + cdc.lastError());
      const QString text = tr("The dongle could not switch to update mode. Reconnect it and try again.");
      log(text);
      QMetaObject::invokeMethod(this, [this, text]() { showWideMessageBox(QMessageBox::Critical, tr("Error"), text); });
      return;
    }
    cdc.close(); // Close to allow re-enumeration

    bool reconnected = false;
    for (int i = 0; i < 30 && !reconnected; i++) { // 15 seconds
      std::this_thread::sleep_for(500ms);
      deskflow::bridge::CdcTransport probe(portName);
      reconnected =
          probe.open(true) && probe.hasDeviceConfig() && probe.deviceConfig().firmwareMode == FirmwareMode::Factory;
    }
    if (reconnected) {
      logDetail("the factory firmware answered on " + port);
      std::this_thread::sleep_for(500ms);
    } else {
      logDetail("the factory firmware did not answer on " + port + " within 15 s, trying the update anyway");
    }
  }

  log(tr("Sending the firmware..."));
  auto progress_cb = [this](size_t done, size_t total, size_t) { setProgress(done, total); };
  auto log_cb = [this](const std::string &msg) { logDetail(msg); };

  FlashResult res = FlashResult::ERROR_GENERAL;
  try {
    res = flash_upgrade(port, package, progress_cb, log_cb);
  } catch (const std::exception &e) {
    logDetail(std::string("flash_upgrade threw: ") + e.what());
  }

  if (res != FlashResult::OK) {
    logFlashFailure(res);
    QString text = flashFailureText(res, false);
    if (check.setup == DongleSetup::FactoryWithoutApp) {
      text += QStringLiteral("\n\n") +
              tr("If this keeps failing, contact %1 with the dongle's serial number.").arg(kSupportEmail);
    }
    QMetaObject::invokeMethod(this, [this, text]() { showWideMessageBox(QMessageBox::Critical, tr("Error"), text); });
    return;
  }

  log(tr("Firmware accepted. Restarting the dongle..."));
  // the factory restarts 100 ms after it acks the reboot: let it go before looking for the new firmware
  std::this_thread::sleep_for(1s);
  const auto started = waitForAppAfterFlash(port);
  if (started.outcome == FactoryFlashOutcome::NotDetected) {
    // the version from before the upgrade stays: the next Check for Updates confirms it
    BridgeFirmwareNotice::noteUnconfirmedUpgrade(check.serial);
  }
  // a notice the check before the upgrade found due goes with this upgrade's success
  const QString notice = started.pairingNotice.isEmpty() ? check.pairingNotice : started.pairingNotice;
  QMetaObject::invokeMethod(this, [this, outcome = started.outcome, notice, version = started.version]() {
    switch (outcome) {
    case FactoryFlashOutcome::AppStarted:
      m_lblCurrentVersion->setText(tr("Current Version: %1").arg(version));
      showUpgradeSuccess(notice);
      break;

    case FactoryFlashOutcome::FactoryWithoutApp:
      m_lblCurrentVersion->setText(tr("Current Version: %1").arg(tr("Unknown")));
      showWideMessageBox(QMessageBox::Warning, tr("No Application Firmware"), noApplicationFirmwareMessage());
      break;

    case FactoryFlashOutcome::NotDetected:
      showWideMessageBox(
          QMessageBox::Warning, tr("Device Not Detected"),
          tr("The firmware was sent, but the dongle did not report back within 60 seconds.\n\n"
             "Disconnect and reconnect the dongle, then click Check for Updates.")
      );
      break;
    }
  });
}

void Esp32HidToolsWidget::showUpgradeSuccess(const QString &pairingNotice)
{
  QString message = tr("Firmware upgrade successful.");
  if (!pairingNotice.isEmpty()) {
    message += QStringLiteral("\n\n") + pairingNotice;
  }
  showWideMessageBox(QMessageBox::Information, tr("Success"), message);
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

  m_tabWidget->setTabText(0, tr("New Dongle Setup"));
  m_tabWidget->setTabText(1, tr("Upgrade"));
  m_tabWidget->setTabText(2, tr("Activation"));
  m_tabWidget->setTabText(3, tr("Order"));

  m_alreadySetUpLbl->setText(tr("This dongle is already set up."));
  m_newDongleIntroLbl->setText(
      tr("Installs DShare-HID firmware on a new dongle. You only need to do this once; after that, use the Upgrade tab "
         "for updates.")
  );
  m_factoryPathEdit->setPlaceholderText(tr("Path to factory.fzip"));
  m_factoryBrowseBtn->setText(tr("Browse..."));
  m_factoryFlashBtn->setText(tr("Flash"));
  m_downloadFlashBtn->setText(tr("Flash"));

  m_checkUpgradeBtn->setText(tr("Check for Updates"));
  m_flashOnlineBtn->setText(tr("Flash"));
  m_upgradePathEdit->setPlaceholderText(tr("Path to upgrade.uzip"));
  m_upgradeBrowseBtn->setText(tr("Browse..."));
  m_flashLocalBtn->setText(tr("Flash"));

  m_btnCopySerial->setText(tr("Copy Serial"));
  m_lineActivationKey->setPlaceholderText(tr("Paste Activation Key Here"));
  m_btnActivate->setText(tr("Activate"));

  m_orderOption2->setText(tr("Purchase Full License"));
  m_orderOption4->setText(tr("Upgrade Profile Capacity"));

  m_chkManualPayment->setText(tr("I will arrange payment via email"));
  m_btnCopyOrder->setText(tr("Copy content"));
  m_btnEmailOrder->setText(tr("Email"));

  // Update dynamic labels that might have "Unknown" or similar markers
  m_lblCurrentVersion->setText(tr("Current Version: %1").arg(m_lblCurrentVersion->text().split(": ").last()));
  m_lblLatestVersion->setText(tr("Latest Version: %1").arg(m_lblLatestVersion->text().split(": ").last()));
  m_labelActivationState->setText(tr("State: %1").arg(m_labelActivationState->text().split(": ").last()));
}

} // namespace deskflow::gui

void deskflow::gui::Esp32HidToolsWidget::onTabChanged(int index)
{
  if (index < 0)
    return;

  QString title = m_tabWidget->tabText(index);
  if (title == tr("Activation") || title == tr("Order")) {
    refreshDeviceState();
  } else if (title == tr("New Dongle Setup")) {
    checkNewDongleSetup();
  }

  // Dynamically adjust splitter sizes to avoid scrollbars while maximizing log space
  // Order tab needs more space (~650px) for full content
  if (title == tr("Order")) {
    m_splitter->setSizes({650, 350});
  } else {
    m_splitter->setSizes({300, 700});
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

  LOG_INFO("firmware tool: refreshing device state");
  m_lineSerial->clear();
  m_orderSerialLabel->setText(tr("-"));
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
      result.activationState = activationStateText(config);
      result.stateEnum = config.activationState;
      result.isActivated = (config.activationState == deskflow::bridge::ActivationState::Activated);
      result.isFactoryMode = (config.firmwareMode == deskflow::bridge::FirmwareMode::Factory);
      result.totalProfiles = config.totalProfiles;
      result.success = true;
    } else {
      result.success = false;
      result.error = QString::fromStdString(cdc.lastError());
    }

    QMetaObject::invokeMethod(this, [this, result]() {
      setControlsEnabled(true);
      if (result.success) {
        m_lineSerial->setText(result.serial);
        m_orderSerialLabel->setText(result.serial);
        m_labelActivationState->setText(
            tr("State: %1, %2 Profiles").arg(result.activationState).arg(result.totalProfiles)
        );

        // Update Order Options
        bool isLicensed = result.isActivated;

        m_orderOption2->setEnabled(!isLicensed);
        m_orderOption4->setEnabled(isLicensed && result.totalProfiles == 2);

        // Check the first valid (enabled) option
        if (m_orderOption2->isEnabled()) {
          m_orderOption2->setChecked(true);
        } else if (m_orderOption4->isEnabled()) {
          m_orderOption4->setChecked(true);
        }

        m_orderTotalProfiles->clear();
        if (result.isActivated) {
          if (result.totalProfiles == 2) {
            m_orderTotalProfiles->addItem("4", 4);
            m_orderTotalProfiles->addItem("6", 6);
            m_orderTotalProfiles->setEnabled(true);
          } else {
            m_orderTotalProfiles->addItem(QString::number(result.totalProfiles), result.totalProfiles);
            m_orderTotalProfiles->setEnabled(false);
          }
        } else {
          m_orderTotalProfiles->addItem("2", 2);
          m_orderTotalProfiles->addItem("4", 4);
          m_orderTotalProfiles->addItem("6", 6);
          m_orderTotalProfiles->setEnabled(result.totalProfiles == 4);
        }

        if (result.isFactoryMode) {
          m_labelActivationState->setText(tr("State: Factory Mode (Cannot Activate)"));
          m_groupActivationInput->setVisible(false);
          logDeviceState(tr("Device State Refreshed. Serial: %1, Mode: Factory").arg(result.serial));
        } else {
          // Conditional UI: Hide activation input if already activated
          // Conditional UI: Hide activation input if already activated AND has more than 2 profiles
          // (i.e. allow upgrade if profiles == 2)
          if (!result.isActivated || result.totalProfiles == 2) {
            m_groupActivationInput->setVisible(true);
          } else {
            m_groupActivationInput->setVisible(false);
          }
          logDeviceState(tr("Device State Refreshed. Serial: %1, State: %2, Profiles: %3")
                             .arg(result.serial, result.activationState, QString::number(result.totalProfiles)));
        }
      } else {
        m_labelActivationState->setText(tr("State: Error"));
        m_lastDeviceState.clear();
        log(tr("Failed to refresh state: %1").arg(result.error));

        // Erased Device Case: If on Order tab and handshake failed, warn user
        QTabWidget *tabs = findChild<QTabWidget *>();
        if (tabs && tabs->tabText(tabs->currentIndex()) == tr("Order")) {
          QMessageBox::warning(
              this, tr("Not Set Up"), tr("This dongle is not set up yet. Use the New Dongle Setup tab first.")
          );
        }
        // Reset Order Options
        m_orderOption2->setEnabled(false);
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
      showWideMessageBox(QMessageBox::Warning, tr("Error"), tr("Please select a valid serial port."));
      return;
    }
  }

  QString key = m_lineActivationKey->text().trimmed();
  if (key.isEmpty()) {
    showWideMessageBox(QMessageBox::Warning, tr("Error"), tr("Please enter an activation key."));
    return;
  }

  log(tr("Activating device..."));

  auto task = [this, portName, key]() {
    deskflow::bridge::CdcTransport cdc(portName);
    // Activation REQUIRES secure connection.
    if (cdc.open(false) && cdc.activateDevice(key.toStdString())) {
      QMetaObject::invokeMethod(this, [this]() {
        log(tr("Activation successful!"));
        showWideMessageBox(QMessageBox::Information, tr("Success"), tr("Device activated successfully."));
        refreshDeviceState(); // Refresh to update UI
      });
    } else {
      // User requested minimal error message
      QString detailedError = QString::fromStdString(cdc.lastError());
      if (!detailedError.isEmpty()) {
        LOG_WARN("Activation detailed error: %s", qPrintable(detailedError));
      }
      QMetaObject::invokeMethod(this, [this]() {
        log(tr("Activation failed."));
        showWideMessageBox(QMessageBox::Critical, tr("Error"), tr("Activation failed."));
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
    showWideMessageBox(QMessageBox::Warning, tr("Input Required"), tr("Please enter your name and email address."));
    return QString();
  }

  QString serial = m_orderSerialLabel->text();
  int totalProfiles = m_orderTotalProfiles->currentData().toInt();

  if (serial == tr("-") || serial.isEmpty()) {
    showWideMessageBox(
        QMessageBox::Warning, tr("Device Error"), tr("Serial number missing. Please check device connection.")
    );
    return QString();
  }

  outOption = -1;
  QString content;

  if (m_orderOption2->isChecked()) {
    outOption = 2;
    outPrefix = "full_license_";
    content = QString("Name: %1\nEmail: %2\nSerial: %3\nTotal Profiles: %4\nRequest: Purchase full license\n")
                  .arg(name, email, serial, QString::number(totalProfiles));
  } else if (m_orderOption4->isChecked()) {
    outOption = 4;
    outPrefix = "profile_";
    content =
        QString("Name: %1\nEmail: %2\nSerial: %3\nTotal Profiles: %4\nRequest: Bump profiles (Already licensed)\n")
            .arg(name, email, serial, QString::number(totalProfiles));
  } else {
    showWideMessageBox(QMessageBox::Warning, tr("Selection Required"), tr("Please select one of the order options."));
    return QString();
  }

  OrderPrice priceInfo = calculateOrderPrice(outOption, totalProfiles);
  double price = priceInfo.price;
  QString priceDesc = priceInfo.desc;

  content.append(QString("Payment Details: %1 = Total $%2 USD\n").arg(priceDesc, QString::number(price, 'f', 2)));
  content.append(QString("Paypal Seller: %1\n").arg(DESKFLOW_PAYPAL_ACCOUNT));

  if (price > 0.01) {
    if (!m_chkManualPayment->isChecked()) {
      QString transId = m_paymentTransId->text().trimmed();
      if (transId.isEmpty()) {
        showWideMessageBox(
            QMessageBox::Warning, tr("Missing Transaction ID"),
            tr("Please enter your PayPal Transaction ID for verification.")
        );
        return QString();
      }
      content.append(QString("PayPal Transaction ID: %1\n").arg(transId));
    } else {
      content.append(QString("Payment Method: Manual / Email Arrangement\n"));
    }
  }

  content.append(QString("Reference No: %1\n").arg(m_paymentRefNo->text()));

  return content;
}

deskflow::gui::OrderPrice deskflow::gui::Esp32HidToolsWidget::calculateOrderPrice(int option, int totalProfiles)
{
  double price = 0.0;
  QString priceDesc;
  QString displayDesc;

  const QString profilesPrice =
      totalProfiles == 2
          ? QStringLiteral("$0.00")
          : QString("$%1").arg(QString::number(totalProfiles == 4 ? m_prices.profile_4 : m_prices.profile_6, 'f', 2));

  if (option == 2) { // Full License
    price += m_prices.license;

    if (totalProfiles == 4) {
      price += m_prices.profile_4;
    } else if (totalProfiles == 6) {
      price += m_prices.profile_6;
    }

    double discount = 0.0;
    if (totalProfiles > 2 && m_prices.combo_discount > 0.0) {
      discount = m_prices.combo_discount;
      price -= discount;
      if (price < 0)
        price = 0; // Safety
    }

    const QString license = QString::number(m_prices.license, 'f', 2);
    priceDesc = QString("License($%1) + %2 Profiles(%3)").arg(license).arg(totalProfiles).arg(profilesPrice);
    displayDesc = tr("License($%1) + %2 Profiles(%3)").arg(license).arg(totalProfiles).arg(profilesPrice);

    if (discount > 0.0) {
      const QString amount = QString::number(discount, 'f', 2);
      priceDesc += QString(" - Combo Discount($%1)").arg(amount);
      displayDesc = tr("%1 - Combo Discount($%2)").arg(displayDesc, amount);
    }

  } else if (option == 4) { // Only Bump Profiles
    if (totalProfiles == 4)
      price += m_prices.profile_4;
    else if (totalProfiles == 6)
      price += m_prices.profile_6;

    priceDesc = QString("Upgrade profiles to %1 (%2)").arg(totalProfiles).arg(profilesPrice);
    displayDesc = tr("Upgrade profiles to %1 (%2)").arg(totalProfiles).arg(profilesPrice);
  }

  return {price, priceDesc, displayDesc};
}

void deskflow::gui::Esp32HidToolsWidget::updatePaymentDetails()
{
  int option = -1;
  if (m_orderOption2->isChecked())
    option = 2;
  else if (m_orderOption4->isChecked())
    option = 4;

  int totalProfiles = m_orderTotalProfiles->currentData().toInt();
  OrderPrice priceInfo = calculateOrderPrice(option, totalProfiles);

  if (option == -1) {
    m_lblPaymentDetails->setText(tr("Select an option"));
    return;
  }

  m_lblPaymentDetails->setText(
      tr("Payment Details: %1 = Total $%2 USD").arg(priceInfo.displayDesc, QString::number(priceInfo.price, 'f', 2))
  );

  m_lblPaymentOwner->setText(
      QString(tr("Paypal Seller: <b><font color='red'>%1</font></b>")).arg(DESKFLOW_PAYPAL_ACCOUNT)
  );
  m_lblPaymentOwner->setVisible(true);
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
  if (m_orderOption2->isChecked())
    option = 2;
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

  QString itemName = QString("DShare-HID: %1").arg(priceDesc);

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

  if (showWideMessageBox(QMessageBox::Question, tr("Confirm Payment"), msg, QMessageBox::Yes | QMessageBox::No) !=
      QMessageBox::Yes) {
    return;
  }

  if (!QDesktopServices::openUrl(url)) {
    showWideMessageBox(
        QMessageBox::Warning, tr("Error"), tr("Failed to open web browser. Please visit PayPal manually.")
    );
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
  showWideMessageBox(QMessageBox::Information, tr("Copied"), tr("Order content successfully copied to clipboard."));
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
  QString subject = QString("DShare Order: %1 %2").arg(prefix, serial);

  // URL encode content and subject
  QUrl url(QString("mailto:dshare.hid@gmail.com"));
  QUrlQuery query;
  query.addQueryItem("subject", subject);
  query.addQueryItem("body", content);
  url.setQuery(query);

  if (QDesktopServices::openUrl(url)) {
    log(tr("Email client opened."));
  } else {
    log(tr("Failed to open email client."));
    showWideMessageBox(QMessageBox::Critical, tr("Error"), tr("Failed to open your default email client."));
  }
}

void deskflow::gui::Esp32HidToolsWidget::onPortChanged(int index)
{
  (void)index;
  QString portName = m_portCombo->currentData().toString();
  if (portName.isEmpty()) {
    portName = m_portCombo->currentText();
  }

  LOG_INFO("Port selection changed to: %s", qPrintable(portName));

  // Reset UI metadata
  m_lblCurrentVersion->setText(tr("Current Version: Unknown"));
  m_lblLatestVersion->setText(tr("Latest Version: Unknown"));
  m_onlineFlashAllowed = false;
  m_flashOnlineBtn->setEnabled(false);
  m_lineSerial->clear();
  m_orderSerialLabel->setText(tr("-"));
  m_labelActivationState->setText(tr("State: Unknown"));
  m_lastDeviceState.clear();

  // Reset Order Options
  m_orderOption2->setEnabled(false);
  m_orderOption4->setEnabled(false);
  m_orderTotalProfiles->setEnabled(false);

  // If on Activation or Order tab, auto-refresh state; the New Dongle Setup tab asks the new port's dongle
  QTabWidget *tabs = findChild<QTabWidget *>();
  if (tabs &&
      (tabs->tabText(tabs->currentIndex()) == tr("Activation") || tabs->tabText(tabs->currentIndex()) == tr("Order"))) {
    refreshDeviceState();
  } else if (tabs && tabs->tabText(tabs->currentIndex()) == tr("New Dongle Setup")) {
    checkNewDongleSetup();
  }
}
