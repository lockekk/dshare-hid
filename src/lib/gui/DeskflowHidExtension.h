/*
 * Deskflow-hid -- created by locke.huang@gmail.com
 */

#pragma once

#include <QHash>
#include <QMap>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <memory>

#include "gui/devices/UsbDeviceMonitor.h"

class QMainWindow;

namespace deskflow::gui { // forward decls
class UsbDeviceMonitor;
class BridgeClientWidget;
} // namespace deskflow::gui

class MainWindow; // The main window class

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

public Q_SLOTS:
  void bridgeClientDeletedFromServerConfig(const QString &configPath);

private Q_SLOTS:
  void usbDeviceConnected(const deskflow::gui::UsbDeviceInfo &device);
  void usbDeviceDisconnected(const deskflow::gui::UsbDeviceInfo &device);
  void bridgeClientConnectToggled(const QString &devicePath, const QString &configPath, bool shouldConnect);
  void bridgeClientConfigureClicked(const QString &devicePath, const QString &configPath);

  void bridgeClientProcessReadyRead(const QString &devicePath);
  void bridgeClientProcessFinished(const QString &devicePath, int exitCode, QProcess::ExitStatus exitStatus);
  void bridgeClientConnectionTimeout(const QString &devicePath);

private:
  void loadBridgeClientConfigs();
  void updateBridgeClientDeviceStates();
  void stopBridgeClient(const QString &devicePath);
  void stopAllBridgeClients();
  bool applyFirmwareDeviceName(const QString &devicePath, const QString &deviceName);
  bool isValidDeviceName(const QString &deviceName) const;
  bool fetchFirmwareDeviceName(const QString &devicePath, QString &outName);

  bool acquireBridgeSerialLock(const QString &serialNumber, const QString &configPath);
  void releaseBridgeSerialLock(const QString &serialNumber, const QString &configPath);
  void applySerialGroupLockState(const QString &serialNumber);

  MainWindow *m_mainWindow;

  deskflow::gui::UsbDeviceMonitor *m_usbDeviceMonitor = nullptr;

  // Bridge client widgets: config path -> widget
  QMap<QString, deskflow::gui::BridgeClientWidget *> m_bridgeClientWidgets;

  // Track device path -> serial number mapping when devices connect
  // (needed because sysfs disappears when device disconnects)
  QMap<QString, QString> m_devicePathToSerialNumber;

  // Serial number -> config path holding the active connection
  QHash<QString, QString> m_bridgeSerialLocks;

  // Device path -> config path for currently running bridge client process
  QHash<QString, QString> m_bridgeClientDeviceToConfig;

  // Bridge client process management: device path -> QProcess*
  QMap<QString, QProcess *> m_bridgeClientProcesses;

  // Bridge client connection timeout timers: device path -> QTimer*
  QMap<QString, QTimer *> m_bridgeClientConnectionTimers;
};
