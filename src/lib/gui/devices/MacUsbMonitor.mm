/*
 * Deskflow-hid -- created by locke.huang@gmail.com
 */

#include "MacUsbMonitor.h"
#include "UsbDeviceHelper.h"

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

namespace {

// RAII wrapper for IOKit objects (io_object_t, io_iterator_t, etc.)
struct ScopedIOObject
{
  io_object_t obj;

  explicit ScopedIOObject(io_object_t o = 0) : obj(o)
  {
  }
  ~ScopedIOObject()
  {
    if (obj) {
      IOObjectRelease(obj);
    }
  }

  // Disable copy
  ScopedIOObject(const ScopedIOObject &) = delete;
  ScopedIOObject &operator=(const ScopedIOObject &) = delete;

  // Allow move
  ScopedIOObject(ScopedIOObject &&other) noexcept : obj(other.obj)
  {
    other.obj = 0;
  }
  ScopedIOObject &operator=(ScopedIOObject &&other) noexcept
  {
    if (this != &other) {
      if (obj)
        IOObjectRelease(obj);
      obj = other.obj;
      other.obj = 0;
    }
    return *this;
  }

  operator io_object_t() const
  {
    return obj;
  }

  // Transfer ownership to caller
  io_object_t release()
  {
    io_object_t tmp = obj;
    obj = 0;
    return tmp;
  }

  // Reset with new object (releases old)
  void reset(io_object_t newObj = 0)
  {
    if (obj) {
      IOObjectRelease(obj);
    }
    obj = newObj;
  }
};

// RAII wrapper for CFTypeRef
// Note: CFTypeRef is void*, so we use a template or just void*
struct ScopedCFTypeRef
{
  CFTypeRef msg;

  explicit ScopedCFTypeRef(CFTypeRef m = nullptr) : msg(m)
  {
  }
  ~ScopedCFTypeRef()
  {
    if (msg) {
      CFRelease(msg);
    }
  }

  // Disable copy
  ScopedCFTypeRef(const ScopedCFTypeRef &) = delete;
  ScopedCFTypeRef &operator=(const ScopedCFTypeRef &) = delete;

  operator CFTypeRef() const
  {
    return msg;
  }

