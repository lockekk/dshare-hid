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
APPDIR="$OUTPUT_DIR/AppDir"

echo "Project Root: $PROJECT_ROOT"
echo "Build Dir:    $BUILD_DIR"
echo "Output Dir:   $OUTPUT_DIR"

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

    # Patch the Qt plugin to avoid some common issues with older glibc checks if running in newer env?
    # Usually not needed for basic usage, but kept in mind.
fi

# Clean up previous run
rm -rf "$APPDIR"

# Install into AppDir
echo "Installing into AppDir..."
DESTDIR="$APPDIR" cmake --install "$BUILD_DIR" --prefix "/usr"

# Check if desktop file exists where we expect it
DESKTOP_FILE=$(find "$APPDIR" -name "*.desktop" | head -n 1)
if [ -z "$DESKTOP_FILE" ]; then
    echo "Error: Could not find .desktop file in AppDir deployment."
    exit 1
fi

# Locate specific libraries if they are in custom locations
# The user is expected to have set PKG_CONFIG_PATH / LD_LIBRARY_PATH if they are in non-standard places.
# linuxdeploy usually looks at the binary's rpath and LD_LIBRARY_PATH.

echo "Running linuxdeploy..."

# We need to export plugin location
export LINUXDEPLOY_PLUGIN_QT_PATH="$LINUXDEPLOY_PLUGIN_QT"

# Auto-detect custom Qt from CMakeCache
CACHE_FILE="$BUILD_DIR/CMakeCache.txt"
if [ -f "$CACHE_FILE" ]; then
    QT_DIR_LINE=$(grep "Qt6_DIR:PATH=" "$CACHE_FILE" | head -n 1)
    if [ ! -z "$QT_DIR_LINE" ]; then
        QT_CMAKE_PATH="${QT_DIR_LINE#*=}"
        QT_ROOT=$(dirname "$(dirname "$(dirname "$QT_CMAKE_PATH")")")
        QT_BIN="$QT_ROOT/bin"

        if [ -d "$QT_BIN" ] && [ -x "$QT_BIN/qmake" ]; then
            echo "Auto-detected Custom Qt: $QT_BIN"
            export PATH="$QT_BIN:$PATH"
            export QMAKE="$QT_BIN/qmake"
            export LD_LIBRARY_PATH="$QT_ROOT/lib:$LD_LIBRARY_PATH"

            # Manually copy iconengines/libqsvgicon.so if it exists (critical for SVG icons)
            if [ -f "$QT_ROOT/plugins/iconengines/libqsvgicon.so" ]; then
                echo "Copying libqsvgicon.so manually..."
                mkdir -p "$APPDIR/usr/plugins/iconengines"
                cp "$QT_ROOT/plugins/iconengines/libqsvgicon.so" "$APPDIR/usr/plugins/iconengines/"
            fi
        fi
    fi
fi

# If we have custom libportal/libei, ensure they are in LD_LIBRARY_PATH
# This is just a helper echo, the user must control the environment before calling this script if strictly needed.
echo "Current LD_LIBRARY_PATH: $LD_LIBRARY_PATH"
echo "Current PATH (for qmake): $PATH"

# Run linuxdeploy
# --appdir: The directory to bundle
# --plugin: Use the qt plugin
# --output: Generate an AppImage
# --icon-file: Explicitly set icon if needed (usually auto-detected from desktop file)
# --desktop-file: Explicitly set desktop file (usually auto-detected)
# -e: Executable to inspect for dependencies

EXECUTABLE="$APPDIR/usr/bin/deskflow-hid"

if [ ! -f "$EXECUTABLE" ]; then
     # Try to find it blindly if the path is different
     EXECUTABLE=$(find "$APPDIR/usr/bin" -type f -executable | head -n 1)
fi

if [ -z "$EXECUTABLE" ]; then
    echo "Error: Could not find executable in $APPDIR/usr/bin"
    exit 1
fi

"$LINUXDEPLOY" --appdir "$APPDIR" \
    --plugin qt \
    --output appimage \
    --desktop-file "$DESKTOP_FILE" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/512x512/apps/org.deskflow.deskflow.png" \
    --executable "$EXECUTABLE"

# Move the result to output dir
mv Deskflow*AppImage "$OUTPUT_DIR/"

echo "AppImage created in $OUTPUT_DIR"
