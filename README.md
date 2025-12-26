# Deskflow-HID: Professional Cross-Platform HID Bridge

## Introduction
Deskflow-HID is a high-performance, open-source extension of the [Deskflow](https://github.com/deskflow/deskflow) project. Supporting **Linux, Windows, and macOS**, Deskflow-HID focuses on sharing keyboard and mouse inputs with mobile devices including iPad, iPhone, and Android phones.

## Expanding the Deskflow Ecosystem: Mobile Integration
While traditional software KVM solutions like Deskflow rely on a client-server architecture where both devices run the software, Deskflow-HID extends these capabilities to platforms with native restrictions:
- **iOS and Android** do not allow background apps to intercept or simulate system-wide HID (Human Interface Device) events for security reasons.
- **Apple Sidecar/Universal Control** is restricted to the Apple ecosystem, leaving Windows and Linux users behind.
- **Remote Desktop** solutions often suffer from high latency and depend on network stability, which can impact fluid, real-time peripheral sharing.

## The Solution: Hardware-Assisted Bridging
Deskflow-HID solves these challenges by utilizing a physical hardware bridge. By converting Deskflow network events into the native **Bluetooth Low Energy (BLE) HID protocol**, it allows you to share your host's native keyboard and mouse with any mobile device wirelessly.

### Universal Compatibility
Deskflow-HID is fully compatible with the entire Deskflow ecosystem. Whether you are using the official upstream Deskflow clients or mobile devices via the HID bridge, all your machines can share the same keyboard and mouse flawlessly.

## Key Features & Advantages
- **Native Experience**: Your mobile device sees exactly what it expects—a standard Bluetooth peripheral. No apps or drivers are required on the target device.
- **Ultra-Low Latency**: By bypassing the network stack and using direct BLE communication, input is significantly more responsive than remote desktop solutions.
- **Universal Compatibility**: Fully supports **Windows, macOS, and Linux** hosts, and interacts seamlessly with all existing Deskflow ecosystem clients.
- **Multi-Device Pairing**: Securely pair and toggle between up to **6 mobile devices**. Switching is instantaneous and effortless.
- **US Layout & International Flexibility**: Acts as a standard US layout keyboard but supports native iOS/Android "Hardware Keyboard" settings for non-US mappings.
- **Consumer Key Support**: Dedicated control for media keys including Play/Pause, Volume, and Mute.
- **"Universal Control" for Everyone**: Brings the seamless, Apple-style ecosystem experience to **Windows and Linux** users, allowing you to control your iPad or Android tablet exactly like Apple’s Universal Control feature—but without being locked into their hardware.
- **Native Display & Productivity**: Use your tablet as a high-quality secondary screen with its native resolution and no compression artifacts.
- **Hardware Stability**: Standardized on **ESP32-C3** hardware for superior RF performance.

> [!NOTE]
> **Clipboard Sharing**: Deskflow's network clipboard sharing is not supported at the moment but will be supported in a future update.

## Getting Started
### 1. Hardware Required
To get started, you will need an **ESP32-C3 Mini** board. These are affordable, widely available, and offer the necessary BLE capabilities.

### 2. Physical Setup
Simply connect the ESP32-C3 board to your host PC via a standard **USB port**. The board acts as the bridge between your desktop's Deskflow server and your mobile devices.

### 3. Firmware
Flash our custom firmware onto the ESP32-C3. Once flashed, the Deskflow-HID application will automatically detect the bridge and you'll be ready to control your mobile devices.

### 4. Building from Source
The development environment and build process for Deskflow-HID are **100% identical to the upstream [Deskflow project](https://github.com/deskflow/deskflow)**. You can follow the official build instructions for Windows, macOS, and Linux without any additional environment setup.

## Open Source & Commercial Terms
Deskflow-HID is an open-source project at its core. The desktop application and bridge architecture are open for contribution and community improvement.

- **7-Day Full Trial**: Experience the full functionality of the firmware and software for 7 days at no cost.
- **Lifetime Value**: After the trial, a one-time activation provides **lifetime free upgrades via OTA (Over-The-Air)** and **lifetime maintenance**.

## Acknowledgments
We would like to express our deepest gratitude to the [Deskflow](https://github.com/deskflow/deskflow) project and its community. Deskflow-HID is built upon their incredible foundation, and we are proud to extend its capabilities. Special thanks to the core maintainers and contributors for their years of dedication to open-source cross-platform productivity.

## Support & Contact
- **Maintainer**: Locke Huang
- **Email**: [deskflow.hid@gmail.com](mailto:deskflow.hid@gmail.com)

## Disclaimer & Legal
While Deskflow-HID is built upon an open-source foundation, it is important to note:
- **Optional Components**: The project may include optional submodules or links to proprietary repositories for specific features (e.g., firmware management).
- **Independence**: The core open-source desktop application is fully functional and can be built independently of any proprietary submodules or firmware components. Deskflow-HID remains a true open-source project at its heart.

## License
Deskflow-HID is licensed under the **GNU General Public License v2.0 (GPL-2.0-only)**. This project is a derivative work based on [Deskflow](https://github.com/deskflow/deskflow).

For more details, see the [LICENSE](LICENSE) file in this repository.

Join the future of cross-platform productivity with Deskflow-HID.
