#!/bin/bash
set -e

# Configuration
BUILD_DIR="${1:-build}"
OUTPUT_DIR="${2:-release}"

# Ensure absolute paths
BUILD_DIR="$(realpath "$BUILD_DIR")"
mkdir -p "$OUTPUT_DIR"
OUTPUT_DIR="$(realpath "$OUTPUT_DIR")"

# Work relative to the script location
SCRIPT_DIR="$(dirname "$(realpath "$0")")"
PROJECT_ROOT="$(realpath "$SCRIPT_DIR/../..")"

# Staging Area (AppDir-like structure for linuxdeploy)
STAGING_DIR="$OUTPUT_DIR/deb_staging"
# Final Deb Root (structure for dpkg-deb)
DEB_ROOT="$OUTPUT_DIR/deb_root"
INSTALL_PREFIX="/opt/deskflow-hid"

echo "Project Root:     $PROJECT_ROOT"
echo "Build Dir:        $BUILD_DIR"
echo "Output Dir:       $OUTPUT_DIR"
echo "Install Prefix:   $INSTALL_PREFIX"

# Download linuxdeploy tools if not present
TOOLS_DIR="$PROJECT_ROOT/tools"
mkdir -p "$TOOLS_DIR"

LINUXDEPLOY="$TOOLS_DIR/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_PLUGIN_QT="$TOOLS_DIR/linuxdeploy-plugin-qt-x86_64.AppImage"

if [ ! -f "$LINUXDEPLOY" ]; then
    echo "Downloading linuxdeploy..."
    wget -O "$LINUXDEPLOY" https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
    chmod +x "$LINUXDEPLOY"
fi

if [ ! -f "$LINUXDEPLOY_PLUGIN_QT" ]; then
    echo "Downloading linuxdeploy-plugin-qt..."
    wget -O "$LINUXDEPLOY_PLUGIN_QT" https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
    chmod +x "$LINUXDEPLOY_PLUGIN_QT"
fi

# Clean up previous run
rm -rf "$STAGING_DIR" "$DEB_ROOT"
mkdir -p "$STAGING_DIR"
mkdir -p "$DEB_ROOT$INSTALL_PREFIX"

# 1. Install into Staging Dir (as if it were /usr)
echo "Installing into Staging Dir..."
DESTDIR="$STAGING_DIR" cmake --install "$BUILD_DIR" --prefix "/usr"

# 2. Find resources
DESKTOP_FILE=$(find "$STAGING_DIR" -name "*.desktop" | head -n 1)
EXECUTABLE="$STAGING_DIR/usr/bin/deskflow-hid"
ICON_FILE="$STAGING_DIR/usr/share/icons/hicolor/512x512/apps/org.deskflow.deskflow.png"

if [ ! -f "$EXECUTABLE" ]; then
    echo "Error: Could not find executable at $EXECUTABLE"
    exit 1
fi

# 3. Use linuxdeploy to bundle dependencies into the staging dir
echo "Running linuxdeploy to bundle dependencies..."
export LINUXDEPLOY_PLUGIN_QT_PATH="$LINUXDEPLOY_PLUGIN_QT"

