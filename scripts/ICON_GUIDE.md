# DShare Icon Update Guide

This guide explains how to update the application icon for DShare across all platforms (Linux, Windows, macOS) and ensures correct integration with the Linux desktop environment.

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
1.  **macOS**: Updates `src/apps/res/DShare-HID.icns` (only when you pass a new source image).
2.  **Windows**: Updates `src/apps/res/dshare-hid.ico` (multi-size ICO).
3.  **Linux**: Updates `deploy/linux/io.github.lockekk.dshare-hid.png` (512x512).
4.  **Windows**: Generates `deploy/windows/wix-dialog.png` & `wix-banner.png`.
5.  **macOS**: Syncs `deploy/mac/dmg-volume.icns` and resets `deploy/mac/dmg-background.tiff` to plain white.
6.  **Internal Resources**: Updates embedded SVG icons used within the application GUI:
    *   `src/apps/res/icons/dshare-hid-dark/apps/64/io.github.lockekk.dshare-hid.svg` (Colorful)
    *   `src/apps/res/icons/dshare-hid-light/apps/64/io.github.lockekk.dshare-hid.svg` (Colorful)
    *   `src/apps/res/icons/dshare-hid-{dark,light}/apps/64/io.github.lockekk.dshare-hid-symbolic.svg` (Monochrome/Grayscale for Tray)

The script only writes to files that already exist. If one is missing (for example after a rename), it stops and lists the paths to fix at the top of the script.

> **Note**: The script automatically generates a high-contrast **Grayscale** version for the "Symbolic" (Monocolor) icon used in the system tray. This ensures visibility even if your source icon is a solid block without transparency.

## 2. Linux Integration

The `.deb` package installs the desktop file and icons under the app ID `io.github.lockekk.dshare-hid` (see `deploy/linux/create_deb.sh`).

The old `scripts/install_linux_integration.sh` was removed when the app was renamed to DShare-HID. If you ran it before then, it left icons named `org.deskflow.deskflow*` in `~/.local/share/icons/hicolor/` and a `~/.local/share/applications/deskflow-hid.desktop` launcher. `~/.local/share` takes priority over system and Flatpak icons, so those leftovers replace upstream Deskflow's own icon. Delete them:

```bash
rm -f ~/.local/share/icons/hicolor/*/apps/org.deskflow.deskflow*.{png,svg} \
      ~/.local/share/applications/deskflow-hid.desktop
```

Never install icons under another app's name.

## 3. Applying Changes

After running the script:

1.  **Rebuild** the application (required to embed the new internal resources for Windows/macOS and the About dialog):
    ```bash
    cmake --build build -j$(nproc)
    ```
2.  **Restart** the application:
    ```bash
    ./build/bin/dshare-hid
    ```

## Troubleshooting

*   **Taskbar Icon is Generic/Gear**: GNOME matches the window to a desktop file named `io.github.lockekk.dshare-hid.desktop`. Install the `.deb`, or make sure an older `.desktop` file isn't taking precedence.
*   **Upstream Deskflow shows the DShare-HID icon**: Remove the leftover files listed in section 2, then log out and back in.
*   **Tray Icon is Invisible**: If using "Monocolor" style in settings, ensure you ran `update_icons.py`. The script ensures the monochrome icon is grayscale (visible) rather than a solid white block.
*   **Permissions**: If scripts fail to run, ensure they are executable: `chmod +x scripts/*.py scripts/*.sh`.

