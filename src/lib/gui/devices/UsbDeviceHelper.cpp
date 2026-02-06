/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#include "UsbDeviceHelper.h"

#include "platform/bridge/CdcTransport.h"

#include "base/Log.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QTextStream>
#include <algorithm>
#include <cwchar>
#include <vector>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
// clang-format off
#include <Windows.h>
#include <SetupAPI.h>
// clang-format on
#include <devguid.h>
#include <initguid.h>
#include <winreg.h>
#endif

#ifdef Q_OS_MAC
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/usb/IOUSBLib.h>
#endif

namespace deskflow::gui {

namespace {

#ifdef Q_OS_LINUX
#include <libudev.h>
QString canonicalUsbDevicePath(const QString &devicePath)
{
  QFileInfo deviceInfo(devicePath);
  QString ttyName = deviceInfo.fileName();

  const QString ttyBasePath = QStringLiteral("/sys/class/tty/%1").arg(ttyName);
  QFileInfo ttyLink(ttyBasePath);
  if (!ttyLink.exists()) {
    LOG_WARN("TTY entry does not exist for %s : %s", qPrintable(devicePath), qPrintable(ttyLink.absoluteFilePath()));
    return QString();
  }

  QFileInfo deviceLink(ttyBasePath + QStringLiteral("/device"));
  return deviceLink.canonicalFilePath();
}

QString readUsbAttribute(const QString &canonicalDevicePath, const QString &attribute)
{
  if (canonicalDevicePath.isEmpty())
    return QString();

  QDir currentDir(canonicalDevicePath);
  constexpr int kMaxTraversal = 6;
  for (int depth = 0; depth < kMaxTraversal; ++depth) {
    const QString candidate = currentDir.absoluteFilePath(attribute);
    QFile file(candidate);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      QTextStream in(&file);
      const QString value = in.readLine().trimmed();
      file.close();
      if (!value.isEmpty())
        return value;
    }
    if (!currentDir.cdUp())
      break;
  }

