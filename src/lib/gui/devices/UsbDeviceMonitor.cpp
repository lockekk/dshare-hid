/*
 * Deskflow-hid -- created by locke.huang@gmail.com
 */

#include "UsbDeviceMonitor.h"
#include <QCoreApplication>
#include <QSet>

namespace deskflow::gui {

UsbDeviceMonitor::UsbDeviceMonitor(QObject *parent) : QObject(parent)
{
}

void UsbDeviceMonitor::setVendorIdFilter(const QString &vendorId)
{
  m_vendorIdFilter = vendorId.toLower();
}

void UsbDeviceMonitor::setProductIdFilter(const QString &productId)
{
  m_productIdFilter = productId.toLower();
}

bool UsbDeviceMonitor::matchesFilter(const UsbDeviceInfo &device) const
{
  // If no filters set, match all devices
  if (m_vendorIdFilter.isEmpty() && m_productIdFilter.isEmpty()) {
    return true;
  }

  // Check vendor ID filter
  if (!m_vendorIdFilter.isEmpty() && device.vendorId.toLower() != m_vendorIdFilter) {
    return false;
  }

  // Check product ID filter
  if (!m_productIdFilter.isEmpty() && device.productId.toLower() != m_productIdFilter) {
    return false;
  }

  return true;
}

void UsbDeviceMonitor::processNewDeviceSnapshot(const QList<UsbDeviceInfo> &newSnapshot)
{
  // 1. Identify new devices
  for (const auto &device : newSnapshot) {
    if (!m_devices.contains(device.devicePath)) {
      m_devices.insert(device.devicePath, device);
      Q_EMIT deviceConnected(device);
    } else {
      // Optional: Update info if serial changed (handled by subclass logic usually, but we could do it here)
      // For now, identity is just path.
    }
  }

  // 2. Identify removed devices
  // Create a set of new paths for fast lookup
  QSet<QString> newPaths;
  for (const auto &device : newSnapshot) {
    newPaths.insert(device.devicePath);
  }

  // Iterate over current (old) devices
  auto it = m_devices.begin();
  while (it != m_devices.end()) {
    if (!newPaths.contains(it.key())) {
      UsbDeviceInfo removed = it.value();
      it = m_devices.erase(it);
      Q_EMIT deviceDisconnected(removed);
    } else {
      ++it;
    }
  }
}

} // namespace deskflow::gui
