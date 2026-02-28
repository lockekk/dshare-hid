# macOS Deployment Guide for DShare-HID

## Overview

This document explains the different macOS deployment options for DShare-HID and how OpenSSL 3.x is handled in each scenario.

---

## Why DShare-HID Has Special OpenSSL Requirements

### Upstream Deskflow vs DShare-HID

| Feature                 | Upstream Deskflow               | DShare-HID                     |
| ----------------------- | ------------------------------- | ------------------------------ |
| OpenSSL Usage           | TLS/SSL networking, SHA digests | ECDSA signature verification   |
| Provider Loading Needed | **No** - auto-loaded            | **Yes** - must load explicitly |
| ossl-modules Needed     | **No**                          | **Yes** - on macOS bundles     |

**Key Point:** Your HID bridge uses `EVP_DigestVerify()` for ECDSA signature verification, which requires explicit OpenSSL 3.x provider loading. Upstream only uses basic crypto (SHA, TLS) that works without explicit provider loading.

---

## Deployment Options Comparison

### Quick Reference

| Deployment Type      | OpenSSL Handling    | Code Signing      | Distribution Method | User Install       |
| -------------------- | ------------------- | ----------------- | ------------------- | ------------------ |
| **DMG (App Bundle)** | Bundle ossl-modules | Required          | GitHub Releases     | Download & drag    |
| **Homebrew Cask**    | Bundle ossl-modules | Required (in DMG) | `brew install`      | Automatic          |
| **Homebrew Formula** | System OpenSSL      | Not required      | `brew install`      | Builds from source |
| **Direct Build**     | System OpenSSL      | Optional          | Manual              | Manual             |

---

## 1. DMG (Disk Image) - Standalone App Bundle

### What It Is

A `.dmg` file containing a self-contained `.app` bundle with all libraries bundled inside.

### Build Process

```bash
# Configure
cmake -B build -G "Unix Makefiles" \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.10.1/macos" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=OFF

# Build
cmake --build build -j$(sysctl -n hw.ncpu)

# Create DMG
cd build && cpack
```

### What Gets Bundled

```
DShare-HID.app/
├── Contents/
│   ├── MacOS/
│   │   └── DShare-HID              # Main executable
│   ├── Frameworks/
│   │   ├── Qt*.dylib                 # Qt frameworks (via macdeployqt)
│   │   ├── libssl.dylib              # OpenSSL SSL library
│   │   ├── libcrypto.dylib           # OpenSSL Crypto library
│   │   └── ossl-modules/             # OpenSSL 3.x provider modules
│   │       ├── legacy.dylib
│   │       └── padlock.dylib
│   ├── Resources/
│   │   └── ...                       # Icons, plists, etc.
│   └── Info.plist
```

### OpenSSL Handling

**Issue:** When bundled, the ossl-modules are not in standard system paths.

**Solution:**
1. Bundle ossl-modules into `Contents/Frameworks/ossl-modules/`
2. Set `OPENSSL_MODULES` environment variable to point there
3. Explicitly load providers at startup

**Code:**
```cpp
// In CdcTransport.cpp - runs at app startup
#ifdef Q_OS_MAC
  CFBundleRef mainBundle = CFBundleGetMainBundle();
  CFURLRef frameworkURL = CFBundleCopyPrivateFrameworksURL(mainBundle);
  if (frameworkURL) {
    char path[1024];
    if (CFURLGetFileSystemRepresentation(frameworkURL, true, (UInt8 *)path, sizeof(path))) {
      std::string modulesPath = std::string(path) + "/ossl-modules";
      setenv("OPENSSL_MODULES", modulesPath.c_str(), 1);
    }
    CFRelease(frameworkURL);
  }
#endif

// Load providers
OSSL_PROVIDER_load(nullptr, "default");
OSSL_PROVIDER_load(nullptr, "legacy");
```

### Code Signing Requirements

| Requirement      | Command                                                               |
| ---------------- | --------------------------------------------------------------------- |
| Basic signing    | `codesign --force --deep --sign "IDENTITY" App.app`                   |
| Hardened Runtime | `codesign --force --deep --options=runtime --sign "IDENTITY" App.app` |
| Verification     | `codesign --verify --deep --verbose=4 App.app`                        |

**Why Hardened Runtime (`--options=runtime`):**
- Required for USB device access (ESP32 communication)
- Required for notarization (distribution outside App Store)

### Deployment Script

See `deploy/mac/deploy.cmake` for the automated signing and bundling process:

