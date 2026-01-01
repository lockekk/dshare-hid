/*
 * Deskflow-hid -- created by locke.huang@gmail.com
 */

#pragma once

#include <QMap>
#include <QObject>
#include <QString>

namespace deskflow::gui {

/**
 * @brief Information about a detected USB device
 */
struct UsbDeviceInfo
{
  QString devicePath;   ///< Device path (e.g., /dev/ttyACM0)
  QString vendorId;     ///< USB Vendor ID (e.g., "2e8a")
  QString productId;    ///< USB Product ID (e.g., "000a")
  QString serialNumber; ///< Serial number (if available)

  bool operator==(const UsbDeviceInfo &other) const
  {
    return devicePath == other.devicePath && vendorId == other.vendorId && productId == other.productId;
  }
};

/**
 * @brief Abstract base class for USB device monitoring
 *
 * This class provides a platform-independent interface for monitoring
 * USB device plug/unplug events. Platform-specific implementations
 * handle the actual device detection.
 */
class UsbDeviceMonitor : public QObject
{
  Q_OBJECT

public:
  explicit UsbDeviceMonitor(QObject *parent = nullptr);
  virtual ~UsbDeviceMonitor() = default;

  /**
   * @brief Start monitoring for USB device events
   * @return true if monitoring started successfully
   */
  virtual bool startMonitoring() = 0;

  /**
   * @brief Stop monitoring for USB device events
   */
  virtual void stopMonitoring() = 0;

  /**
   * @brief Check if monitoring is currently active
   */
  virtual bool isMonitoring() const = 0;

  /**
   * @brief Enumerate currently connected devices
   * @return List of connected USB devices
   */
  virtual QList<UsbDeviceInfo> enumerateDevices() = 0;

  /**
   * @brief Set vendor ID filter for device detection
   * @param vendorId Vendor ID in hex format (e.g., "2e8a")
   */
  void setVendorIdFilter(const QString &vendorId);

  /**
   * @brief Set product ID filter for device detection
   * @param productId Product ID in hex format (e.g., "000a")
   */
  void setProductIdFilter(const QString &productId);

  /**
   * @brief Get current vendor ID filter
   */
  QString vendorIdFilter() const
  {
    return m_vendorIdFilter;
  }

  /**
   * @brief Get current product ID filter
   */
  QString productIdFilter() const
  {
    return m_productIdFilter;
  }

Q_SIGNALS:
  /**
   * @brief Emitted when a USB device is connected
   * @param device Information about the connected device
   */
  void deviceConnected(const UsbDeviceInfo &device);

  /**
   * @brief Emitted when a USB device is disconnected
   * @param device Information about the disconnected device
   */
  void deviceDisconnected(const UsbDeviceInfo &device);

  /**
   * @brief Emitted when an error occurs
   * @param message Error message
   */
  void errorOccurred(const QString &message);

protected:
  /**
   * @brief Check if a device matches the filtered vendor/product
   */
  bool matchesFilter(const UsbDeviceInfo &device) const;

  /**
   * @brief Process a new snapshot of devices (for polling implementations)
   *
   * Calculates specific added/removed events by comparing with m_devices.
   * Updates m_devices and emits signals accordingly.
   */
  void processNewDeviceSnapshot(const QList<UsbDeviceInfo> &newSnapshot);

  // Tracked devices (Key: Device Path)
  // Protected so subclasses can access/update it directly if needed (e.g. Linux events)
  QMap<QString, UsbDeviceInfo> m_devices;

private:
  QString m_vendorIdFilter;
  QString m_productIdFilter;
};

} // namespace deskflow::gui
