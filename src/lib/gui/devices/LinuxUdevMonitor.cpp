// SPDX-FileCopyrightText: 2024 Deskflow Developers
// SPDX-License-Identifier: MIT

#include <memory>
#include <string>
#include <vector>

#include "LinuxUdevMonitor.h"

#include <libudev.h>

#ifdef size
#undef size
#endif

#include <QDebug>
#include <QSocketNotifier>

namespace deskflow::gui {

LinuxUdevMonitor::LinuxUdevMonitor(QObject *parent) : UsbDeviceMonitor(parent)
{
  // Initialize udev context
  m_udev = udev_new();
  if (!m_udev) {
    qWarning() << "Failed to create udev context";
  }
}

LinuxUdevMonitor::~LinuxUdevMonitor()
{
  cleanup();
}

bool LinuxUdevMonitor::startMonitoring()
{
  if (m_monitoring) {
    qDebug() << "USB device monitoring already started";
    return true;
  }

  if (!m_udev) {
    Q_EMIT errorOccurred("Udev context not initialized");
    return false;
  }

  // Create udev monitor for kernel events
  m_monitor = udev_monitor_new_from_netlink(m_udev, "udev");
  if (!m_monitor) {
    Q_EMIT errorOccurred("Failed to create udev monitor");
    return false;
  }

  // Filter for tty subsystem (USB CDC devices appear here)
  if (udev_monitor_filter_add_match_subsystem_devtype(m_monitor, "tty", nullptr) < 0) {
    qWarning() << "Failed to add tty subsystem filter";
    cleanup();
    return false;
  }

  // Enable receiving events
  if (udev_monitor_enable_receiving(m_monitor) < 0) {
    Q_EMIT errorOccurred("Failed to enable udev monitor");
    cleanup();
    return false;
  }

  // Get the file descriptor for the monitor
  int fd = udev_monitor_get_fd(m_monitor);
  if (fd < 0) {
    Q_EMIT errorOccurred("Failed to get udev monitor file descriptor");
    cleanup();
    return false;
  }

  // Create socket notifier to integrate with Qt event loop
  m_notifier = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Read, this);
  connect(m_notifier.get(), &QSocketNotifier::activated, this, &LinuxUdevMonitor::handleUdevEvent);

  m_monitoring = true;
  qDebug() << "USB device monitoring started";
  return true;
}

void LinuxUdevMonitor::stopMonitoring()
{
  if (!m_monitoring) {
    return;
  }

  cleanup();
  m_monitoring = false;
  qDebug() << "USB device monitoring stopped";
}

bool LinuxUdevMonitor::isMonitoring() const
{
  return m_monitoring;
}

void LinuxUdevMonitor::handleUdevEvent()
{
  qDebug() << "[UdevMonitor] handleUdevEvent() called";

  if (!m_monitor) {
    qWarning() << "[UdevMonitor] No monitor available";
    return;
  }

  // Receive device event (non-blocking)
  struct udev_device *device = udev_monitor_receive_device(m_monitor);
  if (!device) {
    qDebug() << "[UdevMonitor] No device received from monitor";
    return;
  }

  // Get the action (add, remove, change, etc.)
  const char *action = udev_device_get_action(device);
  if (!action) {
    qDebug() << "[UdevMonitor] No action in event";
    udev_device_unref(device);
    return;
  }

  qDebug() << "[UdevMonitor] Event action:" << action;

  // Extract device information
  UsbDeviceInfo info = extractDeviceInfo(device);

  QString actionStr = QString::fromUtf8(action);

  qDebug() << "[UdevMonitor] Extracted info - path:" << info.devicePath << "vendor:" << info.vendorId
           << "product:" << info.productId;

  // Handle add event
  if (actionStr == "add") {
    qDebug() << "[UdevMonitor] Filter match:" << matchesFilter(info) << "(filter vendor:" << vendorIdFilter() << ")";

    // Only process if device has valid info and matches filters
    if (!info.devicePath.isEmpty() && matchesFilter(info)) {
      qDebug() << "USB device event: add"
               << "device:" << info.devicePath << "vendor:" << info.vendorId << "product:" << info.productId;

      // Track this device for removal events
      m_connectedDevices[info.devicePath] = info;

      Q_EMIT deviceConnected(info);
    } else {
      qDebug() << "[UdevMonitor] Device filtered out or invalid";
    }
  }
  // Handle remove event
  else if (actionStr == "remove") {
    // For remove events, we can't get vendor ID from the device anymore
    // Check if we previously tracked this device path
    if (m_connectedDevices.contains(info.devicePath)) {
      UsbDeviceInfo trackedInfo = m_connectedDevices[info.devicePath];

      qDebug() << "USB device event: remove"
               << "device:" << trackedInfo.devicePath << "vendor:" << trackedInfo.vendorId
               << "product:" << trackedInfo.productId;

      m_connectedDevices.remove(info.devicePath);
      Q_EMIT deviceDisconnected(trackedInfo);
    } else {
      qDebug() << "[UdevMonitor] Remove event for untracked device:" << info.devicePath;
    }
  }

  udev_device_unref(device);
}

