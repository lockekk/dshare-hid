/*
 * Deskflow-hid -- created by locke.huang@gmail.com
 */

#include "BridgeClientManager.h"
#include <QDebug>
#include <QTimer>

namespace deskflow::gui {

BridgeClientManager::BridgeClientManager(QObject *parent) : QObject(parent)
{
}

BridgeClientManager::~BridgeClientManager()
{
}

void BridgeClientManager::addClientConfig(const QString &configPath)
{
  if (!m_clients.contains(configPath)) {
    BridgeClientState state;
    state.configPath = configPath;
    m_clients.insert(configPath, state);
    Q_EMIT clientUpdated(configPath);
  }
}

void BridgeClientManager::setClientDevice(
    const QString &configPath, const QString &devicePath, const QString &serialNumber
)
{
  if (m_clients.contains(configPath)) {
    BridgeClientState &state = m_clients[configPath];
    state.devicePath = devicePath;

    if (!serialNumber.isEmpty()) {
      // If changing serial, handle locks if needed (though typically this is initial setup)
      if (!state.serialNumber.isEmpty() && state.serialNumber != serialNumber) {
        releaseSerialLock(state.serialNumber, configPath);
      }
      state.serialNumber = serialNumber;
      acquireSerialLock(serialNumber, configPath);
    }

    // Also update reverse lookup
    if (!devicePath.isEmpty()) {
      m_deviceToConfig[devicePath] = configPath;
    }

    Q_EMIT clientUpdated(configPath);
  }
}

void BridgeClientManager::removeClientConfig(const QString &configPath)
{
  if (m_clients.contains(configPath)) {
    BridgeClientState &state = m_clients[configPath];
    if (state.process) {
      state.process->stop();
      state.process->deleteLater();
      state.process = nullptr;
    }
    if (state.connectionTimer) {
      state.connectionTimer->stop();
      state.connectionTimer->deleteLater();
    }

    if (!state.serialNumber.isEmpty()) {
      releaseSerialLock(state.serialNumber, configPath);
    }

    m_deviceToConfig.remove(state.devicePath);
    m_clients.remove(configPath);
  }
}

void BridgeClientManager::setDeviceAvailable(const QString &devicePath, const QString &serialNumber, bool available)
{
  // Find all configs that use this serial number or device path
  for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
    BridgeClientState &state = it.value();
    bool match = false;
    if (!serialNumber.isEmpty() && state.serialNumber == serialNumber) {
      match = true;
    } else if (state.devicePath == devicePath) {
      match = true;
    }

    if (match) {
      state.devicePath = devicePath;
      state.serialNumber = serialNumber;
      state.isAvailable = available;

      if (available) {
        m_deviceToConfig[devicePath] = it.key();
      } else {
        m_deviceToConfig.remove(devicePath);
      }

      Q_EMIT clientUpdated(it.key());
    }
  }
}

void BridgeClientManager::setManuallyDisconnected(const QString &serialNumber, bool disconnected)
{
  if (serialNumber.isEmpty())
    return;

  for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
    if (it.value().serialNumber == serialNumber) {
      it.value().isManuallyDisconnected = disconnected;
      if (disconnected) {
        it.value().connectionAttempts = 0;
      }
      Q_EMIT clientUpdated(it.key());
    }
  }
}

bool BridgeClientManager::canAutoConnect(const QString &configPath) const
{
  if (!m_clients.contains(configPath))
    return false;
  const BridgeClientState &state = m_clients[configPath];

  return state.isAvailable && !state.isManuallyDisconnected && !state.process;
}

bool BridgeClientManager::isSerialNumberManuallyDisconnected(const QString &serialNumber) const
{
  if (serialNumber.isEmpty())
    return false;

  for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
    if (it.value().serialNumber == serialNumber && it.value().isManuallyDisconnected) {
      return true;
    }
  }
  return false;
}

void BridgeClientManager::handleConnectionEstablished(const QString &devicePath)
{
  if (m_deviceToConfig.contains(devicePath)) {
    QString configPath = m_deviceToConfig[devicePath];
    BridgeClientState &state = m_clients[configPath];
    state.connectionAttempts = 0;
    if (state.connectionTimer) {
      state.connectionTimer->stop();
      state.connectionTimer->deleteLater();
      state.connectionTimer = nullptr;
    }
    Q_EMIT clientUpdated(configPath);
  }
}

void BridgeClientManager::handleConnectionFinished(const QString &devicePath)
{
  if (m_deviceToConfig.contains(devicePath)) {
    QString configPath = m_deviceToConfig[devicePath];
    BridgeClientState &state = m_clients[configPath];
    state.process = nullptr;

    // Release lock when process finishes
    if (!state.serialNumber.isEmpty()) {
      releaseSerialLock(state.serialNumber, configPath);
    }

    // connectionTimer might still be running if it was a crash during startup
    if (state.connectionTimer) {
      state.connectionTimer->stop();
      state.connectionTimer->deleteLater();
      state.connectionTimer = nullptr;
    }
    Q_EMIT clientUpdated(configPath);
  }
}

void BridgeClientManager::updateProcess(const QString &configPath, BridgeClientProcess *process)
{
  if (m_clients.contains(configPath)) {
    m_clients[configPath].process = process;
    Q_EMIT clientUpdated(configPath);
  }
}

void BridgeClientManager::updateTimer(const QString &configPath, QTimer *timer)
{
  if (m_clients.contains(configPath)) {
    if (m_clients[configPath].connectionTimer) {
      m_clients[configPath].connectionTimer->stop();
      m_clients[configPath].connectionTimer->deleteLater();
    }
    m_clients[configPath].connectionTimer = timer;
  }
}

void BridgeClientManager::incrementAttempts(const QString &configPath)
{
  if (m_clients.contains(configPath)) {
    m_clients[configPath].connectionAttempts++;
    Q_EMIT clientUpdated(configPath);
  }
}

void BridgeClientManager::clearAttempts(const QString &configPath)
{
  if (m_clients.contains(configPath)) {
    m_clients[configPath].connectionAttempts = 0;
    Q_EMIT clientUpdated(configPath);
  }
}

const BridgeClientState *BridgeClientManager::clientState(const QString &configPath) const
{
  auto it = m_clients.find(configPath);
  if (it != m_clients.end()) {
    return &it.value();
  }
  return nullptr;
}

const BridgeClientState *BridgeClientManager::clientStateByDevice(const QString &devicePath) const
{
  if (m_deviceToConfig.contains(devicePath)) {
    return clientState(m_deviceToConfig[devicePath]);
  }
  return nullptr;
}

bool BridgeClientManager::acquireSerialLock(const QString &serial, const QString &configPath)
{
  if (serial.isEmpty())
    return true;

  QString current = m_serialToConfigLock.value(serial);
  if (!current.isEmpty() && current != configPath) {
    return false;
  }

  m_serialToConfigLock.insert(serial, configPath);
  return true;
}

void BridgeClientManager::releaseSerialLock(const QString &serial, const QString &configPath)
{
  if (serial.isEmpty())
    return;
  if (m_serialToConfigLock.value(serial) == configPath) {
    m_serialToConfigLock.remove(serial);
  }
}

QString BridgeClientManager::configHoldingLock(const QString &serial) const
{
  return m_serialToConfigLock.value(serial);
}

} // namespace deskflow::gui
