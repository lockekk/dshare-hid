# False Disconnects During WM_DEVICECHANGE

The Windows USB monitor still treats any device-change broadcast as a potential
disconnect. When a non-bridge USB device triggers `WM_DEVICECHANGE` while an
ESP32 bridge is already in use, `UsbDeviceHelper::readSerialNumber()` fails to
reopen the busy COM port and the monitor assumes the bridge disappeared. That
causes `MainWindow::usbDeviceDisconnected()` to tear down the running bridge
client even though the hardware is still attached.

**Next steps**

1. Track bridge devices by a stable identifier (e.g., device interface path or
   SetupAPI devnode ID) instead of relying on reopening COM ports.
2. When enumerating, treat “access denied/busy” as “device still present” so the
   cached entry is not removed.
3. Consider subscribing only to GUID_DEVINTERFACE_USB_DEVICE events to avoid
   noise from unrelated device classes.
