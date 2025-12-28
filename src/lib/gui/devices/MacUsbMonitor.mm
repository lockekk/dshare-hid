/*
 * Deskflow-hid -- created by locke.huang@gmail.com
 */

#include "MacUsbMonitor.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/usb/IOUSBLib.h>

#include <QDir>
#include <QObject>
#include <QString>
#include <QTextStream>
#include <QTimer>
#include <QtGlobal>

#include "../../base/Log.h"

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

  m_notifyPort = IONotificationPortCreate(kIOMainPortDefault);
  if (!m_notifyPort) {
    Q_EMIT errorOccurred("Failed to create IOKit notification port");
    return false;
  }

  CFRunLoopSourceRef runLoopSource = IONotificationPortGetRunLoopSource(m_notifyPort);
  CFRunLoopAddSource(CFRunLoopGetMain(), runLoopSource, kCFRunLoopCommonModes);

  // We want to monitor all USB devices to be broad and robust.
  // We'll filter in processAddedDevices in C++.
  const char *classes[] = {"IOUSBDevice", "IOUSBHostDevice"};
  kern_return_t kr;

  for (const char *className : classes) {
    // Addition matching
    CFMutableDictionaryRef addDict = IOServiceMatching(className);
    if (addDict) {
      io_iterator_t iter = 0;
      kr = IOServiceAddMatchingNotification(m_notifyPort, kIOMatchedNotification, addDict, onDeviceAdded, this, &iter);
      if (kr == kIOReturnSuccess) {
        LOG_DEBUG("MacUsbMonitor: Registered add notification for class: %s", className);
        processAddedDevices(iter);
        if (!m_addedIter)
          m_addedIter = iter;
        else
          IOObjectRelease(iter);
      } else {
        LOG_WARN("MacUsbMonitor: Failed to register add notification for class: %s", className);
      }
    }

    // Removal matching
    CFMutableDictionaryRef remDict = IOServiceMatching(className);
    if (remDict) {
      io_iterator_t iter = 0;
      kr = IOServiceAddMatchingNotification(
          m_notifyPort, kIOTerminatedNotification, remDict, onDeviceRemoved, this, &iter
      );
      if (kr == kIOReturnSuccess) {
        LOG_DEBUG("MacUsbMonitor: Registered removal notification for class: %s", className);
        processRemovedDevices(iter);
        if (!m_removedIter)
          m_removedIter = iter;
        else
          IOObjectRelease(iter);
      } else {
        LOG_WARN("MacUsbMonitor: Failed to register removal notification for class: %s", className);
      }
    }
  }

  m_monitoring = true;
  LOG_DEBUG("MacUsbMonitor: Monitoring fully started on Main RunLoop (CommonModes)");
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
  LOG_DEBUG("MacUsbMonitor: onDeviceAdded callback triggered");
  auto *self = static_cast<MacUsbMonitor *>(refCon);
  self->processAddedDevices(iterator);
}

