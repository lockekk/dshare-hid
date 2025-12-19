// SPDX-FileCopyrightText: 2025 Deskflow Developers
// SPDX-License-Identifier: MIT

#include "MacUsbMonitor.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/usb/IOUSBLib.h>

#include <QDebug>
#include <QDir>
#include <QTextStream>

namespace deskflow::gui {

// Helper to convert CFString to QString
static QString fromCFString(CFStringRef cfStr)
{
  if (!cfStr)
    return QString();
  CFIndex length = CFStringGetLength(cfStr);
  CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
  std::vector<char> buffer(maxSize);
  if (CFStringGetCString(cfStr, buffer.data(), maxSize, kCFStringEncodingUTF8)) {
    return QString::fromUtf8(buffer.data());
  }
  return QString();
}

// Helper to get number from registry property
static long getLongProperty(io_service_t device, CFStringRef key)
{
  long value = 0;
  CFTypeRef property = IORegistryEntryCreateCFProperty(device, key, kCFAllocatorDefault, 0);
  if (property) {
    if (CFGetTypeID(property) == CFNumberGetTypeID()) {
      CFNumberGetValue((CFNumberRef)property, kCFNumberLongType, &value);
    }
    CFRelease(property);
  }
  return value;
}

MacUsbMonitor::MacUsbMonitor(QObject *parent) : UsbDeviceMonitor(parent)
{
}

MacUsbMonitor::~MacUsbMonitor()
{
  stopMonitoring();
}

bool MacUsbMonitor::startMonitoring()
{
  if (m_monitoring)
    return true;

  m_notifyPort = IONotificationPortCreate(kIOMasterPortDefault);
  if (!m_notifyPort) {
    emit errorOccurred("Failed to create IOKit notification port");
    return false;
  }

  IONotificationPortSetDispatchQueue(m_notifyPort, dispatch_get_main_queue());

  // Create matching dictionary for USB devices
  CFMutableDictionaryRef matchingDict = IOServiceMatching(kIOUSBDeviceClassName);
  if (!matchingDict) {
    emit errorOccurred("Failed to create matching dictionary");
    stopMonitoring();
    return false;
  }

  // Filter by Vendor ID if set
  if (!vendorIdFilter().isEmpty()) {
    bool ok;
    int vid = vendorIdFilter().toInt(&ok, 16);
    if (ok) {
      CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &vid);
      CFDictionarySetValue(matchingDict, CFSTR(kUSBVendorID), number);
      CFRelease(number);
    }
  }

  // Filter by Product ID if set
  if (!productIdFilter().isEmpty()) {
    bool ok;
    int pid = productIdFilter().toInt(&ok, 16);
    if (ok) {
      CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &pid);
      CFDictionarySetValue(matchingDict, CFSTR(kUSBProductID), number);
      CFRelease(number);
    }
  }

  // Add notification for device matching (Added)
  // Note: IOServiceAddMatchingNotification consumes a reference to matchingDict, so we retain it for the second call
  CFRetain(matchingDict);

  kern_return_t kr = IOServiceAddMatchingNotification(
      m_notifyPort, kIOFirstMatchNotification, matchingDict, onDeviceAdded, this, &m_addedIter
  );

  if (kr != kIOReturnSuccess) {
    emit errorOccurred("Failed to add matching notification");
    CFRelease(matchingDict); // Release retained copy
    stopMonitoring();
    return false;
  }

  // Process existing devices
  processAddedDevices(m_addedIter);

  // Add notification for device removal (Terminated)
  kr = IOServiceAddMatchingNotification(
      m_notifyPort, kIOTerminatedNotification, matchingDict, onDeviceRemoved, this, &m_removedIter
  );

  if (kr != kIOReturnSuccess) {
    emit errorOccurred("Failed to add termination notification");
    stopMonitoring();
    return false;
  }

  // Drain removed iterator
  processRemovedDevices(m_removedIter);

  m_monitoring = true;
  return true;
}

void MacUsbMonitor::stopMonitoring()
{
  if (!m_monitoring)
    return;

  if (m_addedIter) {
    IOObjectRelease(m_addedIter);
    m_addedIter = 0;
  }

  if (m_removedIter) {
    IOObjectRelease(m_removedIter);
    m_removedIter = 0;
  }

  if (m_notifyPort) {
    IONotificationPortDestroy(m_notifyPort);
    m_notifyPort = nullptr;
  }

  m_monitoring = false;
  m_connectedDevices.clear();
}

