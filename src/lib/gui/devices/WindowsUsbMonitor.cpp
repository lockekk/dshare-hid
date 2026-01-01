/*
 * Deskflow-hid -- created by locke.huang@gmail.com
 */

#include "WindowsUsbMonitor.h"
#include "UsbDeviceHelper.h"

#include "base/Log.h"

#define WIN32_LEAN_AND_MEAN
// clang-format off
#include <windows.h>
#include <dbt.h>
#include <devguid.h>
#include <initguid.h>
#include <setupapi.h>
// clang-format on

namespace deskflow::gui {

// Static instance pointer for window procedure callback
WindowsUsbMonitor *WindowsUsbMonitor::s_instance = nullptr;

WindowsUsbMonitor::WindowsUsbMonitor(QObject *parent) : UsbDeviceMonitor(parent)
{
  s_instance = this;
}

WindowsUsbMonitor::~WindowsUsbMonitor()
{
  stopMonitoring();
  s_instance = nullptr;
}

bool WindowsUsbMonitor::startMonitoring()
{
  if (m_monitoring) {
    LOG_WARN("Windows USB monitoring is already active");
    return true;
  }

  LOG_DEBUG("Starting Windows USB device monitoring...");

  // Create a hidden window to receive device change messages
  WNDCLASSW wc = {};
  wc.lpfnWndProc = windowProc;
  wc.hInstance = GetModuleHandle(nullptr);
  wc.lpszClassName = L"DeskflowUsbMonitorClass";

  if (!RegisterClassW(&wc)) {
    DWORD error = GetLastError();
    if (error != ERROR_CLASS_ALREADY_EXISTS) {
      LOG_WARN("Failed to register window class: %lu", error);
      return false;
    }
  }

  m_hwnd = CreateWindowW(
      L"DeskflowUsbMonitorClass", L"Deskflow USB Monitor", 0, 0, 0, 0, 0,
      HWND_MESSAGE, // Message-only window
      nullptr, GetModuleHandle(nullptr), nullptr
  );

  if (!m_hwnd) {
    LOG_WARN("Failed to create message window: %lu", GetLastError());
    return false;
  }

  registerDeviceNotification();

  registerDeviceNotification();

  // Store initial device list without emitting signals, to match previous behavior
  QList<UsbDeviceInfo> currentDevices = enumerateDevices();
  for (const auto &device : currentDevices) {
    m_devices.insert(device.devicePath, device);
  }

  m_monitoring = true;
  LOG_DEBUG("Windows USB device monitoring started successfully");

  return true;
}

void WindowsUsbMonitor::stopMonitoring()
{
  if (!m_monitoring) {
    return;
  }

  LOG_DEBUG("Stopping Windows USB device monitoring...");

  unregisterDeviceNotification();

  if (m_hwnd) {
    DestroyWindow(m_hwnd);
    m_hwnd = nullptr;
  }

  m_monitoring = false;
  m_devices.clear();
  LOG_DEBUG("Windows USB device monitoring stopped");
}

bool WindowsUsbMonitor::isMonitoring() const
{
  return m_monitoring;
}

void WindowsUsbMonitor::registerDeviceNotification()
{
  if (!m_hwnd) {
    return;
  }

  // Register for device interface notifications for all device classes
  DEV_BROADCAST_DEVICEINTERFACE_W notificationFilter = {};
  notificationFilter.dbcc_size = sizeof(notificationFilter);
  notificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;

  m_hDevNotify = RegisterDeviceNotificationW(
      m_hwnd, &notificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE | DEVICE_NOTIFY_ALL_INTERFACE_CLASSES
  );

  if (!m_hDevNotify) {
    LOG_WARN("Failed to register device notification: %lu", GetLastError());
  } else {
    LOG_DEBUG("Registered for Windows device notifications");
  }
}

void WindowsUsbMonitor::unregisterDeviceNotification()
{
  if (m_hDevNotify) {
    UnregisterDeviceNotification(m_hDevNotify);
    m_hDevNotify = nullptr;
  }
}

QList<UsbDeviceInfo> WindowsUsbMonitor::enumerateDevices()
{
  QList<UsbDeviceInfo> devices;

  // Use UsbDeviceHelper to get connected devices (queryDevice=false to avoid opening ports unnecessarily)
  QMap<QString, QString> connectedDevices = UsbDeviceHelper::getConnectedDevices(false);

  for (auto it = connectedDevices.begin(); it != connectedDevices.end(); ++it) {
    UsbDeviceInfo info;
    info.devicePath = it.key();
    QString initialSerial = it.value();

    // Check if we already know this device and have a better serial number
    // Use base class m_devices for caching
    bool foundCached = false;
    if (m_devices.contains(info.devicePath)) {
      const auto &known = m_devices[info.devicePath];
      if (!known.serialNumber.isEmpty() && known.serialNumber != info.devicePath) {
        info.serialNumber = known.serialNumber;
        foundCached = true;
      }
    }

    if (!foundCached) {
      // If not cached, or cache was weak, check if the initial scan gave a good serial
      // (On Windows, getConnectedDevices(false) might return port name as serial if registry lookup fails)
      if (initialSerial.isEmpty() || initialSerial == info.devicePath || initialSerial.startsWith("COM")) {
        // Probe the device physically
        QString probedSerial = UsbDeviceHelper::readSerialNumber(info.devicePath);
        if (!probedSerial.isEmpty()) {
          info.serialNumber = probedSerial;
        } else {
          info.serialNumber = initialSerial;
        }
      } else {
        info.serialNumber = initialSerial;
      }
    }

    info.vendorId = UsbDeviceHelper::kEspressifVendorId; // Assume Espressif for now
    info.productId = UsbDeviceHelper::kEspressifProductId;

    if (matchesFilter(info)) {
      devices.append(info);
    }
  }

  return devices;
}

void WindowsUsbMonitor::checkDeviceChanges()
{
  processNewDeviceSnapshot(enumerateDevices());
}

LRESULT CALLBACK WindowsUsbMonitor::windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  if (msg == WM_DEVICECHANGE && s_instance) {
    if (wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE) {
      // Device change detected, check for changes
      LOG_DEBUG("WM_DEVICECHANGE received, checking device changes...");
      QMetaObject::invokeMethod(s_instance, &WindowsUsbMonitor::checkDeviceChanges, Qt::QueuedConnection);
      return TRUE;
    }
  }

  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace deskflow::gui