  template <typename T> T as() const
  {
    return (T)msg;
  }
};

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
  ScopedCFTypeRef property(IORegistryEntryCreateCFProperty(device, key, kCFAllocatorDefault, 0));
  if (property.msg) {
    if (CFGetTypeID(property.msg) == CFNumberGetTypeID()) {
      CFNumberGetValue((CFNumberRef)property.msg, kCFNumberLongType, &value);
    }
  }
  return value;
}

} // namespace

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

  // Clear pending retries
  for (auto it = m_pendingRetries.begin(); it != m_pendingRetries.end(); ++it) {
    if (it.value().device) {
      IOObjectRelease(it.value().device);
    }
  }
  m_pendingRetries.clear();

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
  io_iterator_t iterRef;
  kern_return_t kr = IORegistryEntryCreateIterator(usbDevice, kIOServicePlane, kIORegistryIterateRecursively, &iterRef);

  if (kr != kIOReturnSuccess)
    return QString();

  ScopedIOObject iter(iterRef);
  QString modemPath;
  io_service_t service;
  while ((service = IOIteratorNext(iter))) {
    ScopedIOObject childService(service);
    if (IOObjectConformsTo(service, "IOSerialBSDClient")) {
      ScopedCFTypeRef path(
          (CFStringRef)IORegistryEntryCreateCFProperty(service, CFSTR(kIOCalloutDeviceKey), kCFAllocatorDefault, 0)
      );
      if (path.msg) {
        modemPath = fromCFString((CFStringRef)path.msg);
      }
      if (!modemPath.isEmpty())
        break;
    }
  }
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
  ScopedCFTypeRef serial(
      (CFStringRef)IORegistryEntryCreateCFProperty(device, CFSTR(kUSBSerialNumberString), kCFAllocatorDefault, 0)
  );
  if (serial.msg) {
    info.serialNumber = fromCFString((CFStringRef)serial.msg);
  }

  // Device Path (CDC Modem)
  // We need to look for child CDC ACM interface and its serial client
  info.devicePath = findCdcModemPath(device);

  return info;
}
void MacUsbMonitor::processAddedDevices(io_iterator_t iterator)
{
  io_service_t deviceRef;
  int count = 0;
  while ((deviceRef = IOIteratorNext(iterator))) {
    ScopedIOObject device(deviceRef);
    count++;
    // Get a unique ID for the map
    uint64_t entryID = 0;
    IORegistryEntryGetRegistryEntryID(device, &entryID);

    // If we are already tracking this device, skip it.
    if (m_connectedDevices.contains(entryID)) {
      LOG_DEBUG("MacUsbMonitor: Already tracking ID: %llu - Skipping duplicate matching.", (unsigned long long)entryID);
      continue;
    }

    // Optimization: Extract just VID/PID first to check filter
    long vid = getLongProperty(device, CFSTR(kUSBVendorID));
    long pid = getLongProperty(device, CFSTR(kUSBProductID));
    QString vendorId = QString().asprintf("%04lx", (unsigned long)vid);
    QString productId = QString().asprintf("%04lx", (unsigned long)pid);

    UsbDeviceInfo tempInfo;
    tempInfo.vendorId = vendorId;
    tempInfo.productId = productId;

    if (!matchesFilter(tempInfo)) {
      continue;
    }

    LOG_DEBUG("MacUsbMonitor: Device matches filter. ID: %llu", (unsigned long long)entryID);

    // Attempt to connect immediately (Attempt 0)
    // We retain the device just in case we need to schedule retry, but for synchronous check,
    // we use the current reference.
    // Actually, tryConnectDevice might NOT retry if we call it directly?
    // Let's use scheduleRetry(..., 0) to keep logic unified, or call tryConnectDevice(..., 0).
    // Let's call tryConnect logic manually first to avoid overhead if it's ready.

    UsbDeviceInfo info = extractDeviceInfo(device);
    if (!info.devicePath.isEmpty()) {
      LOG_INFO(
          "MacUsbMonitor: Processing added device: %s : %s Path: %s ID: %llu", qPrintable(info.vendorId),
          qPrintable(info.productId), qPrintable(info.devicePath), (unsigned long long)entryID
      );
      if (!m_connectedDevices.contains(entryID)) {
        m_connectedDevices.insert(entryID, info);
        Q_EMIT deviceConnected(info);
      }
    } else {
      LOG_INFO("MacUsbMonitor: Device path empty, scheduling retry for ID: %llu", (unsigned long long)entryID);
      scheduleRetry(device, entryID, 1); // Start with attempt 1
    }
  }
}

void MacUsbMonitor::scheduleRetry(io_service_t device, uint64_t entryID, int attempt)
{
  if (attempt > 2) { // limit retries
    LOG_WARN("MacUsbMonitor: Max retries reached for ID: %llu. Giving up.", (unsigned long long)entryID);
    return;
  }

  // Retain device for storage
  if (IOObjectRetain(device) != kIOReturnSuccess) {
    LOG_WARN("MacUsbMonitor: Failed to retain device for retry ID: %llu", (unsigned long long)entryID);
    return;
  }

  PendingRetry pending;
  pending.device = device;
  pending.attempt = attempt;
  m_pendingRetries.insert(entryID, pending);

  int delayMs = (attempt == 1) ? 500 : 1000;
  QTimer::singleShot(delayMs, this, [this, entryID]() { executeRetry(entryID); });
}

void MacUsbMonitor::executeRetry(uint64_t entryID)
{
  if (!m_pendingRetries.contains(entryID)) {
    return; // Already handled or cancelled
  }

  PendingRetry pending = m_pendingRetries.take(entryID);
  ScopedIOObject device(pending.device); // Ensure release when done

  if (!m_monitoring)
    return;

  // Validate device is still alive
  uint64_t currentID = 0;
  if (IORegistryEntryGetRegistryEntryID(device, &currentID) != kIOReturnSuccess) {
    LOG_DEBUG("MacUsbMonitor: Device invalid during retry ID: %llu", (unsigned long long)entryID);
    return;
  }

  UsbDeviceInfo info = extractDeviceInfo(device);
  if (!info.devicePath.isEmpty()) {
    if (matchesFilter(info)) {
      LOG_INFO(
          "MacUsbMonitor: Retry success for ID: %llu Path: %s", (unsigned long long)entryID, qPrintable(info.devicePath)
      );
      if (!m_connectedDevices.contains(entryID)) {
        m_connectedDevices.insert(entryID, info);
        Q_EMIT deviceConnected(info);
      }
      return;
    }
  }

  // Still empty or no match, retry again if limits permit
  scheduleRetry(device, entryID, pending.attempt + 1);
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