bool MacUsbMonitor::isMonitoring() const
{
  return m_monitoring;
}

QList<UsbDeviceInfo> MacUsbMonitor::enumerateDevices()
{
  // Just return cached list which matches IOKit state
  return m_connectedDevices.values();
}

void MacUsbMonitor::onDeviceAdded(void *refCon, io_iterator_t iterator)
{
  auto *self = static_cast<MacUsbMonitor *>(refCon);
  self->processAddedDevices(iterator);
}

void MacUsbMonitor::onDeviceRemoved(void *refCon, io_iterator_t iterator)
{
  auto *self = static_cast<MacUsbMonitor *>(refCon);
  self->processRemovedDevices(iterator);
}

// Find CDC ACM modem path for a USB device
static QString findCdcModemPath(io_service_t usbDevice)
{
  // Iterate children to find IOSerialBSDClient
  io_iterator_t iter;
  kern_return_t kr = IORegistryEntryCreateIterator(usbDevice, kIOServicePlane, kIORegistryIterateRecursively, &iter);

  if (kr != kIOReturnSuccess)
    return QString();

  QString modemPath;
  io_service_t service;
  while ((service = IOIteratorNext(iter))) {
    if (IOObjectConformsTo(service, "IOSerialBSDClient")) {
      CFStringRef path =
          (CFStringRef)IORegistryEntryCreateCFProperty(service, CFSTR(kIOCalloutDeviceKey), kCFAllocatorDefault, 0);
      if (path) {
        modemPath = fromCFString(path);
        CFRelease(path);
      }
      IOObjectRelease(service);
      if (!modemPath.isEmpty())
        break;
    } else {
      IOObjectRelease(service);
    }
  }
  IOObjectRelease(iter);
  return modemPath;
}

UsbDeviceInfo MacUsbMonitor::extractDeviceInfo(io_service_t device)
{
  UsbDeviceInfo info;

  // Vendor ID
  long vid = getLongProperty(device, CFSTR(kUSBVendorID));
  info.vendorId = QString().asprintf("%04lx", vid);

  // Product ID
  long pid = getLongProperty(device, CFSTR(kUSBProductID));
  info.productId = QString().asprintf("%04lx", pid);

  // Serial Number
  CFStringRef serial =
      (CFStringRef)IORegistryEntryCreateCFProperty(device, CFSTR(kUSBSerialNumberString), kCFAllocatorDefault, 0);
  if (serial) {
    info.serialNumber = fromCFString(serial);
    CFRelease(serial);
  }

  // Device Path (CDC Modem)
  // We need to look for child CDC ACM interface and its serial client
  info.devicePath = findCdcModemPath(device);

  return info;
}

void MacUsbMonitor::processAddedDevices(io_iterator_t iterator)
{
  io_service_t device;
  while ((device = IOIteratorNext(iterator))) {
    UsbDeviceInfo info = extractDeviceInfo(device);

    // Get a unique ID for the map
    uint64_t entryID = 0;
    IORegistryEntryGetRegistryEntryID(device, &entryID);

    IOObjectRelease(device);

    if (info.devicePath.isEmpty()) {
      // It might be that the driver hasn't loaded yet?
      // Or maybe it's not a CDC device we care about despite VID matching
      // If we filtered by VID/PID, we assume it's relevant, but without a path we can't open it.
      // We'll skip for now.
      continue;
    }

    if (matchesFilter(info)) {
      if (!m_connectedDevices.contains(entryID)) {
        m_connectedDevices.insert(entryID, info);
        emit deviceConnected(info);
      }
    }
  }
}

void MacUsbMonitor::processRemovedDevices(io_iterator_t iterator)
{
  io_service_t device;
  while ((device = IOIteratorNext(iterator))) {
    uint64_t entryID = 0;
    IORegistryEntryGetRegistryEntryID(device, &entryID);
    IOObjectRelease(device);

    if (m_connectedDevices.contains(entryID)) {
      UsbDeviceInfo info = m_connectedDevices.take(entryID);
      emit deviceDisconnected(info);
    }
  }
}

} // namespace deskflow::gui
