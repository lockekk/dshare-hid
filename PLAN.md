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

#### Planned Behavior
- On matched vendor USB CDC device plug:
  - Open Pico configuration window (fetch/configure arch, screen info)
  - GUI directly communicates with Pico via USB CDC
  - Configuration stored in Pico's flash memory
  - After config window closed, spawn `deskflow-core client --name <unique-name> --link <usb cdc device-path> localhost:<port>`
- On matched vendor USB CDC device unplug:
  - Kill corresponding bridge client process
- Reason: Automatically create/destroy clients based on physical device presence

Note: Pico configuration is a GUI feature (not part of deskflow-core), bridge client only reads existing config from Pico on startup

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

#### Automatic TLS Configuration
Bridge clients automatically follow the server's TLS configuration without requiring manual setup:

**Implementation**:
- `BridgeSocketFactory` reads `security/tlsEnabled` from server's main config (`~/.config/deskflow/deskflow.conf`)
- When server has TLS enabled, bridge clients use **SecurityLevel::Encrypted** (NOT PeerAuth)
- When server has TLS disabled, bridge clients use **SecurityLevel::PlainText**
- No `[security]` section needed in bridge client config files

**Security Level Details**:
- **SecurityLevel::Encrypted**: TLS encryption without certificate fingerprint verification
- **SecurityLevel::PeerAuth**: TLS encryption with certificate fingerprint verification (used by upstream clients)
- Bridge clients always use Encrypted (not PeerAuth) for simplicity and convenience

#### Settings Directory Structure
Bridge clients use isolated settings under the server's config directory:
- **Server settings**: Uses default location (e.g., `~/.config/deskflow/deskflow.conf`)
- **Bridge client settings**: `~/.config/deskflow/bridge-clients/<client-name>.conf` (e.g., `~/.config/deskflow/bridge-clients/my-pico.conf`)
- **Shared TLS directory**: `~/.config/deskflow/tls/` (shared between server and all bridge clients)

#### TLS Files
The `~/.config/deskflow/tls/` directory contains:
1. **`deskflow.pem`**: Server's self-signed certificate (contains both private key and certificate)
2. **`trusted-servers`**: Not used by bridge clients (no fingerprint verification)
3. **`trusted-clients`**: Not used by bridge clients

#### Setup Process for Bridge Clients
**No manual TLS setup required!** Bridge clients automatically:
1. Read TLS setting from server's config
2. Use server's certificate from `~/.config/deskflow/tls/deskflow.pem`
3. Connect with appropriate security level (Encrypted or PlainText)

#### Why This Design?
- **Automatic configuration**: No manual TLS setup for bridge clients
- **Simplified security**: Uses Encrypted (not PeerAuth) to avoid fingerprint management
- **Shared TLS directory**: All bridge clients use the same server certificate
- **Per-client settings**: Each bridge client maintains its own non-TLS configuration (screen name, log level, etc.)
- **Convenience**: Bridge clients "just work" when server has TLS enabled
