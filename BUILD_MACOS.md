# Building Deskflow on macOS (Step-by-Step)

This guide provides the exact steps to build, sign, and launch Deskflow from a clean state.

## 1. Environment Variables

Ensure these variables are set in your terminal session. Replace the paths if yours are different.

```bash
export CMAKE_PREFIX_PATH="/Users/lockehuang/Qt/6.10.1/macos"
export CMAKE_OSX_SYSROOT="/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
export APPLE_CODESIGN_DEV="Apple Development: locke.huang@gmail.com (8479KTS8YJ)"
```

## 2. Clean and Configure

If you want a truly clean build, remove the `build` directory first.

```bash
rm -rf build
cmake -B build -G "Unix Makefiles" \
  -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH" \
  -DCMAKE_OSX_SYSROOT="$CMAKE_OSX_SYSROOT" \
  -DCMAKE_BUILD_TYPE=Release
```

## 3. Build

Build the main Deskflow application.

```bash
cmake --build build -j$(sysctl -n hw.ncpu)
```

**Note**: The binaries will be located in `build/bin/Deskflow.app`.
- Main App: `build/bin/Deskflow.app/Contents/MacOS/Deskflow`
- Core Service: `build/bin/Deskflow.app/Contents/MacOS/deskflow-core`

### 4. Deploy Qt Frameworks
The application requires Qt frameworks to be bundled. Run the following command (ensure `CMAKE_PREFIX_PATH` is set as per step 1):

```bash
$CMAKE_PREFIX_PATH/bin/macdeployqt build/bin/Deskflow.app
```

### 5. Sign (Required for Privacy/Accessibility)

On macOS, the app must be signed to function correctly with system permissions.

```bash
# Sign the entire bundle
codesign --force --deep --sign "$APPLE_CODESIGN_DEV" build/bin/Deskflow.app
```

## 5. Launch

You can launch the app directly from the build directory:

```bash
# Run the main GUI
build/bin/Deskflow.app/Contents/MacOS/Deskflow
```

*Note: The GUI will automatically launch `deskflow-core` from the bundle.*

## Troubleshooting

- **Permissions**: If the app fails to capture input, try resetting accessibility permissions:
  ```bash
  sudo tccutil reset Accessibility com.symless.deskflow
  ```
- **Leftover Processes**: If the server won't start, check for hung `deskflow-core` processes:
  ```bash
  pkill -9 -if deskflow
  ```
- **Runtime Errors (dyld / missing core)**:
  - If you see `core server binary does not exist`, ensure you built with `--target Deskflow deskflow-core`.