```cmake
# 1. Run macdeployqt
execute_process(COMMAND ${DEPLOYQT} "${APP_PATH}" -verbose=1)

# 2. Bundle OpenSSL modules
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory
  "${OSSL_MOD_PATH}" "${APP_PATH}/Contents/Frameworks/ossl-modules")

# 3. Clean extended attributes
execute_process(COMMAND xattr -cr "${APP_PATH}")

# 4. Sign with Hardened Runtime
execute_process(COMMAND codesign --force --deep --options=runtime
  --sign "${OSX_CODESIGN_IDENTITY}" "${APP_PATH}")
```

### Architecture Support

| Build Type            | Command              | Output                                 |
| --------------------- | -------------------- | -------------------------------------- |
| ARM64 (Apple Silicon) | Default on ARM Mac   | `DShare-HID-1.0.0-macos-arm64.dmg`     |
| x86_64 (Intel)        | Default on Intel Mac | `DShare-HID-1.0.0-macos-x86_64.dmg`    |
| Universal             | Manual `lipo` step   | `DShare-HID-1.0.0-macos-universal.dmg` |

**Note:** The `build_universal_openssl.sh` script was removed. You now build architecture-specific DMGs separately.

---

## 2. Homebrew Cask

### What It Is

A Ruby recipe that installs your pre-built DMG via Homebrew.

### Cask File Example

```ruby
# Casks/dshare-hid.rb
cask "dshare-hid" do
  version "1.0.0"
  sha256 "dmg_sha256_hash_here"

  url "https://github.com/lockekk/dshare-hid/releases/download/v#{version}/DShare-HID-#{version}.macos-arm64.dmg"
  name "DShare-HID"
  desc "Professional Cross-Platform HID Bridge for DShare-HID"
  homepage "https://github.com/lockekk/dshare-hid"

  app "DShare-HID.app"

  zap trash: [
    "~/Library/DShare-HID",
    "~/Library/Application Support/DShare-HID",
  ]
end
```

### Architecture-Specific Casks

You need separate casks for ARM and Intel:

```ruby
# For ARM (Apple Silicon)
cask "dshare-hid-arm" do
  url "https://github.com/lockekk/dshare-hid/releases/download/v#{version}/DShare-HID-#{version}.macos-arm64.dmg"
  # ...
end

# For Intel
cask "dshare-hid-intel" do
  url "https://github.com/lockekk/dshare-hid/releases/download/v#{version}/DShare-HID-#{version}.macos-x86_64.dmg"
  # ...
end
```

### OpenSSL Handling

**Same as DMG** - the Cask just installs your pre-built DMG, so all the bundling and signing you did for the DMG applies here.

### User Installation

```bash
brew install --cask dshare-hid-arm    # Apple Silicon
brew install --cask dshare-hid-intel  # Intel
```

### Updates

```bash
brew upgrade --cask dshare-hid-arm
```

---

## 3. Homebrew Formula

### What It Is

A Ruby recipe that builds DShare-HID from source using system libraries.

### Formula File Example

```ruby
# Formula/dshare-hid.rb
class DeskflowHid < Formula
  desc "Professional Cross-Platform HID Bridge for Deskflow"
  homepage "https://github.com/lockekk/dshare-hid"
  url "https://github.com/lockekk/dshare-hid/archive/refs/tags/v1.0.0.tar.gz"
  sha256 "source_tarball_sha256_here"
  license "GPL-2.0-only"

  depends_on "cmake" => :build
  depends_on "qt"
  depends_on "openssl@3"

  def install
    system "cmake", "-S", ".", "-B", "build", *std_cmake_args,
      "-DBUILD_TESTS=OFF"
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
  end

  def caveats
    <<~EOS
      DShare-HID requires access to USB devices for ESP32 communication.
      You may need to grant permissions in System Preferences > Security & Privacy.
    EOS
  end
end
```

### OpenSSL Handling

**No special handling needed!** Homebrew's OpenSSL is already properly configured:

- Libraries at: `/opt/homebrew/opt/openssl@3/lib` (ARM) or `/usr/local/opt/openssl@3/lib` (Intel)
- Modules at: `/opt/homebrew/opt/openssl@3/ossl-modules`
- `OPENSSL_MODULES` already set by Homebrew

Your `ensureOpenSslProviders()` function will work automatically - it will find the system OpenSSL modules.

### Code Signing

**Not required** for Homebrew builds. The build process doesn't create a signed bundle.

### User Installation

```bash
brew install dshare-hid
```

---

## 4. Direct Build (Development)

### What It Is

Building directly from source without creating a distributable package.

### Build Process

```bash
cmake -B build -G "Unix Makefiles" \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.10.1/macos" \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build -j$(sysctl -n hw.ncpu)
./build/bin/DShare-HID.app/Contents/MacOS/DShare-HID
```

