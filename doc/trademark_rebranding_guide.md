# Deskflow to DShare-HID Rebranding Guide

This document provides a comprehensive log of all changes made to rebrand the **Deskflow** project to **DShare-HID**. It compares the state from commit `bf322cc059` (and immediately prior history) to the current `HEAD`.

Use this guide to understand the scope of divergence from upstream and to assist in resolving conflicts when pulling changes from `deskflow/deskflow`.

---

## 1. Executive Summary: Identity & Keywords

The following core identity elements have been replaced throughout the codebase.

| Component          | Upstream (Deskflow)            | Rebranded (DShare-HID)              | Location(s)                                                  |
| :----------------- | :----------------------------- | :---------------------------------- | :----------------------------------------------------------- |
| **Project Name**   | `deskflow`                     | `dshare-hid`                        | `CMakeLists.txt` `project()`                                 |
| **Proper Name**    | `Deskflow`                     | `DShare-HID`                        | `CMAKE_PROJECT_PROPER_NAME` (CMake), UI Titles, About Dialog |
| **Bundle ID**      | `io.github.deskflow`           | `io.github.lockekk.dshare-hid`      | `Constants.h`, Info.plist, Icons                             |
| **Repository URL** | `github.com/deskflow/deskflow` | `github.com/lockekk/dshare-hid`     | `UrlConstants.h`, `CMakeLists.txt`                           |
| **Organization**   | `Deskflow Developers`          | (Preserved or Updated contextually) | License Headers, Docs                                        |
| **Executable**     | `deskflow`                     | `dshare-hid`                        | CMake target definitions                                     |
| **Icon Theme**     | `deskflow-light`               | `dshare-hid-light`                  | `src/apps/res/icons/`                                        |

---

## 2. Directory & File Structure Changes

This section lists every file that was renamed, added, or deleted to support the rebranding.

### A. Icon Assets (Renamed)
The entire icon theme directory was renamed. All files inside `src/apps/res/icons/deskflow-light` were moved to `src/apps/res/icons/dshare-hid-light`.

| Original Path (Deskflow)                                         | New Path (DShare-HID)                                              |
| :--------------------------------------------------------------- | :----------------------------------------------------------------- |
| `src/apps/res/icons/deskflow-light/index.theme`                  | `src/apps/res/icons/dshare-hid-light/index.theme`                  |
| `src/apps/res/icons/deskflow-light/actions/24/list-add.svg`      | `src/apps/res/icons/dshare-hid-light/actions/24/list-add.svg`      |
| `src/apps/res/icons/deskflow-light/actions/24/list-remove.svg`   | `src/apps/res/icons/dshare-hid-light/actions/24/list-remove.svg`   |
| `~` (and 20+ other action icons)                                 | `~` (Corresponding path in dshare-hid-light)                       |
| `src/apps/res/icons/deskflow-light/devices/64/video-display.svg` | `src/apps/res/icons/dshare-hid-light/devices/64/video-display.svg` |
| `src/apps/res/icons/deskflow-light/places/64/user-trash.svg`     | `src/apps/res/icons/dshare-hid-light/places/64/user-trash.svg`     |
| `src/apps/res/icons/deskflow-light/status/64/dialog-*.svg`       | `src/apps/res/icons/dshare-hid-light/status/64/dialog-*.svg`       |
| `src/apps/res/icons/deskflow-light/status/64/security-*.svg`     | `src/apps/res/icons/dshare-hid-light/status/64/security-*.svg`     |

### B. New Assets (Added)
New application icons were added to match the new Bundle ID.

*   `src/apps/res/icons/dshare-hid-light/apps/64/io.github.lockekk.dshare-hid.svg`
*   `src/apps/res/icons/dshare-hid-light/apps/64/io.github.lockekk.dshare-hid-symbolic.svg`

### C. Translation Files (Renamed)
All localization files were renamed to `dshare-hid_<lang>.ts`.

| Original File                    | New File                           |
| :------------------------------- | :--------------------------------- |
| `translations/deskflow_en.ts`    | `translations/dshare-hid_en.ts`    |
| `translations/deskflow_es.ts`    | `translations/dshare-hid_es.ts`    |
| `translations/deskflow_it.ts`    | `translations/dshare-hid_it.ts`    |
| `translations/deskflow_ja.ts`    | `translations/dshare-hid_ja.ts`    |
| `translations/deskflow_ru.ts`    | `translations/dshare-hid_ru.ts`    |
| `translations/deskflow_zh_CN.ts` | `translations/dshare-hid_zh_CN.ts` |

