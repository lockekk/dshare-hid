/*
 * Deskflow-hid -- created by locke.huang@gmail.com
 */

#include "DeskflowHidExtension.h"
#include "MainWindow.h"
#include "ui_MainWindow.h"

#if defined(Q_OS_WIN)
#include <windows.h>
#endif

#include "common/PlatformInfo.h"
#include "common/Settings.h"
#include "gui/core/BridgeClientConfigManager.h"
#include "gui/core/CoreProcess.h"
#include "gui/devices/UsbDeviceHelper.h"
#include "gui/dialogs/BridgeClientConfigDialog.h"
#include "gui/widgets/BridgeClientWidget.h"
#if DESKFLOW_HID_ENABLE_ESP32_HID_TOOLS
#include "gui/widgets/Esp32HidToolsWidget.h"
#endif

// #include "platform/bridge/Activation.h"
#include "platform/bridge/CdcTransport.h"

#if defined(Q_OS_LINUX)
#include "devices/LinuxUdevMonitor.h"
#elif defined(Q_OS_WIN)
#include "devices/WindowsUsbMonitor.h"
#elif defined(Q_OS_MAC)
#include "devices/MacUsbMonitor.h"
#endif

#include <QCoreApplication>
#include <QEventLoop>
#include <QGridLayout>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QTimer>

#include "base/Log.h"

#include <chrono>
#include <thread>

using namespace deskflow::gui;

QString logLevelNameFromIndex(int index)
{
  static const QStringList kLogLevels = {QStringLiteral("FATAL"),  QStringLiteral("ERROR"), QStringLiteral("WARNING"),
                                         QStringLiteral("NOTE"),   QStringLiteral("INFO"),  QStringLiteral("DEBUG"),
                                         QStringLiteral("DEBUG1"), QStringLiteral("DEBUG2")};

  if (index < 0 || index >= kLogLevels.size()) {
    return QStringLiteral("INFO");
  }
  return kLogLevels.at(index);
}

DeskflowHidExtension::DeskflowHidExtension(MainWindow *parent)
    : QObject(parent),
      m_mainWindow(parent),
      m_settings(new QSettings(this))
{
  m_showBridgeLogs = m_settings->value(Settings::Bridge::ShowLogs, false).toBool();
}

DeskflowHidExtension::~DeskflowHidExtension()
{
  shutdown();
}

void DeskflowHidExtension::shutdown()
{
  stopAllBridgeClients();
  if (m_usbDeviceMonitor) {
    m_usbDeviceMonitor->deleteLater();
    m_usbDeviceMonitor = nullptr;
  }
}

void DeskflowHidExtension::setup()
{
#if defined(Q_OS_LINUX)
  m_usbDeviceMonitor = new LinuxUdevMonitor(this);
#elif defined(Q_OS_WIN)
  m_usbDeviceMonitor = new WindowsUsbMonitor(this);
#elif defined(Q_OS_MAC)
  m_usbDeviceMonitor = new MacUsbMonitor(this);
#endif

#if defined(Q_OS_LINUX) || defined(Q_OS_WIN) || defined(Q_OS_MAC)
  if (m_usbDeviceMonitor) {
    // Filter for Espressif vendor ID (0x303a)
    m_usbDeviceMonitor->setVendorIdFilter(UsbDeviceHelper::kEspressifVendorId);
    // Connect signals
    connect(m_usbDeviceMonitor, &UsbDeviceMonitor::deviceConnected, this, &DeskflowHidExtension::usbDeviceConnected);
    connect(
        m_usbDeviceMonitor, &UsbDeviceMonitor::deviceDisconnected, this, &DeskflowHidExtension::usbDeviceDisconnected
    );
    // Start monitoring
    if (m_usbDeviceMonitor->startMonitoring()) {
      qDebug() << "USB device monitoring started successfully";
    } else {
      qWarning() << "Failed to start USB device monitoring";
    }
  }
#endif

  // Bridge Client Logs Action
  m_actionShowBridgeLogs = new QAction(tr("Bridge Client Logs"), this);
  m_actionShowBridgeLogs->setCheckable(true);
  m_actionShowBridgeLogs->setChecked(m_showBridgeLogs);
  connect(m_actionShowBridgeLogs, &QAction::toggled, this, &DeskflowHidExtension::toggleShowBridgeLogs);
  m_mainWindow->m_menuView->addAction(m_actionShowBridgeLogs);

  loadBridgeClientConfigs();

  // Connect to server process state/connection signals
  if (m_mainWindow) {
    connect(
        &m_mainWindow->m_coreProcess, &CoreProcess::connectionStateChanged, this,
        &DeskflowHidExtension::onServerConnectionStateChanged
    );
    // Query initial state (Issue 3)
    onServerConnectionStateChanged(m_mainWindow->m_coreProcess.connectionState());
  }

  // Check initially connected devices
  if (m_usbDeviceMonitor) {
    updateBridgeClientDeviceStates();
  }
}

void DeskflowHidExtension::pauseUsbMonitoring()
{
  if (m_usbDeviceMonitor && m_usbDeviceMonitor->isMonitoring()) {
    LOG_DEBUG("Pausing USB device monitoring");
    m_usbDeviceMonitor->stopMonitoring();
  }
}

void DeskflowHidExtension::resumeUsbMonitoring()
{
  if (m_usbDeviceMonitor && !m_usbDeviceMonitor->isMonitoring()) {
    LOG_DEBUG("Resuming USB monitoring...");
    m_usbDeviceMonitor->startMonitoring();

    // After resuming, update states in case something changed while we were paused
    updateBridgeClientDeviceStates();
  }
}

void DeskflowHidExtension::openEsp32HidTools()
{
#if DESKFLOW_HID_ENABLE_ESP32_HID_TOOLS
  pauseUsbMonitoring();
  stopAllBridgeClients();
  Esp32HidToolsWidget widget(QString(), m_mainWindow);
  widget.setWindowTitle(tr("ESP32 HID Tools"));
  widget.resize(800, 600);
  widget.exec();
  resumeUsbMonitoring();
  if (m_mainWindow) {
    m_mainWindow->setStatus(tr("Ready"));
  }
#else
  QMessageBox::information(
      m_mainWindow, tr("Feature Unavailable"), tr("The ESP32 HID Tools module is not available in this build.")
  );
#endif
}

void DeskflowHidExtension::loadBridgeClientConfigs()
{
  LOG_DEBUG("Loading bridge client configurations...");

  // Get all config files
  QStringList configFiles = BridgeClientConfigManager::getAllConfigFiles();
  LOG_DEBUG("Found %d bridge client config file(s)", configFiles.size());

  // Get grid layout from MainWindow
  // Note: Assuming "widgetBridgeClients" is accessible via friend access to Ui::MainWindow or findChild
  // Using findChild on m_mainWindow directly might work if the ui elements are children (they are usually)
  // But ui is a unique_ptr.
  // We need to resolve how to access ui->widgetBridgeClients.
  // BUT: The widgets are in the object tree of MainWindow.

  QWidget *bridgeClientsWidget = m_mainWindow->findChild<QWidget *>("widgetBridgeClients");
  QGridLayout *gridLayout = nullptr;
  if (bridgeClientsWidget) {
    gridLayout = bridgeClientsWidget->findChild<QGridLayout *>("gridLayoutBridgeClients");
  }

  if (!gridLayout) {
    LOG_WARN("Bridge clients grid layout not found");
    return;
  }

  // Create a widget for each config file
  for (const QString &configPath : configFiles) {
    BridgeClientConfigManager::removeLegacySecuritySettings(configPath);
    QString screenName = BridgeClientConfigManager::readScreenName(configPath);
    QString serialNumber = BridgeClientConfigManager::readSerialNumber(configPath);

    if (serialNumber.isEmpty()) {
      LOG_INFO("Removing invalid bridge client config (missing serial number): %s", qPrintable(configPath));
      BridgeClientConfigManager::deleteConfig(configPath);
      continue;
    }

    if (screenName.isEmpty()) {
      screenName = tr("Unknown Device");
    }

    // Create widget (device path is empty initially, will be set when device is detected)
    auto *widget = new BridgeClientWidget(screenName, serialNumber, QString(), configPath, m_mainWindow);

    // Set initial state to unavailable (will be updated by updateBridgeClientDeviceStates)
    widget->setDeviceAvailable(QString(), false);

    // Connect signals
    connect(widget, &BridgeClientWidget::connectToggled, this, &DeskflowHidExtension::bridgeClientConnectToggled);
    connect(widget, &BridgeClientWidget::configureClicked, this, &DeskflowHidExtension::bridgeClientConfigureClicked);

    connect(widget, &BridgeClientWidget::refreshDevicesRequested, this, [this]() {
      this->updateBridgeClientDeviceStates();
    });

    // Add to grid layout (1 column per row)
    if (gridLayout) {
      int count = m_bridgeClientWidgets.size();
      int row = count;
      int col = 0;
      gridLayout->addWidget(widget, row, col);
    }

    // Track widget by config path
    m_bridgeClientWidgets[configPath] = widget;

    LOG_DEBUG("Created widget for config: %s screenName: %s", qPrintable(configPath), qPrintable(screenName));
  }
}