### OpenSSL Handling

Depends on how OpenSSL was found:

| OpenSSL Source                    | Module Location                            | Action Needed         |
| --------------------------------- | ------------------------------------------ | --------------------- |
| Homebrew (`brew install openssl`) | `/opt/homebrew/opt/openssl@3/ossl-modules` | None (auto-found)     |
| Custom build                      | Custom path                                | Set `OPENSSL_MODULES` |
| Bundled (via cmake)               | Build directory                            | Set `OPENSSL_MODULES` |

---

## Comparison Summary

### Feature Matrix

| Feature              | DMG                    | Homebrew Cask          | Homebrew Formula         | Direct Build           |
| -------------------- | ---------------------- | ---------------------- | ------------------------ | ---------------------- |
| **Distribution**     | GitHub Releases        | Homebrew               | Homebrew                 | N/A (dev)              |
| **Build Time**       | Pre-built              | Pre-built              | On user's machine        | On developer's machine |
| **Install Size**     | ~100MB                 | ~100MB                 | ~50MB (uses system libs) | Variable               |
| **Update Method**    | Manual download        | `brew upgrade`         | `brew upgrade`           | Manual git pull        |
| **Code Signing**     | Required               | Required (in DMG)      | Not required             | Optional               |
| **Notarization**     | Required               | Required (in DMG)      | Not required             | Not required           |
| **OpenSSL Handling** | Must bundle modules    | Must bundle modules    | System (auto)            | Depends on setup       |
| **USB Access**       | Needs entitlements     | Needs entitlements     | May need permissions     | May need permissions   |
| **Architecture**     | ARM + Intel (separate) | ARM + Intel (separate) | ARM + Intel (auto)       | Host arch only         |
| **Suitable For**     | Public release         | Power users            | Developers               | Development            |

### Deployment Complexity

```
┌─────────────────────────────────────────────────────────────┐
│                    Deployment Complexity                     │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  High    │  DMG (with signing & notarization)              │
│          │                                                 │
│  Medium  │  Homebrew Cask                                   │
│          │                                                 │
│  Low     │  Homebrew Formula                               │
│          │  Direct Build                                   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## Recommended Release Strategy

### For Public Release

1. **Primary: DMG** (for general users)
   - Build ARM and Intel versions separately
   - Code sign with Hardened Runtime
   - Notarize with Apple
   - Upload to GitHub Releases

2. **Secondary: Homebrew Cask** (for power users)
   - Points to your DMG
   - Easy install/upgrade via `brew`

3. **Optional: Homebrew Formula** (for developers)
   - Builds from source
   - Uses system libraries
   - No signing needed

### File Structure for Release

```
release/
├── DShare-HID-1.0.0-macos-arm64.dmg
├── DShare-HID-1.0.0-macos-arm64.dmg.dmg.asc       # GPG signature (optional)
├── DShare-HID-1.0.0-macos-x86_64.dmg
├── DShare-HID-1.0.0-macos-x86_64.dmg.dmg.asc
└── checksums.txt                                     # SHA256 checksums
```

---

## Troubleshooting

### "Damaged" App Error

**Cause:** Extended attributes or quarantine flags on the bundle.

**Solution:** Clear attributes before signing:
```bash
xattr -cr DShare-HID.app
codesign --force --deep --options=runtime --sign "IDENTITY" DShare-HID.app
```

### OpenSSL Provider Not Loading

**Symptom:** `Failed to load OpenSSL 'default' provider`

**Debug:**
```bash
# Check if modules are found
echo $OPENSSL_MODULES

# Check bundle contents
ls -la DShare-HID.app/Contents/Frameworks/ossl-modules/

# Check code signature
codesign -dv DShare-HID.app
```

### USB Device Access Denied

**Cause:** Missing Hardened Runtime entitlements.

**Solution:**
1. Add `--options=runtime` to codesign
2. Create entitlements file with USB access:
```xml
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>com.apple.security.device.usb</key>
    <true/>
</dict>
</plist>
```
3. Sign with entitlements: `codesign --options=runtime --entitlements entitlements.plist ...`

---

## References

- [deploy/mac/deploy.cmake](../deploy/mac/deploy.cmake) - macOS deployment script
- [doc/apple_signing_guide.md](apple_signing_guide.md) - Code signing instructions
- [run_task.sh](../run_task.sh) - Build automation script
- [scripts/uninstall_macos.sh](../scripts/uninstall_macos.sh) - Uninstaller script

---

*Last Updated: 2025-01-16*
