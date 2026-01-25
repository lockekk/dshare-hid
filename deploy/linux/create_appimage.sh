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

    # Export for linuxdeploy if needed
    QT_BIN_DIR=$($QMAKE -query QT_INSTALL_BINS)
    export PATH="$QT_BIN_DIR:$PATH"
fi

if [ ! -z "$QT_PLUGINS_DIR" ] && [ -d "$QT_PLUGINS_DIR" ]; then
    # Copy imageformats plugins (needed for SVG icon loading)
    if [ -d "$QT_PLUGINS_DIR/imageformats" ]; then
        echo "Copying imageformats manually..."
        mkdir -p "$APPDIR/usr/plugins/imageformats"

        # Explicit check and copy for libqsvg.so
        if [ -f "$QT_PLUGINS_DIR/imageformats/libqsvg.so" ]; then
             cp "$QT_PLUGINS_DIR/imageformats/libqsvg.so" "$APPDIR/usr/plugins/imageformats/"
             echo "Copied libqsvg.so"
        else
             echo "Warning: libqsvg.so not found in $QT_PLUGINS_DIR/imageformats"
        fi

        # Explicit check and copy for libqico.so
        if [ -f "$QT_PLUGINS_DIR/imageformats/libqico.so" ]; then
            cp "$QT_PLUGINS_DIR/imageformats/libqico.so" "$APPDIR/usr/plugins/imageformats/"
             echo "Copied libqico.so"
        fi
    fi

    # Manually copy iconengines/libqsvgicon.so if it exists (critical for SVG icons)
    if [ -f "$QT_PLUGINS_DIR/iconengines/libqsvgicon.so" ]; then
        echo "Copying libqsvgicon.so manually..."
        mkdir -p "$APPDIR/usr/plugins/iconengines"
        cp "$QT_PLUGINS_DIR/iconengines/libqsvgicon.so" "$APPDIR/usr/plugins/iconengines/"
        echo "Copied libqsvgicon.so"
    else
         echo "Warning: libqsvgicon.so not found in $QT_PLUGINS_DIR/iconengines"
    fi
else
    echo "Warning: Could not determine Qt plugins directory. Icons may be missing."
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

EXECUTABLE="$APPDIR/usr/bin/dshare-hid"

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
    --icon-file "$APPDIR/usr/share/icons/hicolor/512x512/apps/io.github.lockekk.dshare-hid.png" \
    --executable "$EXECUTABLE"

# 5. Extract version for filename
MAJOR=$(grep "set(DESKFLOW_VERSION_MAJOR" "$PROJECT_ROOT/CMakeLists.txt" | head -n1 | awk '{print $2}' | tr -d ')')
MINOR=$(grep "set(DESKFLOW_VERSION_MINOR" "$PROJECT_ROOT/CMakeLists.txt" | head -n1 | awk '{print $2}' | tr -d ')')
PATCH=$(grep "set(DESKFLOW_VERSION_PATCH" "$PROJECT_ROOT/CMakeLists.txt" | head -n1 | awk '{print $2}' | tr -d ')')
VERSION="${MAJOR}.${MINOR}.${PATCH}"
ARCH=$(uname -m)
APPIMAGE_NAME="dshare-hid-${VERSION}-linux-${ARCH}.AppImage"

# Move the result to output dir
mv DShare-HID*AppImage "$OUTPUT_DIR/$APPIMAGE_NAME"

echo "AppImage created in $OUTPUT_DIR as $APPIMAGE_NAME"