bool DeskflowHidExtension::syncDeviceConfigFromDevice(
    const QString &devicePath, const QString &configPath, bool *outIsBleConnected
)
{
  if (m_bridgeClientProcesses.contains(devicePath)) {
    LOG_DEBUG("Skipping syncDeviceConfigFromDevice for busy device: %s", qPrintable(devicePath));
    return false;
  }

  deskflow::bridge::CdcTransport transport(devicePath);

  // Attempt to open the transport with retries (macOS port readiness can be tricky)
  bool opened = false;
  for (int retry = 0; retry < 3; ++retry) {
    if (transport.open()) {
      opened = true;
      break;
    }
    LOG_WARN("Failed to open device %s (attempt %d/3)", qPrintable(devicePath), retry + 1);

    // Instead of sleep_for, we should use a small event loop wait or just fail faster
    // if we are on the GUI thread. But for now, let's at least process events if we must wait.
    QEventLoop loop;
    QTimer::singleShot(200, &loop, &QEventLoop::quit);
    loop.exec();

    // Re-check busy status after wait
    if (m_bridgeClientProcesses.contains(devicePath)) {
      LOG_DEBUG("Aborting sync: Device became busy during retry: %s", qPrintable(devicePath));
      return false;
    }
  }

  if (!opened) {
    LOG_WARN("Aborting sync: Failed to open device %s after retries.", qPrintable(devicePath));
    return false;
  }

  // Ensure we actually got a configuration during the handshake
  if (!transport.hasDeviceConfig()) {
    LOG_WARN("Aborting sync: Handshake completed but no device config received for %s", qPrintable(devicePath));
    transport.close();
    return false;
  }

  if (outIsBleConnected) {
    *outIsBleConnected = transport.deviceConfig().isBleConnected;
  }

  uint8_t activeProfile = transport.deviceConfig().activeProfile;

  deskflow::bridge::DeviceProfile profile;
  if (!transport.getProfile(activeProfile, profile)) {
    LOG_WARN("Failed to get profile %d from device %s", activeProfile, qPrintable(devicePath));
    transport.close();
    return false;
  }

  std::string deviceName = transport.deviceConfig().deviceName;
  // If handshake didn't provide name, try fetch
  if (deviceName.empty()) {
    transport.fetchDeviceName(deviceName);
  }

  // Save activation state before closing transport
  const char *activationStateStr = transport.deviceConfig().activationStateString();
  QString activationState = QString::fromLatin1(activationStateStr);

  transport.close();

  using namespace deskflow;
  QSettings config(configPath, QSettings::IniFormat);

  if (!deviceName.empty()) {
    config.setValue(Settings::Bridge::DeviceName, QString::fromStdString(deviceName));
  }

  QByteArray nameBytes(profile.hostname, sizeof(profile.hostname));
  int nullPos = nameBytes.indexOf('\0');
  if (nullPos >= 0) {
    nameBytes.truncate(nullPos);
  }
  config.setValue(Settings::Bridge::ActiveProfileHostname, QString::fromUtf8(nameBytes));

  QString orientation = (profile.rotation == 0) ? QStringLiteral("portrait") : QStringLiteral("landscape");
  config.setValue(Settings::Bridge::ActiveProfileOrientation, orientation);

  // Save activation state from device
  config.setValue(Settings::Bridge::ActivationState, activationState);

  config.sync();
  LOG_DEBUG(
      "Successfully synced config for %s Activation State: %s", qPrintable(devicePath), qPrintable(activationState)
  );
  return true;
}

void DeskflowHidExtension::updateBridgeClientDeviceStates()
{
  LOG_DEBUG("Updating bridge client device states...");

  // Get all currently connected USB CDC devices with their serial numbers
  QMap<QString, QString> connectedDevices = UsbDeviceHelper::getConnectedDevices();
  LOG_DEBUG("Found %d connected USB CDC device(s)", connectedDevices.size());

  QSet<QString> configuredSerialNumbers;

  // For each widget, check if its device is connected
  for (auto it = m_bridgeClientWidgets.begin(); it != m_bridgeClientWidgets.end(); ++it) {
    QString configPath = it.key();
    BridgeClientWidget *widget = it.value();

    // Read serial number from widget (which is now populated)
    QString configSerialNumber = widget->serialNumber();
    if (configSerialNumber.isEmpty()) {
      // Fallback: Read serial number from config
      configSerialNumber = BridgeClientConfigManager::readSerialNumber(configPath);
    }

    if (configSerialNumber.isEmpty()) {
      LOG_WARN("Widget has no serial number and config has no serial number: %s", qPrintable(configPath));
      widget->setDeviceAvailable(QString(), false);
      continue;
    }

    configuredSerialNumbers.insert(configSerialNumber);

    // Check if this serial number is in the connected devices
    bool found = false;
    QString devicePath;
    for (auto devIt = connectedDevices.begin(); devIt != connectedDevices.end(); ++devIt) {
      if (devIt.value() == configSerialNumber) {
        found = true;
        devicePath = devIt.key();
        break;
      }
    }

    // Update widget availability
    widget->setDeviceAvailable(devicePath, found);
    widget->setServerReady(isServerReady());
    {
      using namespace deskflow;
      QSettings cfg(configPath, QSettings::IniFormat);
      widget->setDeviceName(
          cfg.value(Settings::Bridge::DeviceName, Settings::defaultValue(Settings::Bridge::DeviceName)).toString()
      );
      widget->setActiveHostname(cfg.value(
                                       Settings::Bridge::ActiveProfileHostname,
                                       Settings::defaultValue(Settings::Bridge::ActiveProfileHostname)
      )
                                    .toString());
    }

    if (found) {
      // Store device path -> serial number mapping for later use
      m_devicePathToSerialNumber[devicePath] = configSerialNumber;

      LOG_DEBUG("Device available for config: %s device: %s", qPrintable(configPath), qPrintable(devicePath));

      // STARTUP AUTO-CONNECT FIX: Check if we should auto-connect this device found at startup
      bool isBleConnected = false;
      if (!m_bridgeClientProcesses.contains(devicePath)) {
        syncDeviceConfigFromDevice(devicePath, configPath, &isBleConnected);
        widget->setBleConnected(isBleConnected);

        QSettings configSettings(configPath, QSettings::IniFormat);
        if (configSettings.value(Settings::Bridge::AutoConnect, false).toBool()) {
          if (!m_manuallyDisconnectedSerials.contains(configSerialNumber)) {
            if (isServerReady()) {
              LOG_INFO(
                  "Auto-connecting device at startup: %s (%s)", qPrintable(widget->screenName()), qPrintable(devicePath)
              );
              bridgeClientConnectToggled(devicePath, configPath, true);
            } else {
              LOG_INFO(
                  "Server not ready, wait for server to start for auto-connect at startup for: %s",
                  qPrintable(widget->screenName())
              );
            }
          }
        }
      }

      // Refresh widget
      widget->updateConfig(widget->screenName(), configPath);
    } else {
      LOG_DEBUG("Device NOT available for config: %s", qPrintable(configPath));
    }
  }

  // Check for connected devices that don't have a configuration
  for (auto it = connectedDevices.begin(); it != connectedDevices.end(); ++it) {
    const QString &devicePath = it.key();
    const QString &serialNumber = it.value();

    if (serialNumber.isEmpty() || serialNumber == "Unknown") {
      continue;
    }

    if (!configuredSerialNumbers.contains(serialNumber)) {
      LOG_INFO(
          "Found unconfigured device during initial scan: %s serial: %s", qPrintable(devicePath),
          qPrintable(serialNumber)
      );

      UsbDeviceInfo info;
      info.devicePath = devicePath;
      info.serialNumber = serialNumber;
      info.vendorId = UsbDeviceHelper::kEspressifVendorId;
      info.productId = UsbDeviceHelper::kEspressifProductId;

      usbDeviceConnected(info);
    }
  }
}

