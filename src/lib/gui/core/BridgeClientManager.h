/*
 * Deskflow-hid -- created by locke.huang@gmail.com
 */

#pragma once

#include <QList>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QString>

#include "BridgeClientProcess.h"

class QTimer;

namespace deskflow::gui {

struct BridgeClientState
{
  QString configPath;
  QString devicePath;
  QString serialNumber;
  bool isAvailable = false;
  bool isManuallyDisconnected = false;
  int connectionAttempts = 0;
  int activeProfileIndex = -1;
  BridgeClientProcess *process = nullptr;
  QTimer *connectionTimer = nullptr;
};

class BridgeClientManager : public QObject
{
  Q_OBJECT

public:
  explicit BridgeClientManager(QObject *parent = nullptr);
  ~BridgeClientManager() override;

  void addClientConfig(const QString &configPath);
  void removeClientConfig(const QString &configPath);
  void setClientDevice(const QString &configPath, const QString &devicePath, const QString &serialNumber);
  void setActiveProfile(const QString &configPath, int activeProfile);

  void setDeviceAvailable(const QString &devicePath, const QString &serialNumber, bool available);
  void setManuallyDisconnected(const QString &serialNumber, bool disconnected);

  bool canAutoConnect(const QString &configPath) const;
  bool isSerialNumberManuallyDisconnected(const QString &serialNumber) const;
  void handleConnectionEstablished(const QString &devicePath);
  void handleConnectionFinished(const QString &devicePath);

  void updateProcess(const QString &configPath, BridgeClientProcess *process);
  void updateTimer(const QString &configPath, QTimer *timer);
  void incrementAttempts(const QString &configPath);
  void clearAttempts(const QString &configPath);

  // Getters for DeskflowHidExtension to sync UI
  QList<QString> configPaths() const
  {
    return m_clients.keys();
  }
  const BridgeClientState *clientState(const QString &configPath) const;
  const BridgeClientState *clientStateByDevice(const QString &devicePath) const;

  // Locks
  bool acquireSerialLock(const QString &serial, const QString &configPath);
  void releaseSerialLock(const QString &serial, const QString &configPath);
  QString configHoldingLock(const QString &serial) const;

Q_SIGNALS:
  void clientUpdated(const QString &configPath);

private:
  QMap<QString, BridgeClientState> m_clients;  // configPath -> State
  QMap<QString, QString> m_serialToConfigLock; // serial -> configPath
  QMap<QString, QString> m_deviceToConfig;     // devicePath -> configPath
};

} // namespace deskflow::gui