---

## 3. Source Code Modifications

The following files were modified in-place. The modification usually involves replacing the "Keywords" listed in Section 1.

### Configuration & Build System
*   **`CMakeLists.txt`** (Root):
    *   Updated `project(dshare-hid ...)` metadata matches new name and version.
    *   Updated `HOMEPAGE_URL` to `https://github.com/lockekk/dshare-hid`.
*   **`src/lib/common/CMakeLists.txt`**: Updated library definitions if applicable.
*   **`translations/CMakeLists.txt`**: Updated to generate `dshare-hid_*.qm` files instead of `deskflow_*.qm`.
*   **`src/lib/gui/CMakeLists.user.cmake`**: References to new icon paths.

### Core Logic & Constants
*   **`src/lib/common/Constants.h.in`**:
    *   Replaced `Deskflow` with `DShare-HID` in application versioning strings.
*   **`src/lib/common/UrlConstants.h`**:
    *   `kOrgDomain`: `github.com/lockekk/dshare-hid`
    *   `kUrlApp`: `https://github.com/lockekk/dshare-hid`
    *   `kUrlUpdateCheck`: Points to `lockekk/dshare-hid-release` repo.

### GUI & Application Logic
The following files were updated to display "DShare-HID" in the user interface (Window titles, Labels, Dialogs, Messages).

*   **About Dialog**:
    *   `src/lib/gui/dialogs/AboutDialog.cpp` & `.ui` & `.h`
*   **Main Window**:
    *   `src/lib/gui/MainWindow.cpp`: Window title, System tray tooltips, Status messages ("DShare-HID is running").
    *   `src/lib/gui/DeskflowHidExtension.cpp`: "DShare-HID Extension" strings.
*   **Bridge Client**:
    *   `src/lib/client/BridgeClientApp.cpp`: Client application logging and naming.
    *   `src/lib/client/BridgeSocketFactory.cpp`: Connection logic logging.
    *   `src/lib/gui/widgets/BridgeClientWidget.cpp`
    *   `src/lib/gui/core/BridgeClientConfigManager.cpp`
*   **Platform Specific**:
    *   `src/lib/gui/devices/MacUsbMonitor.mm` (macOS HID monitoring logs)
    *   `src/lib/gui/devices/WindowsUsbMonitor.cpp` (Windows HID monitoring logs)
    *   `src/lib/platform/bridge/HidFrame.cpp`

### Translations Content
*   **`translations/languages/*.json`**:
    *   All values updated: "Deskflow" &rarr; "DShare-HID".
    *   All keys updated: "Deskflow" &rarr; "DShare-HID" (preserving namespaces).
*   **`translations/*.ts`**:
    *   Content updated by `lupdate` to reflect source code changes.
    *   Manual cleanup of obsolete "Deskflow" keys.

---

## 4. Guide for Resolving Upstream Conflicts

When interacting with the upstream `deskflow/deskflow` repository, use the following strategies:

### Scenario A: Upstream Updates Translations
*   **Issue**: Upstream modifies `translations/deskflow_fr.ts`. You have `translations/dshare-hid_fr.ts`.
*   **Git Behavior**: Git may see this as a "Modify vs Delete" conflict because we renamed the file.
*   **Resolution**:
    1.  Inspect upstream's changes to `deskflow_fr.ts`.
    2.  Manually apply the *new* translations to your `dshare-hid_fr.ts`.
    3.  Discard the re-introduction of `deskflow_fr.ts`.

### B. Peer-to-Peer Merge Mappings (The "Link" Table)
Use this table to identify which rebranded file corresponds to an upstream file during a "Modify vs. Delete" conflict.