void MacUsbMonitor::onDeviceRemoved(void *refCon, io_iterator_t iterator)
{
  LOG_DEBUG("MacUsbMonitor: onDeviceRemoved callback triggered");
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
  info.vendorId = QString().asprintf("%04lx", (unsigned long)vid);

  // Product ID
  long pid = getLongProperty(device, CFSTR(kUSBProductID));
  info.productId = QString().asprintf("%04lx", (unsigned long)pid);

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
  int count = 0;
  while ((device = IOIteratorNext(iterator))) {
    count++;
    // Get a unique ID for the map
    uint64_t entryID = 0;
    IORegistryEntryGetRegistryEntryID(device, &entryID);

    // If we are already tracking this device, skip it.
    // This happens because some devices match both IOUSBDevice and IOUSBHostDevice.
    if (m_connectedDevices.contains(entryID)) {
      LOG_DEBUG("MacUsbMonitor: Already tracking ID: %llu - Skipping duplicate matching.", (unsigned long long)entryID);
      IOObjectRelease(device);
      continue;
    }

    // Optimization: Extract just VID/PID first to check filter
    // This avoids expensive path finding (findCdcModemPath involves recursive iteration)
    // for every USB device (mice, keyboards, hubs, etc.)
    long vid = getLongProperty(device, CFSTR(kUSBVendorID));
    long pid = getLongProperty(device, CFSTR(kUSBProductID));
    QString vendorId = QString().asprintf("%04lx", (unsigned long)vid);
    QString productId = QString().asprintf("%04lx", (unsigned long)pid);

    // Initial check without path
    UsbDeviceInfo tempInfo;
    tempInfo.vendorId = vendorId;
    tempInfo.productId = productId;

    if (!matchesFilter(tempInfo)) {
      // Only log if debug is enabled to avoid spam
      IOObjectRelease(device);
      continue;
    }

    // Now it's a candidate, proceed with full extraction
    UsbDeviceInfo info = extractDeviceInfo(device);

    LOG_INFO(
        "MacUsbMonitor: Processing added device: %s : %s Path: %s ID: %llu", qPrintable(info.vendorId),
        qPrintable(info.productId), qPrintable(info.devicePath), (unsigned long long)entryID
    );

    // Do NOT release device yet, we might need it for retry

    if (info.devicePath.isEmpty()) {
      LOG_INFO("MacUsbMonitor: Device path empty, starting retry logic for ID: %llu", (unsigned long long)entryID);

      // Start retry chain (Attempt 1)
      if (IOObjectRetain(device) != kIOReturnSuccess) {
        LOG_WARN("MacUsbMonitor: Failed to retain device for retry, skipping ID: %llu", (unsigned long long)entryID);
        IOObjectRelease(device); // Release iterator's reference
        continue;
      }

      QTimer::singleShot(500, this, [this, device, entryID]() {
        ConnectResult res = tryConnectDevice(device, entryID);
        if (res == ConnectResult::Stop) {
          IOObjectRelease(device); // Release Timer 1 reference
          return;
        }

        // Attempt 2 (res == Retry)
        LOG_INFO("MacUsbMonitor: Retry 1 incomplete, scheduling Retry 2 for ID: %llu", (unsigned long long)entryID);

        // We need to retain for the second timer
        if (IOObjectRetain(device) != kIOReturnSuccess) {
          IOObjectRelease(device); // Release Timer 1 reference
          return;
        }

        QTimer::singleShot(1000, this, [this, device, entryID]() {
          tryConnectDevice(device, entryID); // Final attempt
          IOObjectRelease(device);           // Release Timer 2 reference
        });

        IOObjectRelease(device); // Release Timer 1 reference
      });

      IOObjectRelease(device); // Release the iterator's reference
      continue;
    }

    LOG_DEBUG("MacUsbMonitor: Device matches filter. Connecting ID: %llu", (unsigned long long)entryID);
    if (!m_connectedDevices.contains(entryID)) {
      m_connectedDevices.insert(entryID, info);
      Q_EMIT deviceConnected(info);
    }

    IOObjectRelease(device); // Release the iterator's reference
  }
  if (count == 0) {
    LOG_DEBUG("MacUsbMonitor: processAddedDevices called but no devices found in iterator");
  }
}

void MacUsbMonitor::processRemovedDevices(io_iterator_t iterator)
{
  io_service_t device;
  while ((device = IOIteratorNext(iterator))) {
    uint64_t entryID = 0;
    IORegistryEntryGetRegistryEntryID(device, &entryID);
    LOG_INFO("MacUsbMonitor: Processing removal for entryID: %llu", (unsigned long long)entryID);
    IOObjectRelease(device);

    if (m_connectedDevices.contains(entryID)) {
      UsbDeviceInfo info = m_connectedDevices.take(entryID);
      LOG_INFO("MacUsbMonitor: Found tracked device to remove: %s", qPrintable(info.devicePath));
      Q_EMIT deviceDisconnected(info);
    } else {
      LOG_INFO(
          "MacUsbMonitor: entryID %llu not in tracked list (size: %lld)", (unsigned long long)entryID,
          (long long)m_connectedDevices.size()
      );
      for (auto it = m_connectedDevices.begin(); it != m_connectedDevices.end(); ++it) {
        LOG_INFO("  Tracking ID: %llu Path: %s", (unsigned long long)it.key(), qPrintable(it.value().devicePath));
      }
    }
  }
}

MacUsbMonitor::ConnectResult MacUsbMonitor::tryConnectDevice(io_service_t device, uint64_t entryID)
{
  if (!m_monitoring) {
    return ConnectResult::Stop;
  }

  // Reuse validation block: Validate device is still alive in registry
  if (IORegistryEntryGetRegistryEntryID(device, &entryID) != kIOReturnSuccess) {
    LOG_DEBUG("MacUsbMonitor: Device became invalid during retry check");
    return ConnectResult::Stop;
  }

  UsbDeviceInfo info = extractDeviceInfo(device);

  if (!info.devicePath.isEmpty()) {
    if (matchesFilter(info)) {
      LOG_INFO(
          "MacUsbMonitor: Device connection successful for ID: %llu Path: %s", (unsigned long long)entryID,
          qPrintable(info.devicePath)
      );
      if (!m_connectedDevices.contains(entryID)) {
        m_connectedDevices.insert(entryID, info);
        Q_EMIT deviceConnected(info);
      }
      return ConnectResult::Stop; // Success -> Stop retrying
    } else {
      // Path found but not our device -> Stop retrying
      return ConnectResult::Stop;
    }
  }

  // Path empty -> Retry
  return ConnectResult::Retry;
}

} // namespace deskflow::gui