# Allow user to override QMAKE
if [ -z "$QMAKE" ]; then
    # Auto-detect custom Qt from CMakeCache OR use system qmake
    CACHE_FILE="$BUILD_DIR/CMakeCache.txt"
    QT_PLUGINS_DIR=""

    if [ -f "$CACHE_FILE" ]; then
        QT_DIR_LINE=$(grep "Qt6_DIR:PATH=" "$CACHE_FILE" | head -n 1)
        if [ ! -z "$QT_DIR_LINE" ]; then
            QT_CMAKE_PATH="${QT_DIR_LINE#*=}"
            echo "CMake used Qt from: $QT_CMAKE_PATH"

            # Try to find qmake relative to CMake path first (custom install)
            # Layout 1 (Custom): .../gcc_64/lib/cmake/Qt6 -> .../gcc_64 is root -> bin/qmake
            # Layout 2 (System): .../usr/lib/x86_64-linux-gnu/cmake/Qt6 -> .../usr/lib is root -> ../bin/qmake (usr/bin)

            QT_ROOT=$(dirname "$(dirname "$(dirname "$QT_CMAKE_PATH")")")

            if [ -x "$QT_ROOT/bin/qmake" ]; then
                 export QMAKE="$QT_ROOT/bin/qmake"
            elif [ -x "$QT_ROOT/bin/qmake6" ]; then
                 export QMAKE="$QT_ROOT/bin/qmake6"
            elif [ -x "$QT_ROOT/../bin/qmake" ]; then
                 export QMAKE=$(realpath "$QT_ROOT/../bin/qmake")
            elif [ -x "$QT_ROOT/../bin/qmake6" ]; then
                 export QMAKE=$(realpath "$QT_ROOT/../bin/qmake6")
            else
                 # Fallback to system qmake if not found in derived path
                 echo "Warning: Could not derive qmake from CMake path. Falling back to system discovery."
                 if command -v qmake6 &> /dev/null; then
                     export QMAKE="qmake6"
                 elif command -v qmake &> /dev/null; then
                     export QMAKE="qmake"
                 fi
            fi
        fi
    else
        # Fallback if no CMakeCache
         if command -v qmake6 &> /dev/null; then
             export QMAKE="qmake6"
         elif command -v qmake &> /dev/null; then
             export QMAKE="qmake"
         fi
    fi
fi

if [ ! -z "$QMAKE" ]; then
    echo "Using QMake: $QMAKE"
    QT_PLUGINS_DIR=$($QMAKE -query QT_INSTALL_PLUGINS)
    echo "Detected Qt Plugins Dir: $QT_PLUGINS_DIR"

    # Export for linuxdeploy plugin
    QT_BIN_DIR=$($QMAKE -query QT_INSTALL_BINS)
    export PATH="$QT_BIN_DIR:$PATH"

    # Export for linuxdeploy core (dependency resolution)
    QT_LIB_DIR=$($QMAKE -query QT_INSTALL_LIBS)
    echo "Detected Qt Lib Dir: $QT_LIB_DIR"
    export LD_LIBRARY_PATH="$QT_LIB_DIR:$LD_LIBRARY_PATH"
fi
# If QMAKE is set, find plugins dir to manually copy extra plugins if needed
if [ ! -z "$QMAKE" ]; then
     QT_PLUGINS_DIR=$($QMAKE -query QT_INSTALL_PLUGINS)
     if [ -d "$QT_PLUGINS_DIR/imageformats" ]; then
        mkdir -p "$STAGING_DIR/usr/plugins/imageformats"
        # Copy critical SVG/Icon libs
        cp "$QT_PLUGINS_DIR/imageformats/libqsvg.so" "$STAGING_DIR/usr/plugins/imageformats/" 2>/dev/null || true
        cp "$QT_PLUGINS_DIR/imageformats/libqico.so" "$STAGING_DIR/usr/plugins/imageformats/" 2>/dev/null || true
     fi
     if [ -d "$QT_PLUGINS_DIR/iconengines" ]; then
        mkdir -p "$STAGING_DIR/usr/plugins/iconengines"
        cp "$QT_PLUGINS_DIR/iconengines/libqsvgicon.so" "$STAGING_DIR/usr/plugins/iconengines/" 2>/dev/null || true
     fi
fi


# Run bundling
# We use --appdir to point to staging. linuxdeploy defaults to putting things in usr/
"$LINUXDEPLOY" --appdir "$STAGING_DIR" \
    --plugin qt \
    --executable "$EXECUTABLE" \
    --icon-file "$ICON_FILE" \
    --desktop-file "$DESKTOP_FILE"

# 4. Move Bundled Content to Deb Root structure
# Staging: usr/bin, usr/lib, usr/plugins, usr/share
# Target: /opt/deskflow-hid/bin, /opt/deskflow-hid/lib, ...