| Upstream Path (Deskflow)                          | Rebranded Path (DShare-HID)                              |
| :------------------------------------------------ | :------------------------------------------------------- |
| **Linux Deployment**                              |                                                          |
| `deploy/linux/org.deskflow.deskflow.metainfo.xml` | `deploy/linux/io.github.lockekk.dshare-hid.metainfo.xml` |
| `deploy/linux/org.deskflow.deskflow.desktop`      | `deploy/linux/io.github.lockekk.dshare-hid.desktop`      |
| `deploy/linux/org.deskflow.deskflow.png`          | `deploy/linux/io.github.lockekk.dshare-hid.png`          |
| `deploy/linux/flatpak/org.deskflow.deskflow.yml`  | `deploy/linux/flatpak/io.github.lockekk.dshare-hid.yml`  |
| **macOS Deployment**                              |                                                          |
| `deploy/mac/Deskflow-HID.entitlements`            | `deploy/mac/DShare-HID.entitlements`                     |
| **Translations**                                  |                                                          |
| `translations/deskflow_<lang>.ts`                 | `translations/dshare-hid_<lang>.ts`                      |
| **Icons & Assets**                                |                                                          |
| `src/apps/res/icons/deskflow-light/`              | `src/apps/res/icons/dshare-hid-light/`                   |
| `src/apps/res/deskflow.ico`                       | `src/apps/res/dshare.ico`                                |
| `src/apps/res/deskflow.qrc`                       | `src/apps/res/dshare-hid.qrc`                            |

---

### C. Meticulous Porting Workflow (Detailed)

### Scenario B: Upstream Adds New Icons
*   **Issue**: Upstream adds `src/apps/res/icons/deskflow-light/new-icon.svg`.
*   **Resolution**:
    1.  Move the new file to `src/apps/res/icons/dshare-hid-light/new-icon.svg`.
    2.  Remove the `deskflow-light` directory if it was recreated.

### Scenario C: String Conflicts in C++
*   **Issue**: Upstream changes `MainWindow.cpp` line 100: `setWindowTitle("Deskflow v" + version)`. We have `setWindowTitle("DShare-HID v" + version)`.
*   **Resolution**:
    1.  Keep our change ("DShare-HID").
    2.  Check if upstream added *logic* around the string (e.g. dynamic versioning) and apply that logic while preserving the name "DShare-HID".

### Scenario D: CMake Conflicts
*   **Issue**: Upstream updates dependencies in `CMakeLists.txt`.
*   **Resolution**:
    1.  Accept dependency updates.
    2.  **ENSURE** `project(dshare-hid ...)` remains. Do not revert to `project(deskflow ...)`.
    3.  **ENSURE** `HOMEPAGE_URL` remains `lockekk/dshare-hid`.
### Scenario E: README.md Conflicts
*   **Issue**: Upstream updates `README.md` with new features, community links, or documentation.
*   **Resolution**:
    1.  **Ignore** upstream changes to `README.md`.
    2.  **NEVER** let upstream changes overwrite the local `README.md`.
    3.  If critical information is added upstream, manually port it to the local `README.md` while maintaining the **DShare-HID** branding and structure.

### Scenario F: .gitignore Conflicts
*   **Issue**: Upstream adds new ignore patterns to `.gitignore`.
*   **Resolution**:
    1.  **Keep both changes**.
    2.  Merge the upstream additions with the local additions.
    3.  Ensure that project-specific ignores for **DShare-HID** are preserved.

### Scenario G: Configuration Documentation Updates
*   **Issue**: Upstream updates `doc/user/configuration.md` with clearer wording or new configuration keys.
*   **Resolution**:
    1.  **Adopt the logic/wording Improvements**: Upstream often clarifies how settings work. Use their new descriptions.
    2.  **STRICT BRANDING**: Manually replace all instances of "Deskflow" or "deskflow" in the new text with **DShare-HID** or **dshare-hid**.
    3.  **Preserve Layout**: Ensure tables and links match the DShare-HID structure.

### Scenario H: Daemon & Process Naming Regressions
*   **Issue**: Upstream modifies `daemonName()` or similar methods in `ClientApp.cpp`, `ServerApp.cpp`, or `MainWindow.cpp`.
*   **Git Behavior**: Often auto-merges successfully but reintroduces the string "Deskflow".
*   **Resolution**:
    1.  **POST-MERGE SWEEP**: Always run a grep for "Deskflow" in the source code after a merge.
    2.  **Restore Identity**: Ensure `daemonName()` returns "DShare-HID Client/Server" (Windows) and "dshare-hid-client/server" (Unix).
    3.  **Check Titles**: Ensure window titles and tray icons continue to use the **DShare-HID** constants.
