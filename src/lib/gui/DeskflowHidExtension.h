/*
 * Deskflow-hid -- created by locke.huang@gmail.com
 */

#pragma once

#include <QHash>
#include <QMap>
#include <QObject>
#include <QProcess>
#include <QSet>
#include <QTimer>
#include <memory>

#include "gui/core/CoreProcess.h"
#include "gui/devices/UsbDeviceMonitor.h"

class QMainWindow;

namespace deskflow::gui { // forward decls
class UsbDeviceMonitor;
class BridgeClientWidget;
} // namespace deskflow::gui

#include "gui/core/BridgeClientManager.h"
#include "gui/core/BridgeDeviceService.h"

class MainWindow; // The main window class
class QAction;
class QSettings;

class DeskflowHidExtension : public QObject
{
  Q_OBJECT

public:
  explicit DeskflowHidExtension(MainWindow *parent);
  ~DeskflowHidExtension() override;

  void setup();
  void shutdown();
  void openEsp32HidTools();
  bool hasActiveBridgeClients() const;

  void pauseUsbMonitoring();
  void resumeUsbMonitoring();

public Q_SLOTS:
  void deleteBridgeClientConfig(const QString &configPath);

private Q_SLOTS:
  void usbDeviceConnected(const deskflow::gui::UsbDeviceInfo &device);
  void usbDeviceDisconnected(const deskflow::gui::UsbDeviceInfo &device);
  void bridgeClientConnectToggled(const QString &devicePath, const QString &configPath, bool shouldConnect);
  void bridgeClientConfigureClicked(const QString &devicePath, const QString &configPath);

  // Slots for BridgeClientProcess signals
  void onBridgeProcessLogAvailable(const QString &devicePath, const QString &line);
  void onBridgeProcessFinished(const QString &devicePath, int exitCode, QProcess::ExitStatus exitStatus);
  void onBridgeProcessConnectionEstablished(const QString &devicePath);
  void onBridgeProcessDeviceNameDetected(const QString &devicePath, const QString &name);
  void onBridgeProcessActivationStatusDetected(const QString &devicePath, const QString &state, int profileIndex);
  void onBridgeProcessBleStatusDetected(const QString &devicePath, bool connected);
  void onBridgeProcessHandshakeFailed(const QString &devicePath, const QString &reason);

  void onBridgeDeviceConfigSynced(const QString &configPath, int activeProfile);

  void bridgeClientConnectionTimeout(const QString &devicePath);
  void onServerConnectionStateChanged(deskflow::gui::CoreProcess::ConnectionState state);
  void toggleShowBridgeLogs(bool show);
  void handleSessionStateChanged(bool locked);

private:
  void processConnectionRequests();
  void loadBridgeClientConfigs();
  void updateBridgeClientDeviceStates();
  void applyProfileScreenBonding(const QString &configPath, int activeProfile);
  void stopBridgeClient(const QString &devicePath);
  void stopAllBridgeClients();
  void bridgeClientProcessFinished(const QString &devicePath, int exitCode, QProcess::ExitStatus exitStatus);
  bool isServerReady() const;

  void handleHandshakeFailure(
      const QString &devicePath, const QString &logReason, const QString &tooltip, const QString &statusTemplate
  );

  void applySerialGroupLockState(const QString &serialNumber);

  MainWindow *m_mainWindow;

  deskflow::gui::UsbDeviceMonitor *m_usbDeviceMonitor = nullptr;
  deskflow::gui::BridgeClientManager *m_bridgeClientManager = nullptr;
  deskflow::gui::BridgeDeviceService *m_bridgeDeviceService = nullptr;

  // Bridge client widgets: config path -> widget
  QMap<QString, deskflow::gui::BridgeClientWidget *> m_bridgeClientWidgets;

  // Track config paths that should be reconnected after server restart
  // Maps devicePath -> configPath
  QMap<QString, QString> m_resumeConnectionAfterServerRestart;

  // Timer to debounce/delay device scanning when serials are missing
  QTimer *m_retryScanTimer = nullptr;

  bool m_isSessionLocked = false;

  // Log toggle feature
  QAction *m_actionShowBridgeLogs = nullptr;
  bool m_showBridgeLogs = false;
};
