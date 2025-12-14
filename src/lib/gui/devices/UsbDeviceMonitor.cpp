#include "UsbDeviceMonitor.h"
#include <QCoreApplication>

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

} // namespace deskflow::gui