echo "Migrating to Deb structure ($INSTALL_PREFIX)..."
cp -r "$STAGING_DIR/usr/"* "$DEB_ROOT$INSTALL_PREFIX/"

# Create Wrapper Script for "Static-like" behavior (forcing bundled libs)
mv "$DEB_ROOT$INSTALL_PREFIX/bin/deskflow-hid" "$DEB_ROOT$INSTALL_PREFIX/bin/deskflow-hid.bin"
cat > "$DEB_ROOT$INSTALL_PREFIX/bin/deskflow-hid" <<EOF
#!/bin/sh
HERE="\$(dirname "\$(readlink -f "\${0}")")"
export LD_LIBRARY_PATH="\${HERE}/../lib:\${LD_LIBRARY_PATH}"
exec "\${HERE}/deskflow-hid.bin" "\$@"
EOF
chmod +x "$DEB_ROOT$INSTALL_PREFIX/bin/deskflow-hid"

# 5. Create Control File
VERSION=$(grep "set(DESKFLOW_VERSION_MAJOR" "$PROJECT_ROOT/CMakeLists.txt" | head -n1 | awk '{print $2}' | tr -d ')')
VERSION+="."$(grep "set(DESKFLOW_VERSION_MINOR" "$PROJECT_ROOT/CMakeLists.txt" | head -n1 | awk '{print $2}' | tr -d ')')
VERSION+="."$(grep "set(DESKFLOW_VERSION_PATCH" "$PROJECT_ROOT/CMakeLists.txt" | head -n1 | awk '{print $2}' | tr -d ')')
ARCH=$(dpkg --print-architecture)

mkdir -p "$DEB_ROOT/DEBIAN"
cat > "$DEB_ROOT/DEBIAN/control" <<EOF
Package: deskflow-hid
Version: $VERSION
Section: utils
Priority: optional
Architecture: $ARCH
Maintainer: deskflow-hid <deskflow.hid@gmail.com>
Description: Deskflow HID Client
 Deskflow HID Client for Linux.
 Bundled with dependencies for stability.
EOF

# 6. Create postinst to handle desktop integration (symlinking)
cat > "$DEB_ROOT/DEBIAN/postinst" <<EOF
#!/bin/bash
ln -sf $INSTALL_PREFIX/bin/deskflow-hid /usr/bin/deskflow-hid
ln -sf $INSTALL_PREFIX/share/applications/org.deskflow.deskflow.desktop /usr/share/applications/org.deskflow.deskflow.desktop

# Symlink Icon
mkdir -p /usr/share/icons/hicolor/512x512/apps
ln -sf $INSTALL_PREFIX/share/icons/hicolor/512x512/apps/org.deskflow.deskflow.png /usr/share/icons/hicolor/512x512/apps/org.deskflow.deskflow.png

# Update caches
if command -v update-desktop-database > /dev/null; then
    update-desktop-database /usr/share/applications || true
fi
if command -v gtk-update-icon-cache > /dev/null; then
    gtk-update-icon-cache -f -t /usr/share/icons/hicolor || true
fi
EOF
chmod 755 "$DEB_ROOT/DEBIAN/postinst"

# 7. Create prerm to cleanup Symlinks
cat > "$DEB_ROOT/DEBIAN/prerm" <<EOF
#!/bin/bash
rm -f /usr/bin/deskflow-hid
rm -f /usr/share/applications/org.deskflow.deskflow.desktop
rm -f /usr/share/icons/hicolor/512x512/apps/org.deskflow.deskflow.png
EOF
chmod 755 "$DEB_ROOT/DEBIAN/prerm"


# 8. Build Deb
echo "Building Debian Package..."
dpkg-deb --root-owner-group --build "$DEB_ROOT" "$OUTPUT_DIR/deskflow-hid_${VERSION}_${ARCH}.deb"

echo "Done. Package available at $OUTPUT_DIR/deskflow-hid_${VERSION}_${ARCH}.deb"