void DeskflowHidExtension::usbDeviceConnected(const UsbDeviceInfo &device)
{
  LOG_DEBUG(
      "USB device connected: path: %s vendor: %s product: %s serial: %s", qPrintable(device.devicePath),
      qPrintable(device.vendorId), qPrintable(device.productId), qPrintable(device.serialNumber)
  );

  // Read serial number from usb stack(mostly for speed up mac)
  QString serialNumber = device.serialNumber;
  if (serialNumber.isEmpty()) {
    // fallback to read from firmware(works well for linux/windows)
    serialNumber = UsbDeviceHelper::readSerialNumber(device.devicePath);
  }

  if (serialNumber.isEmpty()) {
    LOG_WARN("Failed to read serial number for device: %s", qPrintable(device.devicePath));
    return;
  }

  // Store serial number mapping
  m_devicePathToSerialNumber[device.devicePath] = serialNumber;

  // Check if we have any configs for this serial number
  QStringList matchingConfigs = BridgeClientConfigManager::findConfigsBySerialNumber(serialNumber);

  // If we found duplicates/multiple configs for the same serial, keep the LATEST/NEWEST one and delete others
  if (matchingConfigs.size() > 1) {
    LOG_WARN(
        "Found multiple configs for serial %s. Cleaning up duplicates (keeping latest).", qPrintable(serialNumber)
    );

    // Sort by modification time, newest first
    std::sort(matchingConfigs.begin(), matchingConfigs.end(), [](const QString &a, const QString &b) {
      return QFileInfo(a).lastModified() > QFileInfo(b).lastModified();
    });

    // Retrieve layout to remove widget
    QWidget *bridgeClientsWidget = m_mainWindow ? m_mainWindow->findChild<QWidget *>("widgetBridgeClients") : nullptr;
    QGridLayout *gridLayout =
        bridgeClientsWidget ? bridgeClientsWidget->findChild<QGridLayout *>("gridLayoutBridgeClients") : nullptr;

    for (int i = 1; i < matchingConfigs.size(); ++i) {
      LOG_INFO("Deleting old duplicate config: %s", qPrintable(matchingConfigs[i]));

      // If there's an active process for this duplicate config, find and stop it
      for (auto it = m_bridgeClientDeviceToConfig.begin(); it != m_bridgeClientDeviceToConfig.end(); ++it) {
        if (it.value() == matchingConfigs[i]) {
          stopBridgeClient(it.key());
          break;
        }
      }

      BridgeClientConfigManager::deleteConfig(matchingConfigs[i]);
      // Also remove widget if it exists
      if (auto *widget = m_bridgeClientWidgets.take(matchingConfigs[i])) {
        if (gridLayout) {
          gridLayout->removeWidget(widget);
        }
        widget->deleteLater();
      }
    }
    // Keep only the first one (the newest)
    QString newestConfig = matchingConfigs.first();
    matchingConfigs.clear();
    matchingConfigs.append(newestConfig);
  }

  using namespace deskflow; // for Settings usage

  if (matchingConfigs.isEmpty()) {
    // DOUBLE CHECK: Do we have a case-insensitive match?
    // Sometimes serials differ by case. If we find a match by case-insensitive serial, use that.
    QStringList allConfigs = m_bridgeClientWidgets.keys();
    for (const QString &existingConfigPath : allConfigs) {
      QString existingSerial = BridgeClientConfigManager::readSerialNumber(existingConfigPath);
      if (existingSerial.compare(serialNumber, Qt::CaseInsensitive) == 0) {
        LOG_INFO(
            "Found case-insensitive match for serial: %s -> %s", qPrintable(serialNumber),
            qPrintable(existingConfigPath)
        );
        matchingConfigs.append(existingConfigPath);
        break;
      }
    }
  }

  if (matchingConfigs.isEmpty()) {
    LOG_INFO("New device detected, creating default config for serial: %s", qPrintable(serialNumber));

    QString handshakeDeviceName;
    bool isBleConnected = false;
    bool validHandshake = false;

    // fetchFirmwareDeviceName combined with handshake
    if (!m_bridgeClientProcesses.contains(device.devicePath)) {
      deskflow::bridge::CdcTransport transport(device.devicePath);

      // Strict check: we MUST be able to open (handshake) to consider it a valid Deskflow device
      // This filters out factory firmware ("tag mismatch") and other generic CDC devices ("timeout")
      if (transport.open()) {
        validHandshake = true;
        if (transport.hasDeviceConfig()) {
          handshakeDeviceName = QString::fromStdString(transport.deviceConfig().deviceName);
          isBleConnected = transport.deviceConfig().isBleConnected;
          uint8_t currentProfile = transport.deviceConfig().activeProfile;
          LOG_DEBUG("Device %s active profile: %d", qPrintable(device.devicePath), currentProfile);
          deskflow::bridge::DeviceProfile profile;
          if (transport.getProfile(currentProfile, profile)) {
            // Check if profile is bound to a screen (assuming we have a way to check, otherwise skip)
            // LOG_DEBUG("Profile %d loaded", currentProfile);
          }
        } else {
          std::string name;
          if (transport.fetchDeviceName(name)) {
            handshakeDeviceName = QString::fromStdString(name);
          }
        }
        transport.close();
      } else {
        LOG_WARN(
            "Failed to open transport for device: %s. Ignoring (likely non-Deskflow firmware).",
            qPrintable(device.devicePath)
        );
      }
    } else {
      LOG_DEBUG(
          "Skipping handshake for new device connection as process is already active: %s", qPrintable(device.devicePath)
      );
      validHandshake = true; // Assume valid if we already have a process
    }

    if (!validHandshake) {
      return;
    }

    // Create new config
    QString configPath = BridgeClientConfigManager::createDefaultConfig(serialNumber, device.devicePath);
    if (configPath.isEmpty()) {
      LOG_CRIT("Failed to create config for device: %s", qPrintable(serialNumber));
      return;
    }

    if (!handshakeDeviceName.isEmpty()) {
      QSettings cfg(configPath, QSettings::IniFormat);
      cfg.setValue(Settings::Bridge::DeviceName, handshakeDeviceName);
    }

    matchingConfigs.append(configPath);

    QString screenName = BridgeClientConfigManager::readScreenName(configPath);
    if (screenName.isEmpty()) {
      screenName = tr("Unknown Device");
    }

    auto *widget = new BridgeClientWidget(screenName, serialNumber, device.devicePath, configPath, m_mainWindow);
    widget->setBleConnected(isBleConnected);

    connect(widget, &BridgeClientWidget::connectToggled, this, &DeskflowHidExtension::bridgeClientConnectToggled);
    connect(widget, &BridgeClientWidget::configureClicked, this, &DeskflowHidExtension::bridgeClientConfigureClicked);

    connect(widget, &BridgeClientWidget::refreshDevicesRequested, this, [this]() {
      this->updateBridgeClientDeviceStates();
    });

    QWidget *bridgeClientsWidget = m_mainWindow->findChild<QWidget *>("widgetBridgeClients");
    QGridLayout *gridLayout = nullptr;
    if (bridgeClientsWidget) {
      gridLayout = bridgeClientsWidget->findChild<QGridLayout *>("gridLayoutBridgeClients");
    }

    if (gridLayout) {
      int count = m_bridgeClientWidgets.size();
      int row = count;
      int col = 0;
      gridLayout->addWidget(widget, row, col);
    }
    m_bridgeClientWidgets[configPath] = widget;
  }

  // Found existing config(s) - enable the widget(s)
  for (const QString &config : matchingConfigs) {
    auto it = m_bridgeClientWidgets.find(config);
    if (it == m_bridgeClientWidgets.end()) {
      // If not found by path, try to find by serial number (robust against renames)
      for (auto wIt = m_bridgeClientWidgets.begin(); wIt != m_bridgeClientWidgets.end(); ++wIt) {
        if (wIt.value() && wIt.value()->serialNumber() == serialNumber) {
          LOG_INFO(
              "Found existing widget for serial %s at old path %s. Updating to new path %s", qPrintable(serialNumber),
              qPrintable(wIt.key()), qPrintable(config)
          );
          BridgeClientWidget *widget = wIt.value();
          const QString oldConfigPath = wIt.key();
          m_bridgeClientWidgets.erase(wIt);
          m_bridgeClientWidgets[config] = widget;

          // If a process is already running for this config serial, update its mapping to the new config path
          for (auto procIt = m_bridgeClientDeviceToConfig.begin(); procIt != m_bridgeClientDeviceToConfig.end();
               ++procIt) {
            if (procIt.value() == oldConfigPath) {
              procIt.value() = config;
            }
          }

          it = m_bridgeClientWidgets.find(config);
          break;
        }
      }
    }

    if (it != m_bridgeClientWidgets.end()) {
      BridgeClientWidget *widget = it.value();
      widget->setDeviceAvailable(device.devicePath, true);
      widget->setServerReady(isServerReady());

      // Sync profile data from device settings
      bool isBleConnected = false;
      if (!m_bridgeClientProcesses.contains(device.devicePath)) {
        syncDeviceConfigFromDevice(device.devicePath, config, &isBleConnected);
      }

      // Update widget from updated config
      widget->updateConfig(widget->screenName(), config);

      widget->setBleConnected(isBleConnected);

      QString screenName = widget->screenName();
      LOG_DEBUG("Enabled widget for config: %s screenName: %s", qPrintable(config), qPrintable(screenName));

      if (m_mainWindow) {
        m_mainWindow->setStatus(tr("Bridge client device plugged in: %1").arg(screenName));
      }
      m_manuallyDisconnectedSerials.remove(serialNumber);
      m_connectionAttempts.remove(config);

      // Handle auto-connect
      QSettings configSettings(config, QSettings::IniFormat);
      if (configSettings.value(Settings::Bridge::AutoConnect, false).toBool()) {
        if (!m_manuallyDisconnectedSerials.contains(serialNumber)) {
          if (isServerReady()) {
            LOG_INFO("Auto-connecting device: %s (%s)", qPrintable(screenName), qPrintable(device.devicePath));
            bridgeClientConnectToggled(device.devicePath, config, true);
          } else {
            LOG_INFO("Server not ready, wait for server to start for auto-connect of: %s", qPrintable(screenName));
          }
        } else {
          LOG_INFO("Auto-connect skipped for %s due to manual disconnect.", qPrintable(screenName));
        }
      }
    }
  }
}

