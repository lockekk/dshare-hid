# Deskflow Icon Update Guide

This guide explains how to update the application icon for Deskflow across all platforms (Linux, Windows, macOS) and ensures correct integration with the Linux desktop environment.

## Prerequisites

The icon update script requires Python 3 and the `Pillow` library (for image processing).

```bash
pip install Pillow
```

## 1. Updating the Master Icon

To change the application icon, you need a source image (preferably a high-resolution PNG, removing the background if possible, although the script now handles solid backgrounds gracefully for monochrome icons).

### Using the Script

Run the `update_icons.py` script from the project root:

```bash
./scripts/update_icons.py path/to/your/new_icon.png
```

**What this does:**
1.  **macOS**: Updates `src/apps/res/Deskflow.icns`.
2.  **Windows**: Updates `src/apps/res/deskflow.ico` (multi-size ICO).
3.  **Linux**: Updates `deploy/linux/org.deskflow.deskflow.png` (512x512).
4.  **Internal Resources**: Updates embedded SVG icons used within the application GUI:
    *   `src/apps/res/icons/deskflow-dark/apps/64/org.deskflow.deskflow.svg` (Colorful)
    *   `src/apps/res/icons/deskflow-light/apps/64/org.deskflow.deskflow.svg` (Colorful)
    *   `src/apps/res/icons/.../org.deskflow.deskflow-symbolic.svg` (Monochrome/Grayscale for Tray)

> **Note**: The script automatically generates a high-contrast **Grayscale** version for the "Symbolic" (Monocolor) icon used in the system tray. This ensures visibility even if your source icon is a solid block without transparency.

## 2. Linux Integration (Critical)

On Linux, simply building the application often isn't enough for the Taskbar/Dock to show the correct icon. The Desktop Environment (GNOME, KDE, etc.) needs to find the icon in a standard system path.

We provide a script to handle this automatically used for local development.

### Running the Integration Script

After updating the icon (Step 1), run:

```bash
./scripts/install_linux_integration.sh
```

**What this does:**
1.  **Installs Icons**: Copies the generated icons to `~/.local/share/icons/hicolor/`.
2.  **Updates Cache**: Runs `gtk-update-icon-cache` to ensure the system sees the new files immediately.
3.  **Desktop File**: Creates a definitive `~/.local/share/applications/deskflow-hid.desktop` file.
    *   Sets `StartupWMClass=Deskflow-HID` to correctly associate the running window with the pinned icon.
    *   Sets the `Icon` path explicitly to the installed system icon.

## 3. Applying Changes

After running both scripts:

1.  **Rebuild** the application (required to embed the new internal resources for Windows/macOS and the About dialog):
    ```bash
    cmake --build build -j$(nproc)
    ```
2.  **Restart** the application:
    ```bash
    ./build/bin/deskflow-hid
    ```

## Troubleshooting

*   **Taskbar Icon is Generic/Gear**: Run `./scripts/install_linux_integration.sh` again and restart the app. Ensure you are not running an old binary or a different `.desktop` file (e.g., from a Flatpak or global install) isn't taking precedence.
*   **Tray Icon is Invisible**: If using "Monocolor" style in settings, ensure you ran `update_icons.py`. The script ensures the monochrome icon is grayscale (visible) rather than a solid white block.
*   **Permissions**: If scripts fail to run, ensure they are executable: `chmod +x scripts/*.py scripts/*.sh`.