  return QString();
}
#endif

#ifdef Q_OS_WIN
QString windowsVendorPattern(const QString &vendorId)
{
  return QStringLiteral("VID_%1").arg(vendorId.toUpper());
}

QString windowsProductPattern(const QString &productId)
{
  return QStringLiteral("PID_%1").arg(productId.toUpper());
}

bool matchesHardwareId(const QString &id, const QString &vendorId, const QString &productId)
{
  const QString upper = id.toUpper();
  const QString vendorPattern = windowsVendorPattern(vendorId);
  const QString productPattern = windowsProductPattern(productId);
  if (!upper.contains(vendorPattern)) {
    return false;
  }
  if (productId.isEmpty()) {
    return true;
  }
  return upper.contains(productPattern);
}

bool deviceMatchesBridge(HDEVINFO infoSet, SP_DEVINFO_DATA &infoData, const QString &vendorId, const QString &productId)
{
  std::vector<wchar_t> buffer(1024);
  DWORD propertyType = 0;
  while (true) {
    DWORD requiredSize = 0;
    if (SetupDiGetDeviceRegistryPropertyW(
            infoSet, &infoData, SPDRP_HARDWAREID, &propertyType, reinterpret_cast<PBYTE>(buffer.data()),
            static_cast<DWORD>(buffer.size() * sizeof(wchar_t)), &requiredSize
        )) {
      break;
    }
    DWORD error = GetLastError();
    if (error == ERROR_INSUFFICIENT_BUFFER) {
      const size_t newSize = (requiredSize / sizeof(wchar_t)) + 1;
      buffer.resize(std::max(buffer.size() * 2, newSize));
      continue;
    }
    return false;
  }

  if (buffer.empty()) {
    return false;
  }

  const wchar_t *entry = buffer.data();
  while (*entry != L'\0') {
    const QString hardwareId = QString::fromWCharArray(entry);
    if (matchesHardwareId(hardwareId, vendorId, productId)) {
      return true;
    }
    entry += wcslen(entry) + 1;
  }

  return false;
}

QString getPortName(HDEVINFO infoSet, SP_DEVINFO_DATA &infoData)
{
  HKEY key = SetupDiOpenDevRegKey(infoSet, &infoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
  if (key == INVALID_HANDLE_VALUE) {
    return QString();
  }

  wchar_t buffer[256] = {0};
  DWORD type = 0;
  DWORD size = sizeof(buffer);
  QString portName;

  if (RegQueryValueExW(key, L"PortName", nullptr, &type, reinterpret_cast<LPBYTE>(buffer), &size) == ERROR_SUCCESS &&
      (type == REG_SZ || type == REG_EXPAND_SZ)) {
    portName = QString::fromWCharArray(buffer).trimmed();
  }

  RegCloseKey(key);
  return portName;
}
#endif

} // namespace

QString UsbDeviceHelper::readSerialNumber(const QString &devicePath)
{
#if defined(Q_OS_LINUX) || defined(Q_OS_WIN) || defined(Q_OS_MAC)
  // Read serial number via CDC command from firmware
  // This ensures we only read when device is not opened by bridge client

  LOG_DEBUG("Reading serial number via CDC for device: %s", qPrintable(devicePath));

  try {
    // Create CDC transport instance
    LOG_DEBUG("Creating transport for %s", qPrintable(devicePath));
    deskflow::bridge::CdcTransport transport(devicePath);

    // Check if device is busy by attempting to open it
    // This will fail if the bridge client already has it open
    LOG_DEBUG("Opening transport for %s", qPrintable(devicePath));
    if (!transport.open()) {
      LOG_DEBUG("Device is busy or not accessible (likely opened by bridge client): %s", qPrintable(devicePath));
      return QString(); // Device is in use, don't block
    }
    LOG_DEBUG("Transport opened successfully for %s", qPrintable(devicePath));

    // Read serial number via CDC command
    std::string serial;
    LOG_DEBUG("Fetching serial number for %s", qPrintable(devicePath));
    if (transport.fetchSerialNumber(serial)) {
      LOG_DEBUG("Closing transport for %s", qPrintable(devicePath));
      transport.close();
      LOG_DEBUG("Converting serial");
      QString qSerial = QString::fromStdString(serial);
      if (!qSerial.isEmpty()) {
        LOG_DEBUG("Read serial number via CDC for %s : %s", qPrintable(devicePath), qPrintable(qSerial));
        return qSerial;
      } else {
        LOG_DEBUG("Device returned empty serial number: %s", qPrintable(devicePath));
        return QString();
      }
    } else {
      LOG_WARN(
          "Failed to read serial number via CDC for %s error: %s", qPrintable(devicePath), transport.lastError().c_str()
      );
      transport.close();
      return QString();
    }

  } catch (const std::exception &e) {
    LOG_WARN("Exception reading serial number via CDC for %s error: %s", qPrintable(devicePath), e.what());
    return QString();
  }

#elif defined(Q_OS_IOS)
  // TODO: Implement CDC serial number reading for iOS
  // iOS has limited access to CDC devices, may need different approach
  LOG_WARN("Serial number reading not implemented for iOS platform");
  return QString();

#else
  LOG_WARN("Serial number reading not implemented for this platform");
  return QString();
#endif
}

QMap<QString, QString> UsbDeviceHelper::getConnectedDevices(bool queryDevice)
{
  QMap<QString, QString> devices;

#ifdef Q_OS_LINUX
  // Use libudev to enumerate devices, matching LinuxUdevMonitor's logic
  struct udev *udevCtx = udev_new();
  if (!udevCtx) {
    LOG_WARN("Failed to create udev context for enumeration");
    // Fallback? Or just return empty.
    return devices;
  }

  struct udev_enumerate *enumerate = udev_enumerate_new(udevCtx);
  if (enumerate) {
    udev_enumerate_add_match_subsystem(enumerate, "tty");
    udev_enumerate_scan_devices(enumerate);

    struct udev_list_entry *deviceList = udev_enumerate_get_list_entry(enumerate);
    struct udev_list_entry *entry;

    udev_list_entry_foreach(entry, deviceList)
    {
      const char *path = udev_list_entry_get_name(entry);
      struct udev_device *device = udev_device_new_from_syspath(udevCtx, path);
      if (device) {
        const char *devNode = udev_device_get_devnode(device);
        if (devNode) {
          QString devicePath = QString::fromUtf8(devNode);
          if (isSupportedBridgeDevice(devicePath)) {
            QString serialNumber;
            // On Linux, we can try to read serial from udev attributes directly first
            // to avoid opening the port (which might be busy)
            struct udev_device *parent = udev_device_get_parent_with_subsystem_devtype(device, "usb", "usb_device");
            if (parent) {
              const char *serial = udev_device_get_sysattr_value(parent, "serial");
              if (serial) {
                serialNumber = QString::fromUtf8(serial);
              }
            }

            if (queryDevice && serialNumber.isEmpty()) {
              serialNumber = readSerialNumber(devicePath);
            }

            if (serialNumber.isEmpty()) {
              LOG_DEBUG("Device %s has no serial (yet), including for monitoring", qPrintable(devicePath));
            }
            devices[devicePath] = serialNumber;
          }
        }
        udev_device_unref(device);
      }
    }
    udev_enumerate_unref(enumerate);
  }
  udev_unref(udevCtx);

  LOG_DEBUG("Found %d connected USB CDC devices via libudev", devices.size());
#elif defined(Q_OS_WIN)
  GUID classGuid = GUID_DEVCLASS_PORTS;
  HDEVINFO deviceInfoSet = SetupDiGetClassDevsW(&classGuid, nullptr, nullptr, DIGCF_PRESENT);
  if (deviceInfoSet == INVALID_HANDLE_VALUE) {
    LOG_WARN("Failed to enumerate COM ports: %lu", GetLastError());
    return devices;
  }

  SP_DEVINFO_DATA devInfoData = {};
  devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
  for (DWORD index = 0; SetupDiEnumDeviceInfo(deviceInfoSet, index, &devInfoData); ++index) {
    if (!deviceMatchesBridge(
            deviceInfoSet, devInfoData, UsbDeviceHelper::kEspressifVendorId, UsbDeviceHelper::kEspressifProductId
        )) {
      continue;
    }

    QString portName = getPortName(deviceInfoSet, devInfoData);
    if (portName.isEmpty() || !portName.startsWith(QStringLiteral("COM"), Qt::CaseInsensitive)) {
      continue;
    }

    const QString devicePath = QStringLiteral("\\\\.\\%1").arg(portName);
    QString serialNumber;

    // optimization: try to read serial number from device instance ID (registry)
    // format: USB\VID_v&PID_p\Serial_Number
    wchar_t instanceId[MAX_PATH] = {0};
    if (SetupDiGetDeviceInstanceIdW(
            deviceInfoSet, &devInfoData, instanceId, sizeof(instanceId) / sizeof(wchar_t), nullptr
        )) {
      QString instanceIdStr = QString::fromWCharArray(instanceId);
      int lastSlash = instanceIdStr.lastIndexOf('\\');
      if (lastSlash != -1) {
        QString possibleSerial = instanceIdStr.mid(lastSlash + 1);
        if (!possibleSerial.isEmpty() && !possibleSerial.contains('&')) {
          serialNumber = possibleSerial;
          LOG_DEBUG("Read serial number from registry for %s : %s", qPrintable(devicePath), qPrintable(serialNumber));
        }
      }
    }

    if (queryDevice && serialNumber.isEmpty()) {
      serialNumber = readSerialNumber(devicePath);
    }
    if (serialNumber.isEmpty()) {
      LOG_DEBUG("Device %s has no serial (yet), including for monitoring", qPrintable(devicePath));
    }

    devices[devicePath] = serialNumber;
    LOG_DEBUG("Found bridge device at %s", qPrintable(devicePath));
  }

  SetupDiDestroyDeviceInfoList(deviceInfoSet);
  LOG_DEBUG("Enumerated %d bridge-capable USB CDC device(s) on Windows", devices.size());
#elif defined(Q_OS_MAC)
  // macOS implementation using IOKit
  // We check IOUSBHostDevice (newer/iPad/Apple Silicon) only, dropping legacy IOUSBDevice
  const char *className = "IOUSBHostDevice";

  io_iterator_t iter = 0;
  kern_return_t kr = IOServiceGetMatchingServices(kIOMainPortDefault, IOServiceMatching(className), &iter);
  if (kr != kIOReturnSuccess) {
    LOG_WARN("Failed to iterate USB devices for class %s on macOS", className);
    return devices;
  }

  io_service_t device;
  while ((device = IOIteratorNext(iter))) {
    // Check Vendor ID
    long vid = 0;
    CFTypeRef vidRef = IORegistryEntryCreateCFProperty(device, CFSTR(kUSBVendorID), kCFAllocatorDefault, 0);
    if (vidRef) {
      if (CFGetTypeID(vidRef) == CFNumberGetTypeID()) {
        CFNumberGetValue((CFNumberRef)vidRef, kCFNumberLongType, &vid);
      }
      CFRelease(vidRef);
    }

    if (vid == 0x303a) { // kEspressifVendorId
      // Check Product ID
      long pid = 0;
      CFTypeRef pidRef = IORegistryEntryCreateCFProperty(device, CFSTR(kUSBProductID), kCFAllocatorDefault, 0);
      if (pidRef) {
        if (CFGetTypeID(pidRef) == CFNumberGetTypeID()) {
          CFNumberGetValue((CFNumberRef)pidRef, kCFNumberLongType, &pid);
        }
        CFRelease(pidRef);
      }

      if (pid == 0x1001) { // kEspressifProductId
        // Found matching device, look for CDC Modem path
        QString devicePath;
        // Search children for IOSerialBSDClient
        io_iterator_t childIter;
        if (IORegistryEntryCreateIterator(device, kIOServicePlane, kIORegistryIterateRecursively, &childIter) ==
            kIOReturnSuccess) {
          io_service_t child;
          while ((child = IOIteratorNext(childIter))) {
            if (IOObjectConformsTo(child, "IOSerialBSDClient")) {
              CFStringRef pathRef = (CFStringRef
              )IORegistryEntryCreateCFProperty(child, CFSTR(kIOCalloutDeviceKey), kCFAllocatorDefault, 0);
              if (pathRef) {
                const CFIndex kMaxPath = 1024;
                char pathBuf[kMaxPath];
                if (CFStringGetCString(pathRef, pathBuf, kMaxPath, kCFStringEncodingUTF8)) {
                  devicePath = QString::fromUtf8(pathBuf);
                }
                CFRelease(pathRef);
              }
            }
            IOObjectRelease(child);
            if (!devicePath.isEmpty())
              break;
          }
          IOObjectRelease(childIter);
        }

        if (!devicePath.isEmpty()) {
          QString serialNumber;
          if (queryDevice) {
            serialNumber = readSerialNumber(devicePath);
          }
          if (serialNumber.isEmpty()) {
            // Fallback to reading from IOKit if possible (though readSerialNumber tries CDC first)
            // IOKit serial is kUSBSerialNumberString
            CFStringRef serialRef = (CFStringRef
            )IORegistryEntryCreateCFProperty(device, CFSTR(kUSBSerialNumberString), kCFAllocatorDefault, 0);
            if (serialRef) {
              const CFIndex kMaxSerial = 256;
              char serialBuf[kMaxSerial];
              if (CFStringGetCString(serialRef, serialBuf, kMaxSerial, kCFStringEncodingUTF8)) {
                serialNumber = QString::fromUtf8(serialBuf);
              }
              CFRelease(serialRef);
            }
          }
          if (serialNumber.isEmpty()) {
            LOG_DEBUG("Device %s has no serial (yet), including for monitoring", qPrintable(devicePath));
          }
          if (!devices.contains(devicePath)) {
            devices[devicePath] = serialNumber;
          }
        }
      }
    }
    IOObjectRelease(device);
  }
  IOObjectRelease(iter);
#endif

  return devices;
}

bool UsbDeviceHelper::isSupportedBridgeDevice(const QString &devicePath)
{
#ifdef Q_OS_LINUX
  const QString canonicalPath = canonicalUsbDevicePath(devicePath);
  const QString vendorId = readUsbAttribute(canonicalPath, QStringLiteral("idVendor")).toLower();
  const QString productId = readUsbAttribute(canonicalPath, QStringLiteral("idProduct")).toLower();
  if (vendorId.isEmpty())
    return false;

  if (vendorId == kEspressifVendorId) {
    if (productId.isEmpty() || productId == kEspressifProductId) {
      return true;
    }
    LOG_DEBUG(
        "Device %s has Espressif vendor id but unexpected product id %s", qPrintable(devicePath), qPrintable(productId)
    );
    return true;
  }

  LOG_DEBUG("Device %s has unsupported vendor id %s", qPrintable(devicePath), qPrintable(vendorId));
  return false;
#elif defined(Q_OS_WIN)
  // On Windows, try to open and handshake with the device
  // This is simpler than querying hardware IDs from the registry
  deskflow::bridge::CdcTransport transport(devicePath);
  if (!transport.open()) {
    return false;
  }

  bool isSupported = transport.hasDeviceConfig();
  transport.close();
  return isSupported;
#elif defined(Q_OS_MAC)
  // On macOS we can check IOKit registry property for the device path
  // Reverse lookup from BSD path to IOUSBDevice is hard without iterating
  // Simpler to just try handshake if we have path, or trust getConnectedDevices filter
  // Let's rely on handshake for robust check

  // Note: We could optimize by checking if the path is in the list returned by getConnectedDevices(false)
  // but that iterates IOKit anyway.

  deskflow::bridge::CdcTransport transport(devicePath);
  if (!transport.open()) {
    return false;
  }
  bool isSupported = transport.hasDeviceConfig();
  transport.close();
  return isSupported;
#else
  Q_UNUSED(devicePath);
  return false;
#endif
}

bool UsbDeviceHelper::verifyBridgeHandshake(
    const QString &devicePath, deskflow::bridge::FirmwareConfig *configOut, int timeoutMs
)
{
#if defined(Q_OS_LINUX) || defined(Q_OS_WIN) || defined(Q_OS_MAC)
  Q_UNUSED(timeoutMs);

  deskflow::bridge::CdcTransport transport(devicePath);
  if (!transport.open()) {
    LOG_WARN("Bridge handshake failed for %s : %s", qPrintable(devicePath), transport.lastError().c_str());
    return false;
  }

  if (transport.hasDeviceConfig()) {
    const auto &cfg = transport.deviceConfig();
    if (configOut != nullptr) {
      *configOut = cfg;
    }

    std::string deviceName;
    if (transport.fetchDeviceName(deviceName)) {
      if (configOut != nullptr) {
        configOut->deviceName = deviceName;
      }
    } else {
      LOG_WARN("Unable to fetch device name: %s", transport.lastError().c_str());
    }

    const QString nameForLog = QString::fromStdString(cfg.deviceName);

    LOG_INFO(
        "Bridge handshake successful with %s proto: %d activation_state: %s (%d) fw_bcd: %d hw_bcd: %d name: %s",
        qPrintable(devicePath), cfg.protocolVersion, cfg.activationStateString(),
        static_cast<unsigned>(cfg.activationState), cfg.firmwareVersionBcd, cfg.hardwareVersionBcd,
        qPrintable(nameForLog)
    );
  } else {
    LOG_INFO("Bridge handshake successful with %s (no config payload)", qPrintable(devicePath));
  }

  transport.close();
  return true;
#else
  Q_UNUSED(devicePath)
  Q_UNUSED(timeoutMs)
  Q_UNUSED(configOut)
  return false;
#endif
}

} // namespace deskflow::gui