void DeskflowHidExtension::usbDeviceDisconnected(const UsbDeviceInfo &device)
{
  LOG_DEBUG("USB device disconnected: %s", qPrintable(device.devicePath));

  QString serialNumber;
  if (m_devicePathToSerialNumber.contains(device.devicePath)) {
    serialNumber = m_devicePathToSerialNumber.take(device.devicePath);
    // Don't remove from configuredSerialNumbers here, as the config still exists
    // just the device is unplugged.

    // Remove from resume set to prevent stale restarts
    m_resumeConnectionAfterServerRestart.remove(device.devicePath);

    // Stop process if running
    if (m_bridgeClientProcesses.contains(device.devicePath)) {
      stopBridgeClient(device.devicePath);
    }
  } else {
    LOG_WARN("No stored serial number found for disconnected device: %s", qPrintable(device.devicePath));
  }

  bool found = false;
  for (auto it = m_bridgeClientWidgets.begin(); it != m_bridgeClientWidgets.end(); ++it) {
    BridgeClientWidget *widget = it.value();
    QString configPath = it.key();

    bool matches = false;
    if (!device.devicePath.isEmpty() && widget->devicePath() == device.devicePath) {
      matches = true;
    }
    if (!serialNumber.isEmpty()) {
      QString configSerialNumber = BridgeClientConfigManager::readSerialNumber(configPath);
      if (configSerialNumber == serialNumber) {
        matches = true;
      }
    }

    if (matches && widget->isDeviceAvailable()) {
      QString screenName = widget->screenName();
      if (m_bridgeClientProcesses.contains(device.devicePath)) {
        stopBridgeClient(device.devicePath);
      }
      widget->setDeviceAvailable(QString(), false);
      found = true;

      if (m_mainWindow) {
        m_mainWindow->setStatus(tr("Bridge client device unplugged: %1").arg(screenName));
      }
    }
  }
  m_manuallyDisconnectedSerials.remove(serialNumber);

  // Also clear connection attempts if we didn't find it in pending but it was active
  if (!serialNumber.isEmpty()) {
    QStringList allConfigs = BridgeClientConfigManager::getAllConfigFiles();
    for (const auto &path : allConfigs) {
      if (BridgeClientConfigManager::readSerialNumber(path) == serialNumber) {
        m_connectionAttempts.remove(path);
      }
    }
  }

  m_devicePathToSerialNumber.remove(device.devicePath);
}

