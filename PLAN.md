# USB-HID Bridge Device Plan

## Change Summary
- Multi-instance support validated in practice: two bridge clients can now connect concurrently to a single server on the same PC.
- Bridge client temporarily reports a fixed 1920×1080 display size so the server accepts DINF responses during debugging.

## Problem
Need to extend PC keyboard/mouse control to mobile devices (iPad/iPhone/Android) that don't support standard Deskflow client installation.

## Solution
Create a hardware bridge using Pico 2 W that converts Deskflow protocol to Bluetooth LE HID, allowing PC to control mobile devices wirelessly through a USB-connected bridge device.

The software portion runs entirely inside `deskflow-core` and integrates with the existing client/server stack. All new behavior must keep protocol compatibility with the upstream server.

### Terminology & Roles
- **GUI** — `deskflow` executable, responsible for configuration and link management.
- **Server** — Modified Deskflow server.
- **Client** — Modified bridge client, aka Bridge client
- **Link** — USB CDC connection to Pico 2 W.
- **Upstream Server / Client** — Original Deskflow components.
- **Pico 2 W** — Remote MCU translating Deskflow HID frames to Bluetooth LE HID.
- **Mobile device** — iPad/iPhone/Android receiving BLE HID input from Pico 2 W.

Compatibility requirements:
- Server remains compatible with upstream clients.
- Bridge client must explicitly reject upstream servers during handshake.

### Code Generation Principles
1. Keep upstream code changes minimal to ease rebasing.
2. Prefer subclassing or implementing existing interfaces instead of altering upstream base classes.
3. Confirmed new artifacts:
   - `BridgeClientApp` derived from `ClientApp`, owns the Link, lives in `src/lib/client/`.
   - `BridgePlatformScreen` derived from `PlatformScreen`, lives in `src/lib/platform/bridge/`.
   - Additional derived components can be added as the design evolves.

## Requirements

### 1. deskflow (GUI) - Server Only
- GUI launches the server once at startup
- GUI dynamically spawns bridge clients as USB devices are plugged in
- GUI blocks duplicate GUI instances (unchanged)
- Reason: GUI is for server configuration and USB device management only

### 2. deskflow (GUI) - USB Device Event Subscription

#### Implementation Status
**Linux (✓ Complete)**:
- Implemented `LinuxUdevMonitor` using libudev for USB device plug/unplug events
- Monitors "tty" subsystem for USB CDC devices (no root permissions required)
- Filters by vendor ID (2e8a for Raspberry Pi Pico)
- Integrates with Qt event loop using `QSocketNotifier`
- Tracks connected devices to handle removal events (vendor ID unavailable after unplug)
- Bridge Clients GUI section added with dynamic buttons (3 per row):
  - Buttons created/removed automatically on device plug/unplug
  - Fixed size buttons (120-200px width, 32px height)
  - Toggle functionality: click to enable/disable (grayed text when disabled)
  - Button label shows device path (e.g., "/dev/ttyACM0")
  - State tracked per device for future bridge client process management

**Windows/macOS (Pending)**:
- Windows: `WM_DEVICECHANGE` (already used in MSWindowsScreen.cpp)
- macOS: IOKit notifications (IOKit already imported in platform code)

#### Implementation Status (✓ Complete)

**Bridge Client Configuration System**:
- Bridge client configs stored in `~/.config/deskflow/bridge-clients/<name>.conf`
- GUI loads all existing configs on startup
- Config contains: serial number, screen dimensions, orientation, screen name, log level
- Serial number used to match USB devices to config files
- Vendor filter: only USB CDC devices with vendor ID 2e8a (Raspberry Pi) generate configs
- Config file names mirror the bridge screen name (default `Bridge-<tty>`), preventing duplicate files when the client launches
- Widgets created for each config (grayed out if device not plugged in)
- Configuration dialog allows editing settings and renaming config files

**Bridge Client Widget Management**:
- Widget tracking: `QMap<QString, BridgeClientWidget*>` keyed by config path
- Device availability: Widgets enable/disable based on USB device presence
- Serial number matching: Devices matched to configs via serial number from sysfs
- Dynamic state: Widgets gray out when device unplugged, enable when plugged in
- Connect/Disconnect buttons: Toggle to start/stop bridge client processes
- Configure button: Always enabled, opens configuration dialog

**Process Management (✓ Complete)**:
- Process tracking: `QMap<QString, QProcess*>` keyed by device path
- Connection timeout: 5-second timer tracks connection success
- Connection detection: Monitors stdout/stderr for "connected to server" messages
- Graceful shutdown: Uses `terminate()` (SIGTERM) first, waits 3 seconds for clean CDC device release
- Forced shutdown: Uses `kill()` (SIGKILL) if graceful shutdown fails after 1 second
- Device disconnect handling: Automatically terminates running process when USB device unplugged
- Process cleanup: Removes from tracking maps, deallocates QProcess and QTimer objects

