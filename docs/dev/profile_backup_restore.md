# Exporting and Restoring Bridge Profiles

Users should be able to save a bridge device's profiles to a file and load them back later: after a full flash wipe, on a replacement dongle, or before a factory reset. Then they don't have to type each profile's hostname, HID mode, resolution, rotation and scroll settings again.

All of this can be built from requests the app already has. It needs no firmware change, no new CDC command and no new bridge core request.

## In the app

The Configure dialog has **Backup...** and **Restore...** at the top of its Profiles group. They act on all of the device's profiles, so they sit by the group's title, away from Reset, which acts on the profile shown.

Only Backup reads the device straight away. Everything else in the Profiles group changes the form, and the dialog sends it to the device on OK (see [Saving on OK](#saving-on-ok)): edits, Reset, a restored backup, and the profile shown becoming the active one.

| Part | Where |
|---|---|
| File format and its checks | `BridgeProfileBackup` (`src/lib/gui/core/BridgeProfileBackup.h`) |
| Reading and writing the device | `BridgeDeviceService::backupProfiles()` and `writeProfiles()` |
| Buttons, file dialogs, messages | `BridgeClientConfigDialog::onProfileBackupClicked()`, `onProfileRestoreClicked()` and `applyProfiles()` (OK) |
| Tests | `src/unittests/bridge/BridgeProfileBackupTests.cpp` |

A backup holds what is stored on the device. Changes in the dialog that haven't been sent with OK aren't included.

## What a backup holds

| Saved | Not saved |
|---|---|
| Each profile that has been set: hostname, HID mode, width, height, rotation, invert, speed, Y scroll scale | Bluetooth pairings. The CDC protocol can't read them; they stay in the firmware's Bluetooth storage. |
| The active profile index | Activation state and profile limit |
| The device name (optional) | |

After a restore onto a wiped or different device, each host has to forget "Deskflow HID" and pair again, with that host's profile active. The app doesn't say so for a restore: its messages stay short. It does say so for a Reset, which always removes the profile's pairing.

## Calls to use

Use `BridgeDeviceService` (`src/lib/gui/core/BridgeDeviceService.h`). Each call goes through the device's running bridge client when one owns the port, and otherwise opens the port itself. So export and restore work whether or not a bridge client is running for the device.

| Step | Call | Request it sends |
|---|---|---|
| Read all profiles and the active index | `readProfiles(devicePath, profileSet, error)` | `profiles` |
| Write one profile | `saveProfile(devicePath, index, profile, profileSet, error)` | `save <index> <profile>` |
| Make a profile active | `switchProfile(devicePath, index, profileSet, error)` | `switch <index>` |
| Read the device name | `fetchFirmwareDeviceName(devicePath, name)` | `getname` |
| Write the device name | `applyFirmwareDeviceName(devicePath, name)` | `setname <name>` |

Each profile call replies with the full profile list and the active index (`BridgeProfileSet`), so you never need an extra read after a write.

For reference, these map onto the firmware's CDC controls `GET_PROFILE` (0x32), `SET_PROFILE` (0x33) and `SWITCH_PROFILE` (0x31) in `CdcTransport`. The bytes on the wire are `DeviceProfile` (52 bytes, `src/lib/platform/bridge/CdcTransport.h`).

## Export

1. Call `readProfiles()`. `profileSet.profiles` has one entry per index, from 0 to `totalProfiles - 1`; `totalProfiles` comes from the HELLO ACK.
2. Skip the profiles that were never set. The firmware never reports an unset profile as missing; it returns this default instead:

   | Field | Default for index N |
   |---|---|
   | hostname | `"Slot N"` |
   | slot | N |
   | hidMode | 0 |
   | screenWidth × screenHeight | 1080 × 2424 |
   | rotation, invert | 0, 0 |
   | speed | 30 |
   | yScrollScale | 0 |

   A profile equal to this in every field is unset. Skipping it loses nothing: restoring the default gives the same result as leaving the profile unset.
3. Optionally call `fetchFirmwareDeviceName()`. It returns the stored name ("Deskflow HID" by default). The firmware only advertises the stored name once the device is activated. During the trial it advertises "Deskflow HID(trial)" whatever is stored, so the stored name is still the right one to save.
4. Check the file before saving it: the app reads back what it is about to write with the restore checks (below). A profile that fails them, such as one set by the script with a hostname the dialog doesn't allow, stops the backup with a message naming the profile, so the user can correct it and save it to the device first. The app never saves a file that it would refuse to restore.
5. Write the file (format below) with `QSaveFile`, so a failed write never leaves half a file.

## Restore

Restore loads a backup into the dialog. Nothing is sent to the device until OK.

1. Read the file and check every value in it (see [Checks before anything is written](#checks-before-anything-is-written)). If anything fails, list the problems and stop. The form is left as it was.
2. **Check the range.** If the file has a profile index, or an `active_profile`, at or above the device's profile limit (the number of tabs, from `readProfiles()`), stop and tell the user. A trial device has 2 profiles, so a backup from an activated device with more can't fully fit. Don't load part of it silently.
3. Put each of the backup's profiles in the form, as an edit of that profile. Profiles not in the backup stay as they are. A Reset not yet sent for one of the backup's profiles is dropped.
4. Show the tab of the backup's `active_profile`, so OK makes it active.
5. If the file has `device_name`, put it in the Device name field, so OK sets it like a typed name: firmware, `.conf` file and main window together. If the device isn't activated, the field is disabled, so the name is skipped.
6. Say how many profiles were loaded, and that OK applies them.

## Saving on OK

The dialog keeps the device's profiles as it last read them, and notes which profiles the user edits; filling in the form to show a profile isn't an edit. On OK (`applyProfiles()`), once the screen and device names pass their checks:

1. Find the changes: profiles marked by Reset, edited profiles that differ from the device's (for a reset one, from the firmware's default), a profile shown other than the active one, and a new device name. With none, OK just closes.
2. Check the profiles to save as a restore checks them. If any fails, list the problems and stay open.
3. List the changes and wait for **Apply**. Cancel keeps the dialog open. The list is short:
   - Reset Pixel. Its host must pair again manually.
   - Save Pixel, Ipad air.
   - Active profile changes to Ipad air.
   - Device name changes to Desk Bridge.
4. Make the changes in that order:
   - Erase each reset profile (`eraseProfile()`). The firmware removes its pairing and its record, so it reads back as the default.
   - Save the edited profiles and make the profile shown active, with `BridgeDeviceService::writeProfiles()`. For each profile it builds a `DeviceProfile` and calls `saveProfile(index, …)`:
     - Set `slot` to the profile's index. The firmware uses `slot` to choose the Bluetooth address and pairing slot for the profile; it always equals the index.
     - Copy `hostname`, which the checks have limited to 31 ASCII characters, and fill the rest of the field with NULs.
     - Set `reserved` to zeros.
     - Compare the profile in the reply with the bytes sent. If they differ, stop: the device didn't store what was written.

     Then it calls `switchProfile()` only if the last reply's active index isn't the profile shown, and checks the reply's active index. With nothing to save, the dialog calls `switchProfile()` itself if the active profile changes.
   - The device name is set after the dialog closes, as before.

`writeProfiles()` runs the checks again on the profiles it is given, whoever built them, and writes only the profiles those checks produce. If a step fails, nothing can be rolled back: the dialog shows the error and stays open with the changes not yet made, and OK tries them again.

### Checks before anything is written

The firmware stores any 52 bytes it is sent as a profile: `SET_PROFILE` only checks the index against the profile limit. So the app checks each value before sending it. A file is restored completely or not at all. One bad value rejects the whole file, and every problem is listed, not just the first.

| Key | Accepted | Why |
|---|---|---|
| `index` | Whole number, 0–14, once per file; below the device's limit | The HELLO ACK reports the profile count in 4 bits |
| `slot` (optional) | Equal to `index` | The firmware picks the Bluetooth address and pairing by the slot |
| `hostname` | 1–31 characters: A–Z, a–z, 0–9, space, `.`, `-`, `_`; not only spaces | The Configure dialog's hostname rule. A longer or other hostname is rejected, not cut. `switchProfile()` fails for an empty one |
| `hid_mode` | 0 or 1 | Combo or mouse only. At boot the firmware sets the active profile's mode inside `ESP_ERROR_CHECK` |
| `width`, `height` | Whole numbers, 100–10000 | The dialog's resolution boxes |
| `rotation` | 0 or 1 | Portrait or landscape |
| `invert` | 0 or 1 | |
| `speed` | Whole number, 0–255 | The legacy scroll speed; the core reads 0 as 120 |
| `y_scroll_scale` | Whole number, 0–100 | Tenths: 1–100 is 0.1–10.0, as in the dialog; 0 is the default, 1.0 |
| `active_profile` | Whole number, 0–14; below the device's limit | |
| `device_name` (optional) | Passes `isValidDeviceName()` (1–22 characters: A–Z, a–z, 0–9, space, `.`, `-`, `_`) and isn't only spaces | The dialog's device name rule |

Numbers must be JSON numbers. A string such as `"0"`, a boolean or a fraction is rejected. The file must be a JSON object with a `profiles` list of at least one profile, and at most 64 KB. Keys the app doesn't know are ignored.

Profiles that are set on the device but missing from the file stay as they are. If the UI needs "replace everything", erase those with `eraseProfile()`, and warn first: erasing a profile also removes its Bluetooth pairing.

### Side effects to expect

- Saving the active profile, or switching profiles, makes the firmware re-apply that profile. It disconnects the connected host, restarts advertising, and the host reconnects about a second later. If the active profile is saved and then switched to, that's two reconnects, which is why step 4 only switches when it has to.
- `saveProfile()` and `switchProfile()` already count as profile changes for the bridge client (`requests::changesActiveProfile()`), so the running client picks up the new screen size and scroll settings by itself.
- `switchProfile()` fails for a profile whose hostname is empty. Profiles written by this flow always have a hostname.

## File format

Use the same JSON as `scripts/usb_cdc_test.py backup-profiles` / `restore-profiles` in the esp32-hid repo, so files from the app and the script can be used with either:

```json
{
  "active_profile": 1,
  "device_name": "Desk Bridge",
  "profiles": [
    {
      "index": 0,
      "hostname": "Pixel",
      "hid_mode": 0,
      "width": 1080,
      "height": 2424,
      "rotation": 0,
      "invert": 0,
      "speed": 30,
      "y_scroll_scale": 10
    },
    {
      "index": 1,
      "hostname": "Ipad air",
      "hid_mode": 1,
      "width": 1080,
      "height": 2424,
      "rotation": 0,
      "invert": 0,
      "speed": 30,
      "y_scroll_scale": 10
    }
  ]
}
```

- `profiles` lists only the profiles that were set, each with its `index`. There is no `slot` field, since it always equals `index`.
- `device_name` is the app's addition, and it's optional. The script ignores it on restore and doesn't write it, so the app must accept files without it.
- The app writes the keys of each object in alphabetical order with 4-space indents (`QJsonDocument`). The script keeps the order above with 2-space indents. The data is the same, so compare the two with `jq -S` rather than `diff`.

## Testing against the firmware script

`scripts/usb_cdc_test.py` in the esp32-hid repo uses the same commands. Close DShare-HID before running it, because it needs the CDC port.

```bash
python3 scripts/usb_cdc_test.py --port <port> backup-profiles before.json   # script export
# app: restore before.json, then export after.json
python3 scripts/usb_cdc_test.py --port <port> backup-profiles check.json
diff before.json check.json                                                  # must be identical
```

Check these:

1. An app export of a device configured by the script matches the script's export, ignoring `device_name`: `diff <(jq -S 'del(.device_name)' app.json) <(jq -S . script.json)`.
2. The app restores a script export onto a wiped device; `backup-profiles` then matches the original.
3. A backup with an index at or above the device's limit is rejected before anything is written.
4. Restoring while a bridge client runs for the device works, and the client takes on the new active profile's screen size.
5. A file with one bad value, such as `"hid_mode": 2`, is rejected with that problem named, and `backup-profiles` afterwards shows the device unchanged.

Unit tests, in `src/unittests/bridge/` (configure with `-DBUILD_TESTS=ON` to build them):

- `BridgeProfileBackupTests`: the file format and its checks, one row per kind of bad value.
- `BridgeDeviceServiceTests`: `backupProfiles()` and `writeProfiles()` against `FakeBridgeDevice`, which answers requests as the firmware does (set through `BridgeDeviceService::setRequestHandler()`).
- `BridgeClientConfigDialogTests`: the Configure dialog against the same fake: edits, Reset, Backup, Restore and the summary on OK, with each message box and file dialog answered by the test.

## Re-pairing after an upgrade from firmware before 1.2.1

Firmware 1.2.1 resets every Bluetooth pairing once, on its first boot. The firmware can't reach the hosts, so the app has to tell the user to pair them again.

### Why the pairings are reset

| Firmware | Bluetooth address |
|---|---|
| Before 1.2.1 (every release up to 1.2.0) | Every profile advertises under the dongle's one public address. |
| 1.2.1 and later | Each profile advertises under its own static random address, made from the Bluetooth MAC (top two bits set, last byte XOR the profile index). A host sees the dongle only while its own profile is active. |

A host paired before 1.2.1 is bonded to the public address, which the dongle never uses again. Its bond can't be used from either side.

### What the firmware does

- On the first boot of 1.2.1 or later, it removes every bond and every profile's paired-host record once, then stores the marker `ble_addr_ver = 2` in its NVS config. The UART log shows `Bonds predate the per-slot addresses, removing all pairings`.
- It keeps the profiles, the active profile, the device name and the activation state. A backup and restore around this upgrade isn't needed.
- It runs once per device. RESET_DEVICE_CONFIG keeps the marker, so a reset doesn't run it again. It only runs again if the first attempt couldn't read the bond list or store the marker.
- A new device also runs it once, with nothing to remove.
- The hosts keep their old entry for the dongle, and it never connects again. A host that keeps it shows two entries with the same name after pairing again.

### When the app shows the notice

Read the version from the HELLO ACK: `firmware_bcd`, in `FirmwareConfig::firmwareVersionBcd`, is major × 100 + minor × 10 + patch. 1.2.0 is 120 and 1.2.1 is 121. Show the notice once per device, when its version goes from below 121 to 121 or above:

- **After the app's own firmware update** (`Esp32HidToolsWidget`): compare the version before the update with the version once the device is back in the app. Read the "before" version while the device still runs the app, before it goes to factory mode. In factory mode the HELLO ACK has no firmware version (`firmware_bcd` is 0).
- **After an update made elsewhere** (another computer, or `scripts/usb_cdc_test.py`): store the last version seen for each device, by its serial number (`CdcTransport::fetchSerialNumber()`, the MAC that also names the bridge client, such as `Bridge-ACA704D05EA0`). If the stored version is below 121 and the current one is 121 or above, show the notice.
- A device the app first sees at 121 or above gets no notice. The app can't know whether it was ever paired before.
- Once the notice is shown, store that against the serial number, together with the current version, so it doesn't show again.

Development builds before 1.2.1 report 1.0.1 (101). The rule covers them too: they also get the one-time reset when they move to 1.2.1.

### Notice text

> Bluetooth pairings were reset. On each phone, tablet or PC, forget "Deskflow HID" in Bluetooth settings, then pair it again on its profile.

If the notice has room for more detail, give the steps:

1. On each host, remove the dongle's old entry from the Bluetooth settings. The name it shows depends on the device's state: "Deskflow HID(trial)" during the trial, "Deskflow HID(expired)" when the trial has expired, or the device name (default "Deskflow HID") once it's activated.
2. Pair the hosts one at a time. Before pairing each one, make its profile active (switch profile). After the reset no profile has a paired host, so the first host to pair while a profile is active becomes that profile's host.

### Downgrades

The marker stays when older firmware is flashed. If a device goes back below 1.2.1, hosts are paired again there, and the device is then upgraded again, the firmware doesn't reset a second time. The app's rule still shows the notice, because the version went from below 121 to 121 or above. To clear the device's side as well, suggest Unpair All (`CdcTransport::unpairAll()`) before pairing again.

### Testing the notice

1. On a development board, run firmware before 1.2.1 (a development build of esp32-hid `a82a483` reports 1.0.1) and pair a host.
2. Update to 1.2.1 with the app. The notice shows once. `python3 scripts/usb_cdc_test.py handshake` reports `firmware version : 1.21`.
3. Reconnect the device, and restart the app. The notice doesn't show again.
4. A device flashed straight to 1.2.1 shows no notice.