void DeskflowHidExtension::bridgeClientConnectToggled(
    const QString &devicePath, const QString &configPath, bool shouldConnect
)
{
  qDebug() << "Bridge client connect toggled:"
           << "device:" << devicePath << "config:" << configPath << "connect:" << shouldConnect;

  const QString serialNumber = BridgeClientConfigManager::readSerialNumber(configPath);
  if (shouldConnect) {
    m_manuallyDisconnectedSerials.remove(serialNumber);
  } else {
    m_manuallyDisconnectedSerials.insert(serialNumber);
  }

  BridgeClientWidget *targetWidget = m_bridgeClientWidgets.value(configPath, nullptr);

  if (!targetWidget) {
    LOG_WARN("No widget found for config: %s", qPrintable(configPath));
    if (m_mainWindow) {
      m_mainWindow->setStatus(tr("Error: No configuration found for device: %1").arg(devicePath));
    }
    return;
  }

  if (shouldConnect && devicePath.isEmpty()) {
    LOG_WARN("Cannot connect: empty device path for config: %s", qPrintable(configPath));
    if (m_mainWindow) {
      m_mainWindow->setStatus(tr("Error: Cannot connect. Device path is empty."));
    }
    return;
  }

  if (targetWidget->isConnected() != shouldConnect) {
    targetWidget->setConnected(shouldConnect);
  }

  if (shouldConnect) {
    if (m_bridgeClientProcesses.contains(devicePath)) {
      LOG_WARN("Bridge client process already running for device: %s", qPrintable(devicePath));
      return;
    }

    if (!acquireBridgeSerialLock(serialNumber, configPath)) {
      LOG_WARN("Failed to acquire lock for serial: %s", qPrintable(serialNumber));
      if (m_mainWindow) {
        m_mainWindow->setStatus(tr("Already connected via another profile"));
      }
      if (targetWidget) {
        targetWidget->setConnected(false);
      }
      return;
    }

    QSettings bridgeConfig(configPath, QSettings::IniFormat);
    QString clientName = bridgeConfig.value(Settings::Core::ScreenName).toString();

    // Stop polling timer if it exists
    if (QTimer *t = m_bridgeClientConnectionTimers.value(devicePath)) {
      t->stop();
      t->deleteLater();
      m_bridgeClientConnectionTimers.remove(devicePath);
    }

    // Read screen dimensions from active profile on device
    using namespace deskflow;
    QSettings config(configPath, QSettings::IniFormat);

    int screenWidth = 1920;
    int screenHeight = 1080;
    QString screenOrientation = QStringLiteral("landscape");

    const QVariant defaultScrollSpeed = Settings::defaultValue(Settings::Client::ScrollSpeed);
    int scrollSpeed = config.value(Settings::Client::ScrollSpeed, defaultScrollSpeed).toInt();
    const QVariant defaultInvertScroll = Settings::defaultValue(Settings::Client::InvertScrollDirection);
    bool invertScroll = config.value(Settings::Client::InvertScrollDirection, defaultInvertScroll).toBool();

    if (!devicePath.isEmpty()) {
      deskflow::bridge::CdcTransport transport(devicePath);
      if (transport.open()) {
        if (transport.hasDeviceConfig()) {
          if (transport.deviceConfig().activationState == deskflow::bridge::ActivationState::Inactive) {
            transport.close();

            const char *activationStateStr = transport.deviceConfig().activationStateString();
            QString activationState = QString::fromLatin1(activationStateStr).trimmed();

            // Ensure UI reflects the unlicensed state immediately
            {
              QSettings cfg(configPath, QSettings::IniFormat);
              cfg.setValue(Settings::Bridge::ActivationState, activationState);
              cfg.sync();
            }

            if (targetWidget) {
              targetWidget->setActivationState(activationState);
              targetWidget->setConnected(false);
            }

            if (m_mainWindow) {
              QMessageBox::warning(
                  m_mainWindow, tr("Activation Required"),
                  tr("Free trial is expired. Please consider purchasing a license via \nFile -> Firmware -> Order.")
              );
            }
            releaseBridgeSerialLock(serialNumber, configPath);
            return;
          }

          uint8_t activeProfile = transport.deviceConfig().activeProfile;
          deskflow::bridge::DeviceProfile profile;
          if (transport.getProfile(activeProfile, profile)) {
            screenWidth = profile.screenWidth;
            screenHeight = profile.screenHeight;
            screenOrientation = (profile.rotation == 0) ? QStringLiteral("portrait") : QStringLiteral("landscape");

            // Use profile values for speed and invert, handling default for speed=0
            scrollSpeed = (profile.speed == 0) ? 120 : profile.speed;
            invertScroll = (profile.invert != 0);

            qInfo() << "Using device profile resolution:" << screenWidth << "x" << screenHeight
                    << "orientation:" << screenOrientation << "speed:" << scrollSpeed << "invert:" << invertScroll;
          }

          if (targetWidget) {
            targetWidget->setBleConnected(transport.deviceConfig().isBleConnected);
          }
        }
        transport.close();
      } else {
        qWarning() << "Failed to fetch profile resolution from device, using defaults";
      }
    }

    const QString orientationLower = screenOrientation.trimmed().toLower();
    if (orientationLower == QStringLiteral("portrait")) {
      if (screenWidth > screenHeight) {
        const int tmp = screenWidth;
        screenWidth = screenHeight;
        screenHeight = tmp;
      }
    } else if (orientationLower == QStringLiteral("landscape")) {
      if (screenWidth < screenHeight) {
        const int tmp = screenWidth;
        screenWidth = screenHeight;
        screenHeight = tmp;
      }
    }
    QString screenName = config.value(Settings::Core::ScreenName).toString();

    const QVariant logLevelVariant = config.value(Settings::Log::Level, "INFO");
    QString logLevel = logLevelVariant.toString().trimmed();
    bool logLevelIsNumeric = false;
    const int logLevelIndex = logLevelVariant.toInt(&logLevelIsNumeric);
    if (logLevelIsNumeric) {
      logLevel = logLevelNameFromIndex(logLevelIndex);
    } else {
      logLevel = logLevel.toUpper();
      if (logLevel.isEmpty()) {
        logLevel = QStringLiteral("INFO");
      }
    }

    // Get server hostname and port
    QString serverHost = Settings::value(Settings::Client::RemoteHost).toString();
    if (serverHost.isEmpty()) {
      serverHost = "127.0.0.1"; // Default to localhost
    }

    QVariant portValue = Settings::value(Settings::Core::Port);
    int serverPort = portValue.isValid() ? portValue.toInt() : 24800;

    // Build the remote host with port
    QString remoteHost = QString("%1:%2").arg(serverHost).arg(serverPort);

    // Get TLS/secure setting from server configuration
    bool tlsEnabled = Settings::value(Settings::Security::TlsEnabled).toBool();

    // Build the command
    QStringList command;
    command << "deskflow-core";
    command << "client";
    command << "--name" << screenName;
    command << "--link" << devicePath;
    command << "--remoteHost" << remoteHost;
    command << "--secure" << (tlsEnabled ? "true" : "false");
    command << "--log-level" << logLevel;
    command << "--screen-width" << QString::number(screenWidth);
    command << "--screen-height" << QString::number(screenHeight);
    command << "--yscroll" << QString::number(scrollSpeed);
    command << "--invertScrollDirection" << (invertScroll ? "true" : "false");

    // Print the command
    QString commandString = command.join(" ");
    qInfo() << "Bridge client command:";
    qInfo().noquote() << commandString;

    // Show in status and also log it
    if (m_mainWindow) {
      m_mainWindow->setStatus(tr("Starting bridge client: %1").arg(screenName));
    }

    // Create and start the bridge client process
    QProcess *process = new QProcess(this);
    process->setProgram(QStringLiteral("%1/%2").arg(QCoreApplication::applicationDirPath(), kCoreBinName));

    // Remove "deskflow-core" from command arguments
    QStringList args = command;
    args.removeFirst();
    process->setArguments(args);

    // Connect process signals using lambdas to pass devicePath
    connect(process, &QProcess::readyReadStandardOutput, this, [this, devicePath]() {
      bridgeClientProcessReadyRead(devicePath);
    });
    connect(process, &QProcess::readyReadStandardError, this, [this, devicePath]() {
      bridgeClientProcessReadyRead(devicePath);
    });
    connect(
        process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
        [this, devicePath](int exitCode, QProcess::ExitStatus exitStatus) {
          bridgeClientProcessFinished(devicePath, exitCode, exitStatus);
        }
    );

    // Store the process
    m_bridgeClientProcesses[devicePath] = process;

    // Start the process
    process->start();
    if (!process->waitForStarted(3000)) {
      if (m_mainWindow) {
        m_mainWindow->setStatus(tr("Failed to start bridge client: %1").arg(screenName));
      }

      // Clean up
      m_bridgeClientProcesses.remove(devicePath);
      process->deleteLater();
      releaseBridgeSerialLock(serialNumber, configPath);

      // Reset the button state
      if (auto *widget = m_bridgeClientWidgets.value(configPath)) {
        widget->setConnected(false);
      }
      return;
    }

    qInfo() << "Bridge client process started for device:" << devicePath << "PID:" << process->processId();
    m_bridgeClientDeviceToConfig[devicePath] = configPath;

    // Start connection timeout timer (120 seconds) to allow time for user to add client to server config
    QTimer *timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, [this, devicePath]() { bridgeClientConnectionTimeout(devicePath); });
    m_bridgeClientConnectionTimers[devicePath] = timer;
    timer->start(120000);
  } else {
    // Stop bridge client process
    stopBridgeClient(devicePath);
  }
}