**Command Line Arguments**:
- Generated command includes:
  - `--name <screenName>` - from config file
  - `--link <devicePath>` - USB CDC device path (e.g., /dev/ttyACM0)
  - `--remoteHost <host>:<port>` - server address and port
  - `--secure <true|false>` - follows server's TLS setting automatically
  - `--log-level <level>` - from config file (e.g., INFO, DEBUG)
  - `--screen-width <width>` - from config file
  - `--screen-height <height>` - from config file
  - `--screen-orientation <orientation>` - from config file (landscape/portrait)

**Device Event Handling**:
- On USB device plug:
  1. Read serial number from sysfs (`/sys/class/tty/ttyACMx/.../serial`)
  2. Find matching config file by serial number
  3. Create config if none exists (default 1920x1080 landscape)
  4. Enable corresponding widget
  5. Store device path → serial number mapping for disconnect handling
- On USB device unplug:
  1. Retrieve serial number from stored mapping (sysfs gone after unplug)
  2. Find matching widget by serial number
  3. Terminate running bridge client process gracefully
  4. Gray out widget to indicate device unavailable
  5. Clean up serial number mapping

**Files Modified**:
- `src/lib/gui/MainWindow.h` - Added process/timer tracking maps, new slot declarations
- `src/lib/gui/MainWindow.cpp` - Implemented process lifecycle management
- `src/lib/gui/widgets/BridgeClientWidget.h/cpp` - Widget state management
- `src/lib/gui/devices/UsbDeviceHelper.h/cpp` - Serial number reading, device enumeration
- `src/lib/gui/core/BridgeClientConfigManager.h/cpp` - Config file management
- `src/lib/gui/dialogs/BridgeClientConfigDialog.h/cpp` - Configuration dialog

**CDC Device Release Strategy**:
When disconnecting a bridge client, the GUI:
1. Calls `QProcess::terminate()` which sends SIGTERM on Unix
2. This allows deskflow-core bridge client to receive the signal and cleanly:
   - Close the serial port file descriptor
   - Release any file locks on `/dev/ttyACMx`
   - Flush pending I/O operations
3. Waits up to 3 seconds for graceful exit
4. If still running, sends SIGKILL and waits 1 second for forced termination
5. Cleans up QProcess and QTimer objects

This ensures the USB CDC device is properly released and available for reconnection without requiring a physical replug.

### 3. deskflow-core - Multiple Instance Support
- Allow multiple instances based on role + name
- Shared memory key strategy:
  - Server: `"deskflow-core-server"` (only 1 allowed)
  - Client: `"deskflow-core-client-<name>"` (multiple allowed, each unique)
- Implementation approach in `deskflow-core.cpp`:
  1. Early lightweight argument pre-parse to extract `--name` and detect client/server mode before QSharedMemory creation
  2. Build shared memory key based on role:
     - Client: `"deskflow-core-client-<name>"`
     - Server: `"deskflow-core-server"`
  3. Create QSharedMemory with the constructed key for single-instance check
  4. Full argument parsing with `CoreArgParser` after single-instance check passes
  5. Set per-instance QSettings file path for clients: `settings/<name>.conf`
- Reason: Enable 1 server + N clients on same machine without instance blocking

### 4. Special USB-HID Bridge Client Implementation

#### Overview
A new client type that acts as a bridge: converts Deskflow events → HID reports → Pico 2 W (via USB CDC) → Bluetooth LE HID → Mobile device (iPad/iPhone/Android)

#### Arguments (passed from GUI)
1. `--name <unique-name>`: For multi-instance support
2. `--link <usb cdc device-path>`: USB CDC device for Pico 2 W communication

#### Architecture Flow
```
Deskflow Server (PC)
    ↓ (network)
USB-HID Bridge Client (PC)
    ↓ (convert events to HID reports)
    ↓ (pack as frames via USB CDC)
Pico 2 W (hardware bridge)
    ↓ (Bluetooth LE HID)
Mobile Device (iPad/iPhone/Android)
```

#### Client Initialization Sequence
Performed before the standard `ClientApp` starts:
1. Query Pico 2 W over CDC for **arch** (e.g., `bridge-ios`, `bridge-android`).
   - Map the result to a `BridgePlatformScreen` instance (derived from `PlatformScreen`) that knows which HID layout to emit.
   - Instantiate `BridgeClientApp` (derived from `ClientApp`) with an initialized Link so `createScreen()` can return the bridge screen.
2. Query Pico 2 W for **screen info**: resolution, rotation, physical size (inches), scale factor.
   - Feed this into `BridgePlatformScreen::getShape()` so Deskflow coordinates match the mobile display.
