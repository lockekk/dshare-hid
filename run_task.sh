#!/bin/bash

# Ensure required environment variables are set
if [ "$(uname)" = "Darwin" ]; then
    if [ -z "$CMAKE_PREFIX_PATH" ]; then
        # Try to auto-detect Qt (common versions)
        if [ -d "$HOME/Qt/6.10.1/macos" ]; then
            export CMAKE_PREFIX_PATH="$HOME/Qt/6.10.1/macos"
            echo "Auto-detected Qt at: $CMAKE_PREFIX_PATH"
        elif [ -d "$HOME/Qt/6.8.0/macos" ]; then
            export CMAKE_PREFIX_PATH="$HOME/Qt/6.8.0/macos"
            echo "Auto-detected Qt at: $CMAKE_PREFIX_PATH"
        else
            echo "Warning: CMAKE_PREFIX_PATH is not set. Build may fail."
        fi
    fi

    if [ -z "$CMAKE_OSX_SYSROOT" ]; then
         if command -v xcrun &> /dev/null; then
            export CMAKE_OSX_SYSROOT="$(xcrun --show-sdk-path)"
            echo "Auto-detected macOS SDK at: $CMAKE_OSX_SYSROOT"
        else
             echo "Warning: CMAKE_OSX_SYSROOT is not set."
        fi
    fi
fi