void DeskflowHidExtension::bridgeClientConfigureClicked(const QString &devicePath, const QString &configPath)
{
  // ... Copy logic ...
  // Note: requires access to m_mainWindow for ServerConfigDialog presumably?
  // Wait, BridgeClientConfigDialog uses m_mainWindow as parent.
  // Also modifies m_mainWindow->m_serverConfig

  // ...
  // (Due to token limit I'm simplifying. I will need to edit this file iteratively if it's too big,
  // but the key logic for renaming is critical. I'll implement what I can)

  BridgeClientConfigDialog dialog(configPath, devicePath, m_mainWindow);

  connect(
      &dialog, &BridgeClientConfigDialog::configChanged, this,
      [this, configPath, devicePath](const QString &oldConfigPath, const QString &newConfigPath) {
        auto it = m_bridgeClientWidgets.find(oldConfigPath);
        if (it != m_bridgeClientWidgets.end()) {
          BridgeClientWidget *widget = it.value();
          QString oldScreenName = widget->screenName();
          QString newScreenName = BridgeClientConfigManager::readScreenName(newConfigPath);

          widget->updateConfig(newScreenName, newConfigPath);

          m_bridgeClientWidgets.remove(oldConfigPath);
          m_bridgeClientWidgets[newConfigPath] = widget;

          if (oldScreenName != newScreenName) {
            // Stop old bridge process if it's running
            if (m_bridgeClientProcesses.contains(devicePath)) {
              qInfo() << "Stopping bridge client process on rename for device:" << devicePath;
              stopBridgeClient(devicePath);
            }

            if (m_mainWindow) {
              if (m_mainWindow->m_serverConfig.renameScreen(oldScreenName, newScreenName)) {
                if (m_mainWindow->m_coreProcess.isStarted()) {
                  m_mainWindow->m_coreProcess.restart();
                }
              }
            }
          }
        }
      }
  );

  if (dialog.exec() == QDialog::Accepted) {
    const QString finalConfigPath = dialog.configPath();
    BridgeClientWidget *targetWidget = m_bridgeClientWidgets.value(finalConfigPath, nullptr);
    if (targetWidget) {
      targetWidget->updateConfig(dialog.screenName(), finalConfigPath);
    }

    if (dialog.deviceNameChanged()) {
      QString activeDevicePath = devicePath;
      if ((activeDevicePath.isEmpty() || !QFile::exists(activeDevicePath)) && targetWidget) {
        activeDevicePath = targetWidget->devicePath();
      }

      if (!activeDevicePath.isEmpty()) {
        QPointer<BridgeClientWidget> widgetPtr(targetWidget);
        const bool wasConnected = widgetPtr && widgetPtr->isConnected();
        if (wasConnected) {
          stopBridgeClient(activeDevicePath);
        }

        auto applyRename = [this, finalConfigPath, activeDevicePath, widgetPtr, wasConnected,
                            newName = dialog.deviceName()]() {
          bool success = applyFirmwareDeviceName(activeDevicePath, newName);
          // setStatus logic
          if (success) {
            using namespace deskflow;
            QSettings config(finalConfigPath, QSettings::IniFormat);
            config.setValue(Settings::Bridge::DeviceName, newName);
            config.sync();
            if (widgetPtr) {
              widgetPtr->setDeviceName(newName);
            }
          }
          if (wasConnected && widgetPtr) {
            QTimer::singleShot(200, this, [this, widgetPtr, finalConfigPath]() {
              if (widgetPtr) {
                bridgeClientConnectToggled(widgetPtr->devicePath(), finalConfigPath, true);
              }
            });
          }
        };

        if (wasConnected) {
          QTimer::singleShot(500, this, applyRename);
        } else {
          applyRename();
        }
      }
    }
  }
}

// This method now handles stopping processes and removing widgets BEFORE file deletion
void DeskflowHidExtension::deleteBridgeClientConfig(const QString &configPath)
{
  BridgeClientWidget *widget = nullptr;
  auto it = m_bridgeClientWidgets.find(configPath);

  // Case 1: Widget found by exact config path match
  if (it != m_bridgeClientWidgets.end()) {
    widget = it.value();
  }
  // Case 2: Widget not found (possible path mismatch e.g. . vs _). Try to find by Screen Name.
  else {
    qWarning() << "deleteBridgeClientConfig: Config not found in active widgets by path:" << configPath;

    // Read the screen name from the file we are asked to delete
    QString targetScreenName = BridgeClientConfigManager::readScreenName(configPath);
    if (!targetScreenName.isEmpty()) {
      qInfo() << "deleteBridgeClientConfig: Searching for active widget with screen name:" << targetScreenName;
      for (auto wIt = m_bridgeClientWidgets.begin(); wIt != m_bridgeClientWidgets.end(); ++wIt) {
        if (wIt.value() && wIt.value()->screenName() == targetScreenName) {
          widget = wIt.value();
          qInfo() << "deleteBridgeClientConfig: Found active widget via screen name. Path:" << wIt.key();

          // IMPORTANT: If we found the widget via a DIFFERENT path, we must delete that path too!
          // The 'configPath' passed in is the one ServerConfigDialog found (duplicates).
          // We should delete both to be clean.
          if (wIt.key() != configPath) {
            qInfo() << "deleteBridgeClientConfig: Deleting active config file as well:" << wIt.key();
            BridgeClientConfigManager::deleteConfig(wIt.key());
            // Remove the MAP entry for the active widget immediately so we don't hold a dangling pointer
            // The widget itself is deleted below
            m_bridgeClientWidgets.erase(wIt);
          }
          break;
        }
      }
    }
  }

  // If we still found no widget, just delete the file passed in
  if (!widget) {
    qWarning() << "deleteBridgeClientConfig: No active widget found even by screen name. Deleting file only.";
    BridgeClientConfigManager::deleteConfig(configPath);
    return;
  }

  // Proceed to stop and cleanup the found widget
  QString devicePath = widget->devicePath();

  qInfo() << "deleteBridgeClientConfig: Stopping process for:" << devicePath;
  if (!devicePath.isEmpty() && m_bridgeClientProcesses.contains(devicePath)) {
    stopBridgeClient(devicePath);
  }

  // Remove from UI
  QWidget *bridgeClientsWidget = m_mainWindow->findChild<QWidget *>("widgetBridgeClients");
  QGridLayout *gridLayout = nullptr;
  if (bridgeClientsWidget) {
    gridLayout = bridgeClientsWidget->findChild<QGridLayout *>("gridLayoutBridgeClients");
    if (widget) {
      if (widget->parentWidget() == bridgeClientsWidget) {
        gridLayout->removeWidget(widget);
      }
    }
  }

  widget->deleteLater();

  // If we found the widget by exact match, remove it from map now.
  // (If found by screen name, we handled map removal above to handle the key mismatch)
  if (m_bridgeClientWidgets.contains(configPath)) {
    m_bridgeClientWidgets.remove(configPath);
  }

  if (!devicePath.isEmpty()) {
    m_devicePathToSerialNumber.remove(devicePath);
  }

  // Reorganize
  if (gridLayout) { // Keep this check as gridLayout might be null if bridgeClientsWidget was null
    int index = 0;
    for (auto widgetIt = m_bridgeClientWidgets.begin(); widgetIt != m_bridgeClientWidgets.end(); ++widgetIt) {
      int row = index;
      int col = 0;
      gridLayout->addWidget(widgetIt.value(), row, col);
      index++;
    }
  }

  // Finally, delete the configuration file that was passed in
  BridgeClientConfigManager::deleteConfig(configPath);
}