3. Configure TLS by reading cert paths from the server's main settings file; configure remote host using the per-instance settings file so the bridge client connects with the user's configured security settings.

#### Key Differences from upstream Client
- **No local event injection**: Doesn't fake mouse/keyboard events on PC
- **Bridge platform screen**: New `IPlatformScreen` implementation packages `fakeMouse*` and `fakeKey*` calls into HID reports sent over CDC
- **USB CDC transport**: Sends framed HID data to Pico 2 W
- **Screen info supplied by Pico 2 W**: Uses mobile device screen properties, not PC screen
- **Clipboard handling**: `BridgePlatformScreen::setClipboard()` discards clipboard payloads instead of forwarding them over the Link; the server still follows the standard clipboard protocol.
- **Settings isolation**: Each bridge client instance loads a dedicated QSettings file at `~/.config/deskflow/bridge-clients/<client-name>.conf` (created on first run if not exists) and strips TLS keys to avoid leaking preferences back into the main Deskflow configuration.
- **TLS compatibility**: Bridge client reuses the server's TLS certificates/config by reading cert paths from the server's main settings file (no user-facing toggle); when the server requires TLS, connect using the same security level and certificate files.
- **Upstream interop**: Bridge client validates the server identity during handshake (server sends identity command) and refuses connections if the identity is missing or indicates an upstream server to avoid unintended pairings.

### 5. CLI / Argument Handling
- Extend `CoreArgParser` options with:
  - `--link <usb cdc dev>` (string) for the Pico USB CDC device path, eg. /dev/ttyACM0
- The GUI always launches bridge clients once a matching Link is detected, so the presence of `--link` implies bridge mode and client-only execution.
- Bridge client settings are automatically stored at `~/.config/deskflow/bridge-clients/<client-name>.conf` (no override option needed).
- Strip/ignore client-side TLS switches from the parsed option set so they cannot override server-driven behavior.
- Update `deskflow-core` `main()` to:
  1. Inspect arguments for `--link` before the single-instance guard.
  2. Select the shared-memory key and settings file based on `--name` (bridge clients use `bridge-clients/<name>.conf`).
  3. Fetch Pico configuration, instantiate `BridgeClientApp`, and run it.

### 6. Transport / HID Framing
- Bridge client owns a CDC transport helper that:
  - Opens the CDC device and exchanges configuration frames (arch + screen info).
  - Provides `sendHidReport(const HidFrame &frame)` for the platform screen.
- `BridgePlatformScreen` maintains button/modifier state so Deskflow’s “release all keys” logic still works.
- Provide unit coverage or logging hooks to validate HID packets during development.

### 7. Arch Reuse Strategy
- Client-side `Arch` services (time, threading, sockets, mutexes, etc.) reuse upstream implementations unchanged.
- The critical deviation is input routing: instead of injecting OS events through the platform `Arch`, the bridge client packages the synthesized input into HID frames and sends them over the Link to Pico 2 W.
- Pico 2 W parses the frame, applies any host-arch key translations, enqueues the HID report into its BLE stack, and the mobile device receives the resulting mouse/keyboard events.

### 8. TLS Configuration for Bridge Clients

#### Runtime Selection via CLI
Bridge clients no longer inspect the server configuration file for TLS settings. Instead:

- The GUI/launcher passes `--secure <true|false>` to `deskflow-core`.
- `CoreArgParser` writes the selected value into `Settings::Security::TlsEnabled` before the bridge client starts.
- `BridgeSocketFactory` reads this flag and:
  - Uses **SecurityLevel::PeerAuth** (with fingerprint verification) when `--secure true`.
  - Uses **SecurityLevel::PlainText** when `--secure false`.

This keeps TLS selection explicit and avoids persisting stale data in bridge configs.

#### Fingerprint Handling
- When running in `SecurityLevel::PeerAuth`, bridge clients validate the server certificate fingerprint.
- The first secure connection performs TOFU: the fingerprint is stored in `~/.config/deskflow/tls/trusted-servers`, creating parent directories if needed.
- Subsequent connections require the fingerprint to match; mismatches abort the TLS session.
- `trusted-clients` remains unused by bridge clients (still server-side only).

#### Configuration Files
- Bridge client `.conf` files no longer contain a `[security]` section; TLS decisions are entirely CLI-driven.
- Server-side `security/checkPeerFingerprints` continues to control whether the server listener demands client certificates (`SecurityLevel::PeerAuth`) or just encryption (`SecurityLevel::Encrypted`).

#### Directory Layout
- **Server settings**: `~/.config/deskflow/deskflow.conf`
- **Bridge client settings**: `~/.config/deskflow/bridge-clients/<client-name>.conf`
- **Shared TLS directory**: `~/.config/deskflow/tls/` containing `deskflow.pem`, `trusted-servers`, and (optionally) `trusted-clients`.