process_input() {
    local input="$1"
    local input_lower=$(echo "$input" | tr '[:upper:]' '[:lower:]')
    local os_name=$(uname -s)

    case "$input_lower" in
        1|"build")
            if [ ! -d "build" ]; then
                echo "Error: 'build' directory not found. Please run a configuration step (2 or 4) first."
                return 1
            fi
            echo "--- BUILDING ---"
            cmake --build build -j$(sysctl -n hw.ncpu 2>/dev/null || nproc)
            if [ $? -eq 0 ]; then
                if [ "$os_name" = "Darwin" ]; then
                    echo "--- DEPLOYING QT ---"
                    $CMAKE_PREFIX_PATH/bin/macdeployqt build/bin/DShare-HID.app -executable=build/bin/DShare-HID.app/Contents/MacOS/dshare-hid-core
                    echo "--- SIGNING ---"
                    codesign --force --deep --sign "$APPLE_CODESIGN_DEV" build/bin/DShare-HID.app
                fi
                echo "Build complete."
            else
                echo "Build failed."
                return 1
            fi
            ;;
        2|"configure")
            echo "--- CLEANING AND RECONFIGURING (PRISTINE) ---"
            rm -rf build
            if [ "$os_name" = "Darwin" ]; then
                cmake -B build -G "Unix Makefiles" \
                  -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH" \
                  -DCMAKE_OSX_SYSROOT="$CMAKE_OSX_SYSROOT" \
                  -DCMAKE_BUILD_TYPE=Release \
                  -DSKIP_BUILD_TESTS=ON \
                  -DBUILD_TESTS=OFF \
                  -DDESKFLOW_PAYPAL_ACCOUNT="$PAYPAL_ACCOUNT" \
                  -DDESKFLOW_PAYPAL_URL="$PAYPAL_URL"
            else
                 # Linux Configuration
                 cmake -B build -G "Unix Makefiles" \
                  -DCMAKE_BUILD_TYPE=Release \
                  -DSKIP_BUILD_TESTS=ON \
                  -DBUILD_TESTS=OFF \
                  -DDESKFLOW_PAYPAL_ACCOUNT="$PAYPAL_ACCOUNT" \
                  -DDESKFLOW_PAYPAL_URL="$PAYPAL_URL"
            fi
            echo "Configuration complete. Run option 1 to build."
            ;;
        4|"configure release")
            echo "--- CLEANING AND RECONFIGURING (RELEASE) ---"
            rm -rf build
            if [ "$os_name" = "Darwin" ]; then
                cmake -B build -G "Unix Makefiles" \
                  -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH" \
                  -DCMAKE_OSX_SYSROOT="$CMAKE_OSX_SYSROOT" \
                  -DCMAKE_BUILD_TYPE=Release \
                  -DSKIP_BUILD_TESTS=ON \
                  -DBUILD_TESTS=OFF \
                  -DDESKFLOW_PAYPAL_ACCOUNT="$PAYPAL_ACCOUNT" \
                  -DDESKFLOW_PAYPAL_URL="$PAYPAL_URL" \
                  -DDESKFLOW_CDC_PUBLIC_KEY="$DESKFLOW_CDC_PUBLIC_KEY" \
                  -DDESKFLOW_ESP32_ENCRYPTION_KEY="$DESKFLOW_ESP32_ENCRYPTION_KEY"
            else
                 # Linux Configuration
                 cmake -B build -G "Unix Makefiles" \
                  -DCMAKE_BUILD_TYPE=Release \
                  -DSKIP_BUILD_TESTS=ON \
                  -DBUILD_TESTS=OFF \
                  -DDESKFLOW_PAYPAL_ACCOUNT="$PAYPAL_ACCOUNT" \
                  -DDESKFLOW_PAYPAL_URL="$PAYPAL_URL" \
                  -DDESKFLOW_CDC_PUBLIC_KEY="$DESKFLOW_CDC_PUBLIC_KEY" \
                  -DDESKFLOW_ESP32_ENCRYPTION_KEY="$DESKFLOW_ESP32_ENCRYPTION_KEY"
            fi
            echo "Release Configuration complete. Run option 1 to build."
            ;;
        3|"launch")
            if [ "$os_name" = "Darwin" ]; then
                if [ ! -d "build/bin/DShare-HID.app" ]; then
                    echo "Error: Application not built. Select '1' to build first."
                else
                    echo "--- LAUNCHING DSHARE-HID ---"
                    build/bin/DShare-HID.app/Contents/MacOS/DShare-HID
                fi
            else
                # Linux Launch
                 if [ ! -f "build/bin/dshare-hid" ]; then
                    echo "Error: Application not built. Select '1' to build first."
                else
                     echo "--- LAUNCHING DSHARE-HID ---"
                     ./build/bin/dshare-hid
                fi
            fi
            ;;
        5|"build deploy")
            if [ "$os_name" = "Darwin" ]; then
                echo "--- PREPARING MACOS DEPLOY (UNIVERSAL) ---"

                # 1. Ensure Universal OpenSSL (x86_64 + arm64) is available
                OPENSSL_UNIVERSAL_DIR="$(pwd)/deps/openssl-universal/universal"
                if [ ! -f "${OPENSSL_UNIVERSAL_DIR}/lib/libssl.dylib" ]; then
                    echo "Universal OpenSSL not found. Building it now (this takes a few minutes)..."
                    ./deploy/mac/build_universal_openssl.sh
                    if [ $? -ne 0 ]; then
                        echo "Error: Failed to build Universal OpenSSL."
                        return 1
                    fi
                else
                    echo "Found Universal OpenSSL at: ${OPENSSL_UNIVERSAL_DIR}"
                fi

                # Check for signing identity
                TARGET_IDENTITY="-"
                if [ -n "$APPLE_CODESIGN_DEV" ]; then
                    TARGET_IDENTITY="$APPLE_CODESIGN_DEV"
                    echo "Signing with Identity: $TARGET_IDENTITY"
                else
                    echo "No signing identity found (APPLE_CODESIGN_DEV not set). Using ad-hoc signing."
                fi

                # Use a separate build directory for deployment to avoid polluting dev build
                BUILD_DIR="build_dmg"
                rm -rf "$BUILD_DIR"

                echo "--- CONFIGURING ---"
                # Note: CMAKE_OSX_ARCHITECTURES="arm64;x86_64" enables the universal build
                cmake -B "$BUILD_DIR" -G "Unix Makefiles" \
                  -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH" \
                  -DCMAKE_OSX_SYSROOT="$CMAKE_OSX_SYSROOT" \
                  -DCMAKE_BUILD_TYPE=Release \
                  -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
                  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
                  -DSKIP_BUILD_TESTS=ON \
                  -DBUILD_TESTS=OFF \
                  -DDESKFLOW_PAYPAL_ACCOUNT="$PAYPAL_ACCOUNT" \
                  -DDESKFLOW_PAYPAL_URL="$PAYPAL_URL" \
                  -DDESKFLOW_CDC_PUBLIC_KEY="$DESKFLOW_CDC_PUBLIC_KEY" \
                  -DDESKFLOW_ESP32_ENCRYPTION_KEY="$DESKFLOW_ESP32_ENCRYPTION_KEY" \
                  -DOSX_CODESIGN_IDENTITY="$TARGET_IDENTITY" \
                  -DOPENSSL_ROOT_DIR="${OPENSSL_UNIVERSAL_DIR}"

                if [ $? -ne 0 ]; then
                    echo "Configuration failed."
                    return 1
                fi

                echo "--- BUILDING ---"
                cmake --build "$BUILD_DIR" -j$(sysctl -n hw.ncpu 2>/dev/null || nproc)

                if [ $? -ne 0 ]; then
                    echo "Build failed."
                    return 1
                fi

                echo "--- VERIFYING ARCHITECTURES ---"
                BINARY_PATH="$BUILD_DIR/bin/DShare-HID.app/Contents/MacOS/DShare-HID"
                if [ -f "$BINARY_PATH" ]; then
                    lipo -info "$BINARY_PATH"
                else
                    echo "Warning: Binary not found at $BINARY_PATH"
                fi

                echo "--- PACKAGING (DMG) ---"
                # CPack needs to run from within the build directory
                (cd "$BUILD_DIR" && cpack)

                if [ $? -eq 0 ]; then
                    echo "Deployment package created successfully in $BUILD_DIR"
                else
                    echo "Packaging failed."
                    return 1
                fi
                return 0
            fi

            if ! command -v flatpak-builder &> /dev/null; then
                echo "Error: flatpak-builder is not installed."
                return 1
            fi

            echo "--- PREPARING FLATPAK BUILD (RELEASE) ---"

            # Extract directories for keys to mount them into the sandbox
            ENC_KEY_DIR=$(dirname "${DESKFLOW_ESP32_ENCRYPTION_KEY}")
            CDC_KEY_DIR=$(dirname "${DESKFLOW_CDC_PUBLIC_KEY}")

            sed "/-DCMAKE_BUILD_TYPE=Release/a \\
      - \"-DDESKFLOW_PAYPAL_ACCOUNT=${PAYPAL_ACCOUNT}\"\\
      - \"-DDESKFLOW_PAYPAL_URL=${PAYPAL_URL}\"\\
      - \"-DDESKFLOW_CDC_PUBLIC_KEY=${DESKFLOW_CDC_PUBLIC_KEY}\"\\
      - \"-DDESKFLOW_ESP32_ENCRYPTION_KEY=${DESKFLOW_ESP32_ENCRYPTION_KEY}\"\\
      - \"-DSKIP_BUILD_TESTS=ON\"\\
      - \"-DBUILD_TESTS=OFF\"" \
            deploy/linux/flatpak/io.github.lockekk.dshare-hid.yml > io.github.lockekk.dshare-hid.generated.yml

            # 1. Fix patch file path
            sed -i "s|path: libportal-qt69.patch|path: deploy/linux/flatpak/libportal-qt69.patch|g" io.github.lockekk.dshare-hid.generated.yml

            # 2. Fix source dir path (../../../ becomes .)
            sed -i "s|path: ../../../|path: .|g" io.github.lockekk.dshare-hid.generated.yml

            # 3. Inject build-time filesystem permissions for keys
            # We insert 'build-options' -> 'build-args' after the module name 'dshare-hid'
            sed -i "/- name: dshare-hid/a \\    build-options:\\n      build-args:\\n        - \"--filesystem=${ENC_KEY_DIR}\"\\n        - \"--filesystem=${CDC_KEY_DIR}\"" io.github.lockekk.dshare-hid.generated.yml

            echo "Generated manifest: io.github.lockekk.dshare-hid.generated.yml"

            echo "--- BUILDING FLATPAK ---"

            # Build using the generated manifest
            flatpak-builder --user --force-clean --repo=repo build_flatpak io.github.lockekk.dshare-hid.generated.yml

            if [ $? -eq 0 ]; then
                echo "--- CREATING BUNDLE ---"
                MAJOR=$(grep "set(DESKFLOW_VERSION_MAJOR" CMakeLists.txt | head -n1 | awk '{print $2}' | tr -d ')')
                MINOR=$(grep "set(DESKFLOW_VERSION_MINOR" CMakeLists.txt | head -n1 | awk '{print $2}' | tr -d ')')
                PATCH=$(grep "set(DESKFLOW_VERSION_PATCH" CMakeLists.txt | head -n1 | awk '{print $2}' | tr -d ')')
                VERSION="${MAJOR}.${MINOR}.${PATCH}"

                ARCH=$(uname -m)
                BUNDLE_NAME="dshare-hid-${VERSION}-linux-${ARCH}.flatpak"

                flatpak build-bundle --runtime-repo=https://dl.flathub.org/repo/flathub.flatpakrepo repo "build_flatpak/${BUNDLE_NAME}" io.github.lockekk.dshare-hid

                if [ $? -eq 0 ]; then
                    echo "Flatpak bundle created: ${BUNDLE_NAME}"
                else
                    echo "Failed to create flatpak bundle."
                fi
            else
                echo "Flatpak build failed."
                rm io.github.lockekk.dshare-hid.generated.yml
                return 1
            fi

            rm io.github.lockekk.dshare-hid.generated.yml
            ;;
        6|"appimage")
            if [ "$os_name" = "Darwin" ]; then
                echo "Error: AppImage generation is only supported on Linux."
                return 1
            fi
            echo "--- GENERATING APPIMAGE ---"
            if [ ! -d "build" ]; then
                echo "Error: 'build' directory not found. Please run a configuration step (2 or 4) first."
                return 1
            fi
            # Using 'build' as output directory as requested ("cat the build folder")
            ./deploy/linux/create_appimage.sh build build
            ;;
        7|"deb")
            if [ "$os_name" = "Darwin" ]; then
                echo "Error: Deb generation is only supported on Linux."
                return 1
            fi
            echo "--- GENERATING DEB PACKAGE ---"
             if [ ! -d "build" ]; then
                echo "Error: 'build' directory not found. Please run a configuration step (2 or 4) first."
                return 1
            fi
            ./deploy/linux/create_deb.sh build build_deb
            ;;
        q|"quit"|"exit")
            echo "Exiting..."
            return 1
            ;;
        *)
            echo "Invalid selection: $input"
            ;;
    esac
    return 0
}

# If argument supplied, run it and exit
if [ "$#" -gt 0 ]; then
    process_input "$@"
    exit $?
fi

# Interactive mode
while true; do
    echo ""
    echo "=================================================="
    echo "           Deskflow Interactive Build             "
    echo "=================================================="
    echo "  1) Build (Compile Only)"
    echo "  2) Configure Pristine (Clean & Reconfigure)"
    echo "  3) Launch"
    echo "  4) Configure Release (Clean & Config with Production Keys)"
    echo "  5) Build Deploy (Flatpak/DMG Release)"
    echo "  6) Build AppImage (Linux)"
    echo "  7) Build Deb (Ubuntu 24+)"
    echo "  q) Quit"
    echo ""
    read -p "Select a task (1-4 or q): " input

    process_input "$input"
    if [ $? -ne 0 ]; then
        break
    fi
done