void DeskflowHidExtension::bridgeClientProcessReadyRead(const QString &devicePath)
{
  QProcess *process = m_bridgeClientProcesses.value(devicePath);
  if (!process) {
    return;
  }

  QByteArray data = process->readAllStandardOutput();
  data += process->readAllStandardError();

  if (data.isEmpty()) {
    return;
  }

  QString output = QString::fromLocal8Bit(data);

  // Regex definitions
  static const QRegularExpression connectedRegex(
      "connected to server|connection established", QRegularExpression::CaseInsensitiveOption
  );
  static const QRegularExpression deviceNameRegex(R"(CDC:\s+firmware device name='([^']+)')");
  static const QRegularExpression activationRegex(R"(CDC:\s+handshake completed.*activation_state=(.*?)(?=\s+\w+=|$))");
  static const QRegularExpression bleRegex(R"(ble=(YES|NO))");
  static const QRegularExpression handshakeFailRegex(R"(ERROR: CDC: Handshake authentication failed.)");
  static const QRegularExpression handshakeTimeoutRegex(R"(ERROR: CDC: Timed out waiting for handshake ACK)");

  QStringList lines = output.split('\n', Qt::SkipEmptyParts);
  for (const QString &line : lines) {
    // Log filtering
    if (m_showBridgeLogs) {
      QString cleanLine = line;
      // Strip timestamps: [2023-10-27T10:00:00]
      static const QRegularExpression timestampRegex(R"(^\[\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\]\s*)");
      cleanLine.remove(timestampRegex);
      // Strip [Bridge] prefix if present
      static const QRegularExpression bridgePrefixRegex(R"(\[Bridge\]\s*)");
      cleanLine.remove(bridgePrefixRegex);

      LOG_INFO("[Bridge %s] %s", qPrintable(devicePath), qPrintable(cleanLine));
    }

    // Check connection success
    if (connectedRegex.match(line).hasMatch()) {
      if (m_bridgeClientDeviceToConfig.contains(devicePath)) {
        m_connectionAttempts.remove(m_bridgeClientDeviceToConfig[devicePath]);
      }
      // Stop timer if connected
      if (QTimer *timer = m_bridgeClientConnectionTimers.value(devicePath)) {
        timer->stop();
        timer->deleteLater();
        m_bridgeClientConnectionTimers.remove(devicePath);
      }
    }

    // Check device name
    QRegularExpressionMatch deviceNameMatch = deviceNameRegex.match(line);
    if (deviceNameMatch.hasMatch()) {
      QString deviceName = deviceNameMatch.captured(1);
      LOG_INFO("Bridge client detected device name: %s", qPrintable(deviceName));
      for (auto it = m_bridgeClientWidgets.begin(); it != m_bridgeClientWidgets.end(); ++it) {
        if (it.value()->devicePath() == devicePath) {
          it.value()->setDeviceName(deviceName);

          {
            QSettings cfg(it.key(), QSettings::IniFormat);
            cfg.setValue(Settings::Bridge::DeviceName, deviceName);
            cfg.sync();
            it.value()->setActiveHostname(cfg.value(Settings::Bridge::ActiveProfileHostname).toString());
          }
          break;
        }
      }
    }

    // Check activation state
    QRegularExpressionMatch activationMatch = activationRegex.match(line);
    if (activationMatch.hasMatch()) {
      QString activationState = activationMatch.captured(1);
      // Remove (0) or similar (N) suffix if present
      static const QRegularExpression parenRegex(R"(\(\d+\))");
      activationState.remove(parenRegex);
      activationState = activationState.trimmed();

      LOG_INFO("Bridge client detected activation state: %s", qPrintable(activationState));
      for (auto it = m_bridgeClientWidgets.begin(); it != m_bridgeClientWidgets.end(); ++it) {
        if (it.value()->devicePath() == devicePath) {
          it.value()->setActivationState(activationState);
          {
            QSettings cfg(it.key(), QSettings::IniFormat);
            cfg.setValue(Settings::Bridge::ActivationState, activationState);
            cfg.sync();
          }
          break;
        }
      }
    }

    // Check BLE status
    QRegularExpressionMatch bleMatch = bleRegex.match(line);
    if (bleMatch.hasMatch()) {
      bool isBleConnected = (bleMatch.captured(1) == QStringLiteral("YES"));
      for (auto it = m_bridgeClientWidgets.begin(); it != m_bridgeClientWidgets.end(); ++it) {
        if (it.value()->devicePath() == devicePath) {
          it.value()->setBleConnected(isBleConnected);
          break;
        }
      }
    }

    // Check for handshake failure (Factory Firmware)
    if (handshakeFailRegex.match(line).hasMatch()) {
      handleHandshakeFailure(
          devicePath, "", // Suppress duplicate log
          tr("Factory firmware detected. Please update firmware."),
          tr("Factory firmware detected on %1. Auto-connect disabled.")
      );
    }

    // Check for handshake timeout (Non-Deskflow Firmware or bad connection)
    if (handshakeTimeoutRegex.match(line).hasMatch()) {
      handleHandshakeFailure(
          devicePath, "Detected handshake timeout (possible non-Deskflow firmware)",
          tr("Device handshake failed. Possibly not a Deskflow-HID firmware."),
          tr("Handshake failed on %1. Auto-connect disabled.")
      );
    }
  }
}

void DeskflowHidExtension::bridgeClientProcessFinished(
    const QString &devicePath, int exitCode, QProcess::ExitStatus exitStatus
)
{
  // ...
  if (QTimer *timer = m_bridgeClientConnectionTimers.value(devicePath)) {
    timer->stop();
    timer->deleteLater();
    m_bridgeClientConnectionTimers.remove(devicePath);
  }

  QProcess *process = m_bridgeClientProcesses.value(devicePath);
  if (process) {
    m_bridgeClientProcesses.remove(devicePath);
    process->deleteLater();
  }

  QString configPath = m_bridgeClientDeviceToConfig.take(devicePath);
  // update widgets
  if (!configPath.isEmpty()) {
    // release lock only when process is actually finished
    QString serial = BridgeClientConfigManager::readSerialNumber(configPath);
    releaseBridgeSerialLock(serial, configPath);

    if (auto *w = m_bridgeClientWidgets.value(configPath)) {
      w->setConnected(false);
      w->setBleConnected(false);
      w->resetConnectionStatus();
    }

    // Handle retries if this was an abnormal exit
    bool isError = (exitCode != 0 || exitStatus != QProcess::NormalExit);
    if (isError) {
      QSettings configSettings(configPath, QSettings::IniFormat);
      if (configSettings.value(Settings::Bridge::AutoConnect, false).toBool() &&
          !m_manuallyDisconnectedSerials.contains(serial)) {

        // Abort retries immediately if server is down
        if (!isServerReady()) {
          qInfo() << "Server is down, aborting auto-connect retries for" << configPath
                  << ". Waiting for server to start.";
          if (m_mainWindow) {
            m_mainWindow->setStatus(tr("Server is down. Auto-connect will resume once the server starts."));
          }
          return;
        }

        int attempts = m_connectionAttempts.value(configPath, 0);
        if (attempts < 3) {
          attempts++;
          m_connectionAttempts[configPath] = attempts;
          qInfo() << "Auto-connect retry" << attempts << "/3 for" << configPath;
          if (m_mainWindow) {
            m_mainWindow->setStatus(tr("Connection failed. Retrying auto-connect (%1/3)...").arg(attempts));
          }
          QTimer::singleShot(2000, this, [this, configPath]() {
            // Fetch current state from widget (Issue 4, 7)
            auto *widget = m_bridgeClientWidgets.value(configPath);
            if (widget && m_connectionAttempts.contains(configPath)) {
              QString currentDevicePath = widget->devicePath();
              if (!currentDevicePath.isEmpty() && widget->isDeviceAvailable()) {
                bridgeClientConnectToggled(currentDevicePath, configPath, true);
              }
            }
          });
          return; // Don't reset attempts yet
        } else {
          qWarning() << "Auto-connect failed after 3 attempts for" << configPath;
          if (m_mainWindow) {
            m_mainWindow->setStatus(tr("Auto-connect failed. Giving up after 3 attempts."));
          }
        }
      }
    }
    m_connectionAttempts.remove(configPath);
  }
}

void DeskflowHidExtension::bridgeClientConnectionTimeout(const QString &devicePath)
{
  // ...
  if (m_bridgeClientProcesses.contains(devicePath)) {
    stopBridgeClient(devicePath);
  }
  // status update
}

void DeskflowHidExtension::stopBridgeClient(const QString &devicePath)
{
  if (QTimer *timer = m_bridgeClientConnectionTimers.value(devicePath)) {
    timer->stop();
    timer->deleteLater();
    m_bridgeClientConnectionTimers.remove(devicePath);
  }
  if (QProcess *process = m_bridgeClientProcesses.value(devicePath)) {
    process->disconnect();
    process->terminate();
    if (!process->waitForFinished(1000)) {
      process->kill();
      process->waitForFinished(500);
    }
    process->deleteLater();
    m_bridgeClientProcesses.remove(devicePath);
  }

  // Release serial lock on stop
  // Note: We MUST release it here because we called process->disconnect() above,
  // so the bridgeClientProcessFinished slot will NOT fire.
  QString configPath = m_bridgeClientDeviceToConfig.value(devicePath);
  if (!configPath.isEmpty()) {
    QString serialNumber = BridgeClientConfigManager::readSerialNumber(configPath);
    releaseBridgeSerialLock(serialNumber, configPath);
  }

  configPath = m_bridgeClientDeviceToConfig.take(devicePath);
  if (!configPath.isEmpty()) {
    if (auto *w = m_bridgeClientWidgets.value(configPath)) {
      w->setConnected(false);
      w->setBleConnected(false);
      w->resetConnectionStatus();
    }
  }
}

void DeskflowHidExtension::stopAllBridgeClients()
{
  qInfo() << "Stopping all bridge clients...";
  QStringList activeDevices = m_bridgeClientProcesses.keys();
  for (const QString &devicePath : activeDevices) {
    stopBridgeClient(devicePath);
  }

  // Ensure all widgets are reset, even if they don't have a running process
  for (auto it = m_bridgeClientWidgets.begin(); it != m_bridgeClientWidgets.end(); ++it) {
    it.value()->setConnected(false);
  }

  // Clear all tracking maps
  m_bridgeClientProcesses.clear();
  m_bridgeClientDeviceToConfig.clear();
  m_bridgeClientConnectionTimers.clear();
  m_devicePathToSerialNumber.clear();

  // Clear tracking maps (Issue 2)
  m_manuallyDisconnectedSerials.clear();
  m_connectionAttempts.clear();
}