UsbDeviceInfo LinuxUdevMonitor::extractDeviceInfo(struct udev_device *device)
{
  UsbDeviceInfo info;

  if (!device) {
    return info;
  }

  // Get device node path (e.g., /dev/ttyACM0)
  const char *devNode = udev_device_get_devnode(device);
  if (devNode) {
    info.devicePath = QString::fromUtf8(devNode);
  }

  // Get parent USB device to extract vendor/product IDs
  struct udev_device *usbDevice = getUsbDevice(device);
  if (usbDevice) {
    const char *vendorId = udev_device_get_sysattr_value(usbDevice, "idVendor");
    const char *productId = udev_device_get_sysattr_value(usbDevice, "idProduct");
    const char *serial = udev_device_get_sysattr_value(usbDevice, "serial");

    if (vendorId) {
      info.vendorId = QString::fromUtf8(vendorId).toLower();
    }
    if (productId) {
      info.productId = QString::fromUtf8(productId).toLower();
    }
    if (serial) {
      info.serialNumber = QString::fromUtf8(serial);
    }

    // Don't unref parent device - it's owned by the child
  }

  return info;
}

struct udev_device *LinuxUdevMonitor::getUsbDevice(struct udev_device *device)
{
  if (!device) {
    return nullptr;
  }

  // Walk up the device tree to find the USB device
  struct udev_device *parent = udev_device_get_parent_with_subsystem_devtype(device, "usb", "usb_device");

  return parent; // Don't unref - parent is owned by child device
}

QList<UsbDeviceInfo> LinuxUdevMonitor::enumerateDevices()
{
  QList<UsbDeviceInfo> devices;

  if (!m_udev) {
    return devices;
  }

  // Create enumeration object
  struct udev_enumerate *enumerate = udev_enumerate_new(m_udev);
  if (!enumerate) {
    qWarning() << "Failed to create udev enumeration";
    return devices;
  }

  // Add filter for tty subsystem
  udev_enumerate_add_match_subsystem(enumerate, "tty");

  // Scan devices
  udev_enumerate_scan_devices(enumerate);

  // Get list of devices
  struct udev_list_entry *deviceList = udev_enumerate_get_list_entry(enumerate);
  struct udev_list_entry *entry;

  udev_list_entry_foreach(entry, deviceList)
  {
    const char *path = udev_list_entry_get_name(entry);
    struct udev_device *device = udev_device_new_from_syspath(m_udev, path);

    if (device) {
      UsbDeviceInfo info = extractDeviceInfo(device);

      // Only add devices that have valid info and match filters
      if (!info.devicePath.isEmpty() && matchesFilter(info)) {
        devices.append(info);
        // Track this device for removal events
        m_connectedDevices[info.devicePath] = info;
        qDebug() << "Found USB device:" << info.devicePath << "vendor:" << info.vendorId
                 << "product:" << info.productId;
      }

      udev_device_unref(device);
    }
  }

  udev_enumerate_unref(enumerate);
  return devices;
}

void LinuxUdevMonitor::cleanup()
{
  // Disconnect socket notifier
  if (m_notifier) {
    m_notifier->setEnabled(false);
    m_notifier.reset();
  }

  // Clean up udev monitor
  if (m_monitor) {
    udev_monitor_unref(m_monitor);
    m_monitor = nullptr;
  }

  // Clean up udev context
  if (m_udev) {
    udev_unref(m_udev);
    m_udev = nullptr;
  }
}

} // namespace deskflow::gui
