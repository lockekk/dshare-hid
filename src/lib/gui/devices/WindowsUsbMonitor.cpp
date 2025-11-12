// SPDX-FileCopyrightText: 2025 Deskflow Developers
// SPDX-License-Identifier: MIT

#include "WindowsUsbMonitor.h"
#include "UsbDeviceHelper.h"

#include <QDebug>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbt.h>
#include <setupapi.h>
#include <initguid.h>
#include <devguid.h>

namespace deskflow::gui {

// Static instance pointer for window procedure callback
WindowsUsbMonitor* WindowsUsbMonitor::s_instance = nullptr;

WindowsUsbMonitor::WindowsUsbMonitor(QObject *parent)
    : UsbDeviceMonitor(parent)
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
    qWarning() << "Windows USB monitoring is already active";
    return true;
  }

  qDebug() << "Starting Windows USB device monitoring...";

  // Create a hidden window to receive device change messages
  WNDCLASSW wc = {};
  wc.lpfnWndProc = windowProc;
  wc.hInstance = GetModuleHandle(nullptr);
  wc.lpszClassName = L"DeskflowUsbMonitorClass";

  if (!RegisterClassW(&wc)) {
    DWORD error = GetLastError();
    if (error != ERROR_CLASS_ALREADY_EXISTS) {
      qWarning() << "Failed to register window class:" << error;
      return false;
    }
  }

  m_hwnd = CreateWindowW(
    L"DeskflowUsbMonitorClass",
    L"Deskflow USB Monitor",
    0, 0, 0, 0, 0,
    HWND_MESSAGE,  // Message-only window
    nullptr,
    GetModuleHandle(nullptr),
    nullptr
  );

  if (!m_hwnd) {
    qWarning() << "Failed to create message window:" << GetLastError();
    return false;
  }

  registerDeviceNotification();

  // Store initial device list
  m_lastKnownDevices = enumerateDevices();

  m_monitoring = true;
  qDebug() << "Windows USB device monitoring started successfully";

  return true;
}

void WindowsUsbMonitor::stopMonitoring()
{
  if (!m_monitoring) {
    return;
  }

  qDebug() << "Stopping Windows USB device monitoring...";

  unregisterDeviceNotification();

  if (m_hwnd) {
    DestroyWindow(m_hwnd);
    m_hwnd = nullptr;
  }

  m_monitoring = false;
  qDebug() << "Windows USB device monitoring stopped";
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
    m_hwnd,
    &notificationFilter,
    DEVICE_NOTIFY_WINDOW_HANDLE | DEVICE_NOTIFY_ALL_INTERFACE_CLASSES
  );

  if (!m_hDevNotify) {
    qWarning() << "Failed to register device notification:" << GetLastError();
  } else {
    qDebug() << "Registered for Windows device notifications";
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

  // Use UsbDeviceHelper to get connected devices
  QMap<QString, QString> connectedDevices = UsbDeviceHelper::getConnectedDevices();

  for (auto it = connectedDevices.begin(); it != connectedDevices.end(); ++it) {
    UsbDeviceInfo info;
    info.devicePath = it.key();
    info.serialNumber = it.value();
    info.vendorId = UsbDeviceHelper::kEspressifVendorId;  // Assume Espressif for now
    info.productId = UsbDeviceHelper::kEspressifProductId;

    if (matchesFilter(info)) {
      devices.append(info);
    }
  }

  return devices;
}

void WindowsUsbMonitor::checkDeviceChanges()
{
  QList<UsbDeviceInfo> currentDevices = enumerateDevices();

  // Check for newly connected devices
  for (const auto &device : currentDevices) {
    bool wasKnown = false;
    for (const auto &known : m_lastKnownDevices) {
      if (device.devicePath == known.devicePath) {
        wasKnown = true;
        break;
      }
    }

    if (!wasKnown) {
      qDebug() << "Detected new device:" << device.devicePath;
      Q_EMIT deviceConnected(device);
    }
  }

  // Check for disconnected devices
  for (const auto &known : m_lastKnownDevices) {
    bool stillPresent = false;
    for (const auto &device : currentDevices) {
      if (device.devicePath == known.devicePath) {
        stillPresent = true;
        break;
      }
    }

    if (!stillPresent) {
      qDebug() << "Detected device removal:" << known.devicePath;
      Q_EMIT deviceDisconnected(known);
    }
  }

  m_lastKnownDevices = currentDevices;
}

LRESULT CALLBACK WindowsUsbMonitor::windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  if (msg == WM_DEVICECHANGE && s_instance) {
    if (wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE) {
      // Device change detected, check for changes
      qDebug() << "WM_DEVICECHANGE received, checking device changes...";
      QMetaObject::invokeMethod(s_instance, &WindowsUsbMonitor::checkDeviceChanges, Qt::QueuedConnection);
      return TRUE;
    }
  }

  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace deskflow::gui
