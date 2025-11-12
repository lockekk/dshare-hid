// SPDX-FileCopyrightText: 2025 Deskflow Developers
// SPDX-License-Identifier: MIT

#pragma once

#include "UsbDeviceMonitor.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace deskflow::gui {

/**
 * @brief Windows USB device monitor using WM_DEVICECHANGE messages
 *
 * This implementation monitors USB device plug/unplug events on Windows
 * by creating a hidden window to receive WM_DEVICECHANGE messages.
 */
class WindowsUsbMonitor : public UsbDeviceMonitor {
  Q_OBJECT

public:
  explicit WindowsUsbMonitor(QObject *parent = nullptr);
  ~WindowsUsbMonitor() override;

  bool startMonitoring() override;
  void stopMonitoring() override;
  bool isMonitoring() const override;
  QList<UsbDeviceInfo> enumerateDevices() override;

private:
  void registerDeviceNotification();
  void unregisterDeviceNotification();
  void checkDeviceChanges();

  static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  QList<UsbDeviceInfo> m_lastKnownDevices;
  HWND m_hwnd = nullptr;
  HDEVNOTIFY m_hDevNotify = nullptr;
  bool m_monitoring = false;

  static WindowsUsbMonitor* s_instance;
};

} // namespace deskflow::gui
