/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#pragma once

#include "UsbDeviceMonitor.h"

#include <QMap>
#include <QSocketNotifier>
#include <memory>

// Forward declarations - these are in the global namespace
struct udev;
struct udev_monitor;
struct udev_device;

namespace deskflow::gui {

/**
 * @brief Linux implementation of USB device monitoring using libudev
 *
 * This class uses libudev to monitor USB device plug/unplug events.
 * It integrates with Qt's event loop using QSocketNotifier to watch
 * the udev file descriptor for events.
 *
 * No root permissions are required for monitoring device events.
 */
class LinuxUdevMonitor : public UsbDeviceMonitor
{
  Q_OBJECT

public:
  explicit LinuxUdevMonitor(QObject *parent = nullptr);
  ~LinuxUdevMonitor() override;

  bool startMonitoring() override;
  void stopMonitoring() override;
  bool isMonitoring() const override;
  QList<UsbDeviceInfo> enumerateDevices() override;

private Q_SLOTS:
  /**
   * @brief Handle events from udev monitor
   * Called when the socket notifier detects activity
   */
  void handleUdevEvent();

private:
  /**
   * @brief Extract device information from a udev device
   * @param device udev device handle
   * @return Device information, or empty if device doesn't match criteria
   */
  UsbDeviceInfo extractDeviceInfo(::udev_device *device);

  /**
   * @brief Get parent USB device from a tty device
   * @param device tty device
   * @return parent USB device, or nullptr if not found
   */
  ::udev_device *getUsbDevice(::udev_device *device);

  /**
   * @brief Cleanup resources
   */
  void cleanup();

  ::udev *m_udev = nullptr;
  ::udev_monitor *m_monitor = nullptr;
  std::unique_ptr<QSocketNotifier> m_notifier;
  bool m_monitoring = false;

  // Track connected devices by path so we can handle removal events
  // (vendor ID not available when device is removed)
  QMap<QString, UsbDeviceInfo> m_connectedDevices;
};

} // namespace deskflow::gui
