# Build Instructions

## Linux (Flatpak)

We use Flatpak for building and distributing the Linux version. This ensures compatibility across different Linux distributions (Ubuntu, Fedora, Arch, etc.).

### Prerequisites

You need `flatpak-builder` and the KDE SDK/Runtime.

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install flatpak-builder

# Add Flathub repo
flatpak remote-add --if-not-exists --user flathub https://dl.flathub.org/repo/flathub.flatpakrepo

# Install required KDE SDK (version 6.10)
flatpak install --user -y flathub org.kde.Sdk//6.10 org.kde.Platform//6.10
```

### Building

Run these commands from the root of the repository.

1.  **Build the Project**
    This compiles the project using the manifest. It automatically uses all available CPU cores.
    ```bash
    flatpak-builder --user --force-clean --repo=repo build-dir deploy/linux/flatpak/org.lockekk.dshare-hid.yml
    ```

2.  **Create Bundle**
    Packet the build into a single `.flatpak` file.
    *   **App ID**: `org.lockekk.dshare-hid`
    ```bash
    flatpak build-bundle repo dshare-hid-1.25.0-linux-x86_64.flatpak org.lockekk.dshare-hid
    ```

### Installing & Running

1.  **Install**
    ```bash
    flatpak install --user -y dshare-hid-1.25.0-linux-x86_64.flatpak
    ```

2.  **Run**
    ```bash
    flatpak run org.lockekk.dshare-hid
    ```

### Uninstalling & Reinstalling

*   **Uninstall**
    ```bash
    flatpak uninstall --user org.lockekk.dshare-hid
    ```

*   **Reinstall (Update)**
    If you have rebuilt the package and want to update your existing installation:
    ```bash
    flatpak install --user --reinstall -y dshare-hid-1.25.0-linux-x86_64.flatpak
    ```

### Notes
*   **Permissions**: The manifest uses `--device=all` to ensure the application can access USB CDC devices (like `/dev/ttyACM0`). This is required for the bridge client to function.

---

## Linux (AppImage)

AppImage provides a single executable file that runs on most Linux distributions without installation.

### Building

The easiest way to generate an AppImage is using the interactive task runner.

#### Method 1: Using run_task.sh (Recommended)

1.  Run the task runner:
    ```bash
    ./run_task.sh
    ```
2.  Select **Option 6 ("Build AppImage")**.
3.  The AppImage will be generated directly in your `build` directory (e.g., `build/DShare-HID-x86_64.AppImage`).

#### Method 2: Manual Script

If you want more control or are scripting the build, you can use the generation script directly. It can reuse an existing build directory.

```bash
# Usage: ./deploy/linux/create_appimage.sh <BUILD_DIR> <OUTPUT_DIR>
./deploy/linux/create_appimage.sh build release
```

### Troubleshooting & Advanced

*   **Custom Libraries**: If you have `libportal` or `libei` in non-standard locations (e.g., `/opt/custom`), ensure `LD_LIBRARY_PATH` is set before running the build/script. The tool will bundle them automatically.
*   **Custom Qt**: The script automatically detects if you strictly used a custom Qt version (via `CMakeCache.txt`) and bundles the correct libraries, avoiding mismatches with system Qt.
*   **Compatibility**: To ensure the AppImage runs on older Linux distributions (e.g., Ubuntu 22.04, Debian 11), you should build it inside an environment with an older `glibc` (like an Ubuntu 20.04 Docker container). If built on a modern system (e.g., Ubuntu 24.04), it will only run on systems with `glibc` >= 2.38.

---

## Windows

We use the `package` target in CMake to generate an MSI installer, leveraging CPack and the WiX Toolset.

### Prerequisites

*   **Visual Studio 2022** (or newer) with C++ Desktop Development.
*   **Microsoft Visual C++ Redistributable v14.44 or later**
*   **CMake** (usually included with VS).
*   **[WiX Toolset v4](https://wixtoolset.org/)** (required for MSI generation).
    *   Install: `dotnet tool install --global wix`
    *   Extensions: `wix extension add --global WixToolset.Util.wixext`, `wix extension add --global WixToolset.Firewall.wixext`

### Building & Packaging

The recommended way to build and package is using the `run_task.bat` script.

1.  **Open Command Prompt or PowerShell.**
2.  **Run:**
    ```cmd
    run_task.bat 5
    ```
    *   Or run `run_task.bat` and select Option **5**.

This performs the following:
1.  **Configure Release**: Configures the project with Release settings and production keys.
2.  **Build**: Compiles the application.
3.  **Package**: Generates the MSI installer using CPack/WiX.

### Output

The MSI file will be created in the `build` directory (e.g., `build/DShare-HID-1.25.0-win-x64.msi`).

---

## macOS

We use `cmake` combined with `cpack` to create a DMG installer for macOS.

### Prerequisites

*   **Xcode 15+** or **Command Line Tools** installed.
*   **Qt 6.7+** (installed via Homebrew or Qt Installer).
*   **CMake 3.24+**.
*   **Apple Developer ID Application Certificate** (optional, for code signing).

### Building & Packaging

The `run_task.sh` script (Option 5) handles the entire process.

1.  **Build Universal OpenSSL (One-time)**
    Since standard macOS package managers do not provide universal static libraries, we must build OpenSSL from source using a helper script. This ensures the app runs on both Intel and Apple Silicon Macs.

    ```bash
    ./deploy/mac/build_universal_openssl.sh
    ```
    *This creates a `rel_openssl_universal` directory in your project root.*

2.  **Set Signing Identity (Optional)**
    If you have a Developer ID certificate, export its name to sign the application. This is required for distribution outside of your local machine (to avoid Gatekeeper issues).

    ```bash
    export APPLE_CODESIGN_DEV="Developer ID Application: Your Name (TEAMID)"
    ```

    *If not set, the script will use ad-hoc signing (`-codesign=-`), which works for local testing but will require manual un-quarantine on other machines.*

3.  **Run Build Deploy**

    ```bash
    ./run_task.sh 5
    ```

    *   This will create a clean build in `build_deploy`.
    *   It creates a **Universal Binary** (x86_64 + arm64).

4.  **Output**

    The `.dmg` file will be generated in `build_deploy/` (e.g., `DShare-HID-1.25.0-macos-universal.dmg`).

