#!/bin/bash
set -e

# Define paths
ICON_SRC_PNG="deploy/linux/org.deskflow.deskflow.png"
ICON_SRC_SVG="src/apps/res/icons/deskflow-light/apps/64/org.deskflow.deskflow.svg"
ICON_SRC_SYMB="src/apps/res/icons/deskflow-light/apps/64/org.deskflow.deskflow-symbolic.svg"

DEST_ICON_DIR="$HOME/.local/share/icons/hicolor"
DEST_APP_DIR="$HOME/.local/share/applications"

echo "Creating directories..."
mkdir -p "$DEST_ICON_DIR/512x512/apps"
mkdir -p "$DEST_ICON_DIR/scalable/apps"
mkdir -p "$DEST_ICON_DIR/symbolic/apps"
mkdir -p "$DEST_APP_DIR"

echo "Installing icons..."
cp "$ICON_SRC_PNG" "$DEST_ICON_DIR/512x512/apps/org.deskflow.deskflow.png"
cp "$ICON_SRC_SVG" "$DEST_ICON_DIR/scalable/apps/org.deskflow.deskflow.svg"
# Install symbolic icon to both standard locations for safety
cp "$ICON_SRC_SYMB" "$DEST_ICON_DIR/symbolic/apps/org.deskflow.deskflow-symbolic.svg"
# DO NOT copy symbolic to scalable/apps as it confuses the DE to prefer the white icon
# cp "$ICON_SRC_SYMB" "$DEST_ICON_DIR/scalable/apps/org.deskflow.deskflow-symbolic.svg"

# Ensure index.theme exists for caching
if [ ! -f "$DEST_ICON_DIR/index.theme" ]; then
    echo "Creating index.theme..."
    # Copy from system if available, else create basic one
    if [ -f "/usr/share/icons/hicolor/index.theme" ]; then
        cp "/usr/share/icons/hicolor/index.theme" "$DEST_ICON_DIR/"
    else
        echo -e "[Icon Theme]\nName=Hicolor\nComment=Default Theme\nDirectories=512x512/apps,scalable/apps,symbolic/apps" > "$DEST_ICON_DIR/index.theme"
    fi
fi

echo "Updating icon cache..."
gtk-update-icon-cache -f -t "$DEST_ICON_DIR" || echo "Warning: gtk-update-icon-cache failed"

echo "Creating .desktop entries..."

# Variables for desktop file
ICON_PATH="$DEST_ICON_DIR/512x512/apps/org.deskflow.deskflow.png"
EXEC_NAME="deskflow-hid"

# 2. Create Single Definitive Desktop Entry
# We use the binary name "deskflow-hid" as the filename to match the process.
# We use StartupWMClass=Deskflow-HID to match the observed X11 window class.
cat > "$DEST_APP_DIR/deskflow-hid.desktop" <<EOF
[Desktop Entry]
Type=Application
Version=1.0
Name=Deskflow
Comment=Mouse and keyboard sharing utility
Exec=$EXEC_NAME
Icon=$ICON_PATH
Terminal=false
Categories=Utility;
Keywords=keyboard;mouse;sharing;network;share;
StartupWMClass=Deskflow-HID
StartupNotify=true
EOF

# Remove legacy/alias files to prevent confusion
rm -f "$DEST_APP_DIR/org.deskflow.deskflow.desktop"
rm -f "$DEST_APP_DIR/Deskflow.desktop"
rm -f "$DEST_APP_DIR/Deskflow-HID.desktop"

# 3. Clean up potential bad symbolic icon placement from previous run
rm -f "$DEST_ICON_DIR/scalable/apps/org.deskflow.deskflow-symbolic.svg"

# Install symbolic icon correctly for Tray usage
mkdir -p "$DEST_ICON_DIR/symbolic/apps"
cp "$ICON_SRC_SYMB" "$DEST_ICON_DIR/symbolic/apps/org.deskflow.deskflow-symbolic.svg"

echo "Updating desktop database..."


echo "Updating desktop database..."
update-desktop-database "$DEST_APP_DIR" || echo "Warning: update-desktop-database failed"

echo "Done. Please restart the application."
