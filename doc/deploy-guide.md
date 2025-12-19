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
    flatpak-builder --user --force-clean --repo=repo build-dir deploy/linux/flatpak/org.deskflow.deskflow.yml
    ```

2.  **Create Bundle**
    Package the build into a single `.flatpak` file.
    *   **App ID**: `org.fs34a.deskflow-hid`
    ```bash
    flatpak build-bundle repo deskflow-1.25.0-linux-x86_64.flatpak org.fs34a.deskflow-hid
    ```

### Installing & Running

1.  **Install**
    ```bash
    flatpak install --user -y deskflow-1.25.0-linux-x86_64.flatpak
    ```

2.  **Run**
    ```bash
    flatpak run org.fs34a.deskflow-hid
    ```

### Uninstalling & Reinstalling

*   **Uninstall**
    ```bash
    flatpak uninstall --user org.fs34a.deskflow-hid
    ```

*   **Reinstall (Update)**
    If you have rebuilt the package and want to update your existing installation:
    ```bash
    flatpak install --user --reinstall -y deskflow-1.25.0-linux-x86_64.flatpak
    ```

### Notes
*   **Permissions**: The manifest uses `--device=all` to ensure the application can access USB CDC devices (like `/dev/ttyACM0`). This is required for the bridge client to function.

---

## Windows

*Documentation coming soon.*

---

## macOS

*Documentation coming soon.*
