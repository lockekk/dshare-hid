/*
 * Deskflow-hid -- created by locke.huang@gmail.com
 */

#pragma once

#include "UsbDeviceMonitor.h"

#include <IOKit/IOKitLib.h>
#include <QMap>
#include <QSocketNotifier>
#include <vector>

namespace deskflow::gui {

/**
 * @brief macOS implementation of USB device monitoring using IOKit
 */
class MacUsbMonitor : public UsbDeviceMonitor
{
  Q_OBJECT

public:
  explicit MacUsbMonitor(QObject *parent = nullptr);
  ~MacUsbMonitor() override;

  bool startMonitoring() override;
  void stopMonitoring() override;
  bool isMonitoring() const override;
  QList<UsbDeviceInfo> enumerateDevices() override;

private Q_SLOTS:
  // No socket notifier needed for IOKit, it uses RunLoop callbacks
  // But we might need to emit signals on the main thread if callback is on another thread
  // actually IOKit callbacks usually happen on the RunLoop they are scheduled on.
  // We will schedule on CFRunLoopGetMain().

private:
  static void onDeviceAdded(void *refCon, io_iterator_t iterator);
  static void onDeviceRemoved(void *refCon, io_iterator_t iterator);

  void processAddedDevices(io_iterator_t iterator);
  void processRemovedDevices(io_iterator_t iterator);

  UsbDeviceInfo extractDeviceInfo(io_service_t device);

  enum class ConnectResult
  {
    Stop, // Failure or Success (stop processing)
    Retry // Temporary failure (try again)
  };
  ConnectResult tryConnectDevice(io_service_t device, uint64_t entryID);

  IONotificationPortRef m_notifyPort = nullptr;
  io_iterator_t m_addedIter = 0;
  io_iterator_t m_removedIter = 0;
  bool m_monitoring = false;

  // Track connected devices by registry entry ID (or unique ID) to handle removals
  QMap<uint64_t, UsbDeviceInfo> m_connectedDevices;

  struct PendingRetry
  {
    io_service_t device = 0;
    int attempt = 0;
  };
  QMap<uint64_t, PendingRetry> m_pendingRetries;

  void scheduleRetry(io_service_t device, uint64_t entryID, int attempt);
  void executeRetry(uint64_t entryID);
};

} // namespace deskflow::gui
