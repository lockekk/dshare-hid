# DShare-HID: Share Keyboard and Mouse with Mobile Devices

![License](https://img.shields.io/github/license/lockekk/dshare-hid?style=flat-square) ![Release](https://img.shields.io/github/v/tag/lockekk/dshare-hid?style=flat-square)

**[ English ]** | [ 简体中文 ](README_zh-CN.md)

---



## Introduction

DShare-HID is a high-performance, open-source project derived from [Deskflow](https://github.com/deskflow/deskflow). While sharing the core architecture, DShare-HID is an independent fork focused on sharing keyboard and mouse inputs with mobile devices including iPad, iPhone, and Android phones.


## Expanding the Deskflow Ecosystem: Mobile Integration

While traditional software KVM solutions like Deskflow work great between computers, they cannot support mobile platforms like iPadOS or Android. DShare-HID bridges this gap by extending Deskflow's capabilities to these devices:
- **iOS and Android** do not allow background apps to intercept or simulate system-wide HID (Human Interface Device) events for security reasons.
- **Apple Sidecar/Universal Control** is restricted to the Apple ecosystem, leaving Windows and Linux users behind.
- **Remote Desktop** solutions often suffer from high latency and depend on network stability, which can impact fluid, real-time peripheral sharing.

## The Solution: Hardware Bridge Client

DShare-HID uses a budget-friendly (~$2.50 USD on AliExpress) **ESP32-C3 Supermini** board as a hardware bridge. It converts Deskflow events into **Bluetooth Low Energy (BLE) HID**, letting you wirelessly share your keyboard and mouse with any mobile device.

<br/> <img src="doc/images/esp32-c3-supermini.png" height="120" alt="ESP32-C3 Super Mini"> <br/> <sub>Image credit: [Josselin Hefti](https://www.printables.com/model/1360390-esp32-c3-super-mini-model)</sub>



## 📸 Screenshots

|                            Main UI                            |                               Device Configuration                               |
| :-----------------------------------------------------------: | :------------------------------------------------------------------------------: |
| <img src="doc/images/main_ui.png" height="300" alt="Main UI"> | <img src="doc/images/device_config.png" height="300" alt="Device Configuration"> |


## Key Features

- **Native Experience**: No apps or drivers are required on the target device. It sees a standard Bluetooth peripheral.
- **Ultra-Low Latency**: Offers superior responsiveness compared to remote screen sharing solutions.
- **Universal Compatibility**: Fully supports **Windows, macOS, and Linux** hosts. Compatible with official upstream Deskflow clients.
- **Multi-Device Pairing**: Securely pair and toggle between up to **6 mobile devices**. Switching is instantaneous and effortless.
- **US Layout & International Flexibility**: Acts as a standard US layout keyboard but supports native iOS/Android "Hardware Keyboard" settings for non-US mappings.
- **Consumer Key Support**: Dedicated control for media keys, including Play/Pause and Volume.
- **Native External Display**: Use your tablet as a high-quality secondary screen.


> [!NOTE]
> **Clipboard Sharing**: Deskflow's network clipboard sharing is not supported at the moment but will be supported in a future update.

## 📥 Installation

We provide pre-built binaries for all major platforms. Choose the one that fits your environment.

### macOS (Universal)
Suitable for macOS 12+ (Intel & Apple Silicon).

#### Option A: Homebrew (Disabled - Coming Soon)
This option is currently disabled and will be supported in a future update.

<!--
```bash
# 1. Add the Tap
brew tap lockekk/dshare-hid

# 2. Install
brew install --cask dshare-hid

# To Uninstall
brew uninstall dshare-hid
# To Update
brew upgrade dshare-hid
```
-->

#### Option B: Manual Install
1.  Download the latest `.dmg` from the [Releases](https://github.com/lockekk/dshare-hid/releases) page.
2.  Open `dshare-hid-1.25.0-macos-universal.dmg` and drag the application to your `Applications` folder.
3.  **Note**: If you encounter a "Damaged" or "Unverified" error, run the following command in Terminal:
    ```bash
    xattr -cr /Applications/DShare-HID.app
    ```

> [!IMPORTANT]
> **Permissions & Setup**:
> - **Accessibility Access**: You must grant "Accessibility" access (Privacy & Security) to both the **DShare-HID** app and the **dshare-hid** process.
> - **macOS Sequoia**: You may also need to allow DShare-HID under "Local Network" settings.
> - **Upgrading**: If you are upgrading and already have DShare-HID on the allowed list, you may need to **manually remove** the old entries before accessibility access can be granted to the new version.

### Windows
**Dependency**: Please ensure you have installed **Microsoft Visual C++ Redistributable v14.44 or later** before use.

Available as a portable archive (Recommended) or installer.

-   **Portable (.7z)**: **Recommended**. Extract and run `dshare-hid.exe`.
-   **Installer (.msi)**: Download and double-click to install.

### Linux
We support major distributions via AppImage and Flatpak.

> [!IMPORTANT]
> **Permissions**: To access the USB device, your user **must** have permission to access serial ports (usually the `dialout` group).
> Run the following command and then **logout and login** (or **reboot**) for the change to take effect:
> ```bash
> sudo usermod -a -G dialout $USER
> ```

#### Option A: AppImage (Universal)
Works on newer Linux distributions (Ubuntu 22.04+, Fedora 36+, etc.).

1.  Download the `.AppImage` file from [Releases](https://github.com/lockekk/dshare-hid/releases).
2.  Make it executable:
    ```bash
    chmod +x dshare-hid-1.25.0-linux-x86_64.AppImage
    ```
3.  Run it:
    ```bash
    ./dshare-hid-1.25.0-linux-x86_64.AppImage
    ```

#### Option B: Flatpak
Please download the `.flatpak` file from our [Releases](https://github.com/lockekk/dshare-hid/releases) page and install it locally.

**1. Setup Flatpak**
If you haven't used Flatpak before, ensure it is installed and the Flathub repository is added:
```bash
# Debian/Ubuntu
sudo apt install flatpak

# Fedora
sudo dnf install flatpak

# Arch Linux
sudo pacman -S flatpak

# Add Flathub Repo (All Distros)
flatpak remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
```

**2. Install**
```bash
flatpak install --user ./dshare-hid-1.25.0-linux-x86_64.flatpak
```

# Manage
```bash
# Uninstall
flatpak uninstall org.lockekk.dshare-hid

# Reinstall (Remove then Install)
flatpak uninstall org.lockekk.dshare-hid
flatpak install --user ./dshare-hid-1.25.0-linux-x86_64.flatpak
```

#### Option C: Debian Package (Ubuntu 24+)
For Ubuntu 24.04 and newer.

1.  Download the `.deb` file from [Releases](https://github.com/lockekk/dshare-hid/releases).
2.  Install:
    ```bash
    sudo apt install ./dshare-hid_1.25.0_ubuntu_24.04_amd64.deb
    ```
3.  Uninstall:
    ```bash
    sudo apt remove dshare-hid
    ```

## Contribute

[![Good first issues](https://img.shields.io/github/issues/lockekk/dshare-hid/good%20first%20issue?label=good%20first%20issues&color=%2344cc11)](https://github.com/lockekk/dshare-hid/labels/good%20first%20issue)

There are many ways to contribute to the DShare-HID project.

We're a friendly, active, and welcoming community focused on building a great app.

Read our [Wiki](https://github.com/lockekk/dshare-hid/wiki) to get started.

For instructions on building DShare-HID, use the wiki page: [Building](https://github.com/lockekk/dshare-hid/wiki/Building)

## Operating Systems

We support all major operating systems, including Windows, macOS, Linux, and Unix-like BSD-derived.

Windows 10 v1809 or higher is required.

macOS 13 or higher is required to use our CI builds for Apple Silicon machines. macOS 12 or higher is required for Intel macs or local builds.

Linux requires libei 1.3+ and libportal 0.8+ for the server/client. Additionally, Qt 6.7+ is required for the GUI.
Linux users with systems not meeting these requirements should use flatpak in place of a native package.

We officially support FreeBSD, and would also like to support: OpenBSD, NetBSD, DragonFly, Solaris.

## Repology

Repology monitors a huge number of package repositories and other sources comparing package
versions across them and gathering other information.

[![Repology](https://repology.org/badge/vertical-allrepos/dshare-hid.svg?exclude_unsupported=1)](https://repology.org/project/dshare-hid/versions)

# To Update
brew upgrade dshare-hid
```
-->

#### Option B: Manual Install
1.  Download the latest `.dmg` from the [Releases](https://github.com/lockekk/dshare-hid/releases) page.
2.  Open `dshare-hid-1.25.0-macos-universal.dmg` and drag the application to your `Applications` folder.
3.  **Note**: If you encounter a "Damaged" or "Unverified" error, run the following command in Terminal:
    ```bash
    xattr -cr /Applications/DShare-HID.app
    ```

> [!IMPORTANT]
> **Permissions & Setup**:
> - **Accessibility Access**: You must grant "Accessibility" access (Privacy & Security) to both the **DShare-HID** app and the **dshare-hid** process.
> - **macOS Sequoia**: You may also need to allow DShare-HID under "Local Network" settings.
> - **Upgrading**: If you are upgrading and already have DShare-HID on the allowed list, you may need to **manually remove** the old entries before accessibility access can be granted to the new version.

### Windows
**Dependency**: Please ensure you have installed **Microsoft Visual C++ Redistributable v14.44 or later** before use.

Available as a portable archive (Recommended) or installer.

-   **Portable (.7z)**: **Recommended**. Extract and run `dshare-hid.exe`.
-   **Installer (.msi)**: Download and double-click to install.

### Linux
We support major distributions via AppImage and Flatpak.

> [!IMPORTANT]
> **Permissions**: To access the USB device, your user **must** have permission to access serial ports (usually the `dialout` group).
> Run the following command and then **logout and login** (or **reboot**) for the change to take effect:
> ```bash
> sudo usermod -a -G dialout $USER
> ```

#### Option A: AppImage (Universal)
Works on newer Linux distributions (Ubuntu 22.04+, Fedora 36+, etc.).

1.  Download the `.AppImage` file from [Releases](https://github.com/lockekk/dshare-hid/releases).
2.  Make it executable:
    ```bash
    chmod +x dshare-hid-1.25.0-linux-x86_64.AppImage
    ```
3.  Run it:
    ```bash
    ./dshare-hid-1.25.0-linux-x86_64.AppImage
    ```

#### Option B: Flatpak
Please download the `.flatpak` file from our [Releases](https://github.com/lockekk/dshare-hid/releases) page and install it locally.

**1. Setup Flatpak**
If you haven't used Flatpak before, ensure it is installed and the Flathub repository is added:
```bash
# Debian/Ubuntu
sudo apt install flatpak

# Fedora
sudo dnf install flatpak

# Arch Linux
sudo pacman -S flatpak

# Add Flathub Repo (All Distros)
flatpak remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
```

**2. Install**
```bash
flatpak install --user ./dshare-hid-1.25.0-linux-x86_64.flatpak
```

# Manage
```bash
# Uninstall
flatpak uninstall org.lockekk.dshare-hid

# Reinstall (Remove then Install)
flatpak uninstall org.lockekk.dshare-hid
flatpak install --user ./dshare-hid-1.25.0-linux-x86_64.flatpak
```

#### Option C: Debian Package (Ubuntu 24+)
For Ubuntu 24.04 and newer.

1.  Download the `.deb` file from [Releases](https://github.com/lockekk/dshare-hid/releases).
2.  Install:
    ```bash
    sudo apt install ./dshare-hid_1.25.0_ubuntu_24.04_amd64.deb
    ```
3.  Uninstall:
    ```bash
    sudo apt remove dshare-hid
    ```

## First Use & Flashing Guide

<div align="center">
  <img src="doc/images/firmware_update.png" width="600" alt="Firmware Flash Tool">
</div>

### 1. Prepare Hardware
You will need an **ESP32-C3 Supermini** board. These are cheap and widely available from AliExpress or local electronics stores. Connect it to your computer via USB.

### 2. Access Flash Tool
Open DShare-HID and navigate to **File -> Firmware** to open the management interface.

### 3. Setup (For New Devices)
For a brand new "virgin" device, follow this sequence:

1.  **Factory Mode (Online)**:
    *   Go to the **Factory Mode** tab.
    *   Click **Flash** under the "Online" section to install the generic factory firmware.
    *   This prepares the device for pairing and licensing.

2.  **Request per-device application**:
    *   Go to the **Order** tab.
    *   Choose **Request 7-Day Free Trial** or **Purchase Full License**.
    *   Fill in your details and click **Email** to send the request.
    *   You will receive a specific **per-device firmware file** (e.g., `app_xxxx.uzip`) via email.

3.  **Install Application (Manual)**:
    *   Go to the **Upgrade Mode** tab.
    *   Under "Manual", browse and select the file you received.
    *   Click **Flash** to install your licensed firmware.

### 4. Updates & Maintenance
*   **Upgrading**: Go to the **Upgrade Mode** tab and click **Check for Updates** -> **Flash** to install the latest features via OTA.
*   **Activation**: Go to the **Activation** tab to view your license status or input a new activation key.

## Building from Source
The development environment and build process for DShare-HID are **identical to the upstream [Deskflow project](https://github.com/deskflow/deskflow)**. You can follow the official build instructions for Windows, macOS, and Linux.

**Note**: The firmware flashing and authentication modules are optional components and not part of the core open-source project. Users are welcome to build and use their own custom firmware implementation.

## Open Source & Commercial Terms
DShare-HID is an open-source project at its core. The desktop application and bridge architecture are open for contribution and community improvement.

- **7-Day Full Trial**: Experience the full functionality of the firmware and software for 7 days at no cost.
- **Lifetime Value**: After the trial, a one-time activation provides **lifetime free upgrades via OTA** and **lifetime maintenance**.

## Acknowledgments
Special thanks to the [Deskflow](https://github.com/deskflow/deskflow) project and its contributors. This project is built upon their strong foundation.

We also thank [Josselin Hefti](https://www.printables.com/model/1360390-esp32-c3-super-mini-model) for the excellent 3D model image of the ESP32-C3 Supermini.

## Support & Contact
- **Email**: [dshare.hid@gmail.com](mailto:dshare.hid@gmail.com)
- **Issues**: Please report bugs via [GitHub Issues](https://github.com/lockekk/dshare-hid/issues).

## Disclaimer & Legal
DShare-HID is an open-source project. However, some optional features (like firmware management) may rely on proprietary components. The core application remains fully functional and open-source without these components.

## License
DShare-HID is licensed under the **GNU General Public License v2.0 (GPL-2.0-only)**. This project is a derivative work based on [Deskflow](https://github.com/deskflow/deskflow).

