# Building Deskflow on macOS (Step-by-Step)

This guide provides the exact steps to build, sign, and launch Deskflow from a clean state.

## 1. Prerequisites: Code Signing

To run the app with Accessibility permissions, you need a valid signing identity.
The easiest way is to use Xcode to generate a free "Apple Development" certificate.

1. **Open Xcode**:
   ```bash
   open -a Xcode
   ```
2. Go to **Xcode** menu (top bar) → **Settings** (or Preferences).
3. Click the **Accounts** tab.
4. Add your **Apple ID** if not listed (click `+` in bottom left).
5. Select your account (Team) → Click **Manage Certificates...**
6. Click the **+** button (bottom left) → Select **Apple Development**.
7. Click **Done**.

Verify it exists by running:
```bash
security find-identity -p codesigning -v
```
*You should see a line like: `"Apple Development: your.email@example.com (TEAMID)"`*

### Troubleshooting: Invalid Identity?
If `security find-identity` shows the certificate but says **0 valid identities**, run this to trust it:

```bash
# 1. Export the certificate (replace with your email found above)
security find-certificate -c "locke.huang@gmail.com" -p > /tmp/mycert.pem

# 2. Add to trusted store (will ask for password)
security add-trusted-cert -r trustRoot -k "$HOME/Library/Keychains/login.keychain-db" /tmp/mycert.pem
```

## 2. Environment Variables

Add these to your `~/.zshrc` (recommended) or export them in your current session.

> **Important**: Replace `YOUR_IDENTITY_STRING_HERE` with the exact string found in the verification step above (including quotes if it has spaces).

```bash
export CMAKE_PREFIX_PATH="$HOME/Qt/6.10.1/macos"
export CMAKE_OSX_SYSROOT="/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
export APPLE_CODESIGN_DEV="Apple Development: locke.huang@gmail.com (8479KTS8YJ)"
```

Then source your config:
```bash
source ~/.zshrc
```

## 3. Clean and Configure

If you want a truly clean build, remove the `build` directory first.

```bash
rm -rf build
cmake -B build -G "Unix Makefiles" \
  -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH" \
  -DCMAKE_OSX_SYSROOT="$CMAKE_OSX_SYSROOT" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=OFF \
  -DSKIP_BUILD_TESTS=ON
```

## 4. Build

Build the main Deskflow application.

```bash
cmake --build build -j$(sysctl -n hw.ncpu)
```

**Note**: The binaries will be located in `build/bin/Deskflow-HID.app`.
- Main App: `build/bin/Deskflow-HID.app/Contents/MacOS/Deskflow-HID`
- Core Service: `build/bin/Deskflow-HID.app/Contents/MacOS/deskflow-core`

### 4. Deploy Qt Frameworks
The application requires Qt frameworks to be bundled. Run the following command (ensure `CMAKE_PREFIX_PATH` is set as per step 1):

```bash
$CMAKE_PREFIX_PATH/bin/macdeployqt build/bin/Deskflow-HID.app
```

### 5. Sign (Required for Privacy/Accessibility)

On macOS, the app must be signed to function correctly with system permissions.

```bash
# Sign the entire bundle
codesign --force --deep --sign "$APPLE_CODESIGN_DEV" build/bin/Deskflow-HID.app
```

## 5. Launch

You can launch the app directly from the build directory:

```bash
# Run the main GUI
build/bin/Deskflow-HID.app/Contents/MacOS/Deskflow-HID
```

*Note: The GUI will automatically launch `deskflow-core` from the bundle.*

## Troubleshooting

- **Permissions**: If the app fails to capture input, try resetting accessibility permissions:
  ```bash
  sudo tccutil reset Accessibility org.deskflow.deskflow-hid
  ```
- **Leftover Processes**: If the server won't start, check for hung `deskflow-core` processes:
  ```bash
  ps -ef | grep deskflow-hid-core | grep -v grep
  pkill -9 -if deskflow-hid
  pkill -9 -if deskflow-hid-core
  ```
- **Runtime Errors (dyld / missing core)**:
  - If you see `core server binary does not exist`, ensure you built with `--target Deskflow-HID deskflow-hid-core`.