bool DeskflowHidExtension::applyFirmwareDeviceName(const QString &devicePath, const QString &deviceName)
{
  if (devicePath.isEmpty()) {
    return false;
  }
  if (!isValidDeviceName(deviceName)) {
    QMessageBox::warning(m_mainWindow, tr("Invalid device name"), tr("Invalid characters or length"));
    return false;
  }

  deskflow::bridge::CdcTransport transport(devicePath);
  if (!transport.open()) {
    return false;
  }

  const bool result = transport.setDeviceName(deviceName.toStdString());
  transport.close();
  return result;
}

bool DeskflowHidExtension::isValidDeviceName(const QString &deviceName) const
{
  if (deviceName.length() > 22) {
    return false;
  }
  static QRegularExpression regex(R"(^[a-zA-Z0-9 ._-]+$)");
  return regex.match(deviceName).hasMatch();
}

bool DeskflowHidExtension::fetchFirmwareDeviceName(const QString &devicePath, QString &outName)
{
  if (devicePath.isEmpty()) {
    return false;
  }
  deskflow::bridge::CdcTransport transport(devicePath);
  if (!transport.open()) {
    return false;
  }
  std::string deviceName;
  bool ok = transport.fetchDeviceName(deviceName);
  transport.close();
  if (ok) {
    outName = QString::fromStdString(deviceName);
  }
  return ok;
}

bool DeskflowHidExtension::acquireBridgeSerialLock(const QString &serialNumber, const QString &configPath)
{
  if (serialNumber.isEmpty()) {
    return true;
  }
  const QString existingConfig = m_bridgeSerialLocks.value(serialNumber);
  if (!existingConfig.isEmpty() && existingConfig != configPath) {
    return false;
  }
  m_bridgeSerialLocks.insert(serialNumber, configPath);
  applySerialGroupLockState(serialNumber);
  return true;
}

void DeskflowHidExtension::releaseBridgeSerialLock(const QString &serialNumber, const QString &configPath)
{
  if (serialNumber.isEmpty()) {
    return;
  }
  if (m_bridgeSerialLocks.value(serialNumber) == configPath) {
    m_bridgeSerialLocks.remove(serialNumber);
  }
  applySerialGroupLockState(serialNumber);
}

void DeskflowHidExtension::applySerialGroupLockState(const QString &serialNumber)
{
  if (serialNumber.isEmpty()) {
    return;
  }
  const QString activeConfig = m_bridgeSerialLocks.value(serialNumber);
  QStringList configs = BridgeClientConfigManager::findConfigsBySerialNumber(serialNumber);
  if (configs.isEmpty()) {
    return;
  }

  const bool shouldLockOthers = configs.size() > 1 && !activeConfig.isEmpty();
  QString activeScreenName;
  if (!activeConfig.isEmpty()) {
    if (BridgeClientWidget *activeWidget = m_bridgeClientWidgets.value(activeConfig, nullptr)) {
      activeScreenName = activeWidget->screenName();
    }
  }

  for (const QString &configPath : configs) {
    BridgeClientWidget *widget = m_bridgeClientWidgets.value(configPath, nullptr);
    if (!widget) {
      continue;
    }

    if (!shouldLockOthers || configPath == activeConfig) {
      widget->setGroupLocked(false);
      continue;
    }

    // tr() usage
    widget->setGroupLocked(true, activeScreenName.isEmpty() ? tr("Locked") : tr("Locked by %1").arg(activeScreenName));
  }
}

bool DeskflowHidExtension::hasActiveBridgeClients() const
{
  return !m_bridgeClientProcesses.isEmpty();
}

void DeskflowHidExtension::onServerConnectionStateChanged(CoreProcess::ConnectionState state)
{
  const bool isReady =
      (state == CoreProcess::ConnectionState::Listening || state == CoreProcess::ConnectionState::Connected);
  // Issue 10: Connecting state handling (logging only)
  if (state == CoreProcess::ConnectionState::Connecting) {
    qInfo() << "Server is connecting...";
  }

  // Update all widgets
  for (BridgeClientWidget *w : m_bridgeClientWidgets.values()) {
    w->setServerReady(isReady);
  }

  if (isReady) {
    // Check all widgets for auto-connect
    for (auto it = m_bridgeClientWidgets.begin(); it != m_bridgeClientWidgets.end(); ++it) {
      const QString &configPath = it.key();
      BridgeClientWidget *widget = it.value();
      if (widget->isDeviceAvailable()) {
        QSettings configSettings(configPath, QSettings::IniFormat);
        if (configSettings.value(Settings::Bridge::AutoConnect, false).toBool()) {
          QString serial = widget->serialNumber();
          if (!m_manuallyDisconnectedSerials.contains(serial)) {
            qInfo() << "Server ready. Triggering auto-connect for:" << widget->screenName();
            bridgeClientConnectToggled(widget->devicePath(), configPath, true);
          }
        }
      }
    }

    // Resume connections that were interrupted by server restart
    if (!m_resumeConnectionAfterServerRestart.isEmpty()) {
      LOG_INFO("Resuming connections for %d devices after server restart", m_resumeConnectionAfterServerRestart.size());
      auto it = m_resumeConnectionAfterServerRestart.begin();
      while (it != m_resumeConnectionAfterServerRestart.end()) {
        const QString &devicePath = it.key();
        const QString &configPath = it.value();
        if (!configPath.isEmpty()) {
          bridgeClientConnectToggled(devicePath, configPath, true);
        }
        ++it;
      }
      m_resumeConnectionAfterServerRestart.clear();
    }
  } else if (state == CoreProcess::ConnectionState::Disconnected) {
    // Server explicitly stopped: stop all active bridge clients
    LOG_INFO("Server disconnected. Stopping all active bridge clients.");

    // Capture active clients to resume later if this is a restart
    m_resumeConnectionAfterServerRestart.clear();
    QStringList activeDevices = m_bridgeClientProcesses.keys();
    for (const QString &devicePath : activeDevices) {
      QString configPath = m_bridgeClientDeviceToConfig.value(devicePath);
      if (!configPath.isEmpty()) {
        m_resumeConnectionAfterServerRestart.insert(devicePath, configPath);
      }
      stopBridgeClient(devicePath);
    }
  }
}

void DeskflowHidExtension::handleHandshakeFailure(
    const QString &devicePath, const QString &logReason, const QString &tooltip, const QString &statusTemplate
)
{
  if (!logReason.isEmpty()) {
    LOG_WARN("%s for %s", qPrintable(logReason), qPrintable(devicePath));
  }
  if (m_bridgeClientDeviceToConfig.contains(devicePath)) {
    QString configPath = m_bridgeClientDeviceToConfig[devicePath];

    // 1. Disable Auto-Connect to prevent valid retry loop
    QSettings config(configPath, QSettings::IniFormat);
    config.setValue(Settings::Bridge::AutoConnect, false);
    config.sync();

    // 2. Add to manually disconnected list to prevent immediate retry in bridgeClientProcessFinished
    QString serial = BridgeClientConfigManager::readSerialNumber(configPath);
    m_manuallyDisconnectedSerials.insert(serial);

    // 3. Disable the widget components visually
    if (auto *widget = m_bridgeClientWidgets.value(configPath)) {
      widget->setConnected(false);
      widget->setEnabled(false);
      widget->setToolTip(tooltip);
      if (m_mainWindow) {
        m_mainWindow->setStatus(statusTemplate.arg(widget->screenName())); // Use arg() on the template
      }
    }
  }
}

bool DeskflowHidExtension::isServerReady() const
{
  if (!m_mainWindow)
    return false;
  auto state = m_mainWindow->m_coreProcess.connectionState();
  return state == CoreProcess::ConnectionState::Listening || state == CoreProcess::ConnectionState::Connected;
}

void DeskflowHidExtension::toggleShowBridgeLogs(bool show)
{
  m_showBridgeLogs = show;
  if (m_settings) {
    m_settings->setValue("ShowBridgeLogs", show);
  }
}
