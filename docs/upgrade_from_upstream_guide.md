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
*   **`src/apps/deskflow-daemon/CMakeLists.txt`**:
    *   Wrapped `install()` commands in `if(INSTALL_DAEMON)` block (default `OFF`) to exclude the daemon from the MSI installation.
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
*   **Issue**: Upstream modifies, adds, or removes files under `translations/` (e.g., `translations/deskflow_<lang>.ts`, `translations/languages/*.json`, or new locales like `deskflow_ko.ts`).
*   **Git Behavior**: Often "Modify vs Delete" conflicts because we renamed the upstream `deskflow_*.ts` files to `dshare-hid_*.ts`.
*   **Resolution**:
    1.  **Discard all upstream `.ts` changes.** `git rm` any re-introduced `translations/deskflow_*.ts` files. Do not hand-port translation strings into the rebranded `dshare-hid_*.ts` files.
    2.  **Skip new locales by default.** If upstream adds a new locale (e.g., Korean `ko`), drop it unless the user explicitly asks to port it. Do not ask — just discard.
    3.  **Do not ask the user** about translations during a merge. The answer is always "discard upstream, keep ours."
*   **Rationale**: Translation `.ts` files are auto-regenerated by Qt's `lupdate` from source-code strings on every build (see `translations/CMakeLists.txt`). Source-code string changes will flow through to the `dshare-hid_*.ts` files automatically the next time the build runs. The user does not review or hand-merge translation content.
*   **Exception**: Only re-engage if the user explicitly asks to add a new locale or port a specific translation.

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
*   **Issue**: Upstream updates `docs/user/configuration.md` with clearer wording, new configuration keys, moved keys, or removed keys.
*   **Why this file matters more than other docs**: `configuration.md` is the user-facing contract for baseline functionality (server/client/options/hotkeys/actions). Changes here usually correspond to *behavioral* changes in the source code (settings keys added, moved between sections, deprecated, semantics changed). Treat this file as an early-warning signal, not just docs.
*   **Resolution**:
    1.  **Adopt the logic/wording improvements**: Upstream often clarifies how settings work. Use their new descriptions.
    2.  **STRICT BRANDING**: Manually replace all instances of "Deskflow" or "deskflow" in the new text with **DShare-HID** or **dshare-hid**.
    3.  **Preserve Layout**: Ensure tables and links match the DShare-HID structure.
    4.  **Diff the keys, not just the prose**: Run `git diff <merge-base>..upstream-master -- docs/user/configuration.md` and enumerate what changed at the *key/option* level:
        - **Added** option → new feature in upstream; check if backing code landed and whether DShare-HID needs UI/Settings work to expose it.
        - **Removed** option → upstream deprecated/removed a feature; verify our code doesn't still rely on it (especially in `BridgeClient*`, `Settings.*`, `ServerConfig.*`).
        - **Moved** option (e.g., `protocol` moved from `[options]` → `[server]` in this last merge) → there's an underlying code refactor; track down the upstream commit (`git log -p -- docs/user/configuration.md`) and verify the corresponding code change merged cleanly into our tree.
        - **Semantic change** → wording change without key change can hide behavior tweaks (e.g., default value flipped, validation added). Read the upstream commit body.
    5.  **ALERT THE USER** when any function/option is **added, removed, moved, or changed in semantics**. Surface the list explicitly with the upstream commit ref and a one-line "this might affect us because ___" note. Do not silently rebrand-and-commit. The user wants the chance to decide whether DShare-HID needs to adopt, ignore, or work around each change.

### Scenario H: Daemon & Process Naming Regressions
*   **Issue**: Upstream modifies `daemonName()` or similar methods in `ClientApp.cpp`, `ServerApp.cpp`, or `MainWindow.cpp`.
*   **Git Behavior**: Often auto-merges successfully but reintroduces the string "Deskflow".
*   **Resolution**:
    1.  **POST-MERGE SWEEP**: Always run a grep for "Deskflow" in the source code after a merge.
    2.  **Restore Identity**: Ensure `daemonName()` returns "DShare-HID Client/Server" (Windows) and "dshare-hid-client/server" (Unix).
    3.  **Check Titles**: Ensure window titles and tray icons continue to use the **DShare-HID** constants.
### Scenario I: Daemon Exclusion from MSI (Windows)
*   **Issue**: Upstream's `src/apps/deskflow-daemon/CMakeLists.txt` automatically installs the daemon, which includes it in the MSI. In this project, the daemon is intentionally excluded from the MSI.
*   **Git Behavior**: Merge conflicts in `src/apps/deskflow-daemon/CMakeLists.txt` when upstream updates `install()` logic.
*   **Resolution**:
    1.  **Keep the `INSTALL_DAEMON` Toggle**: Ensure the `install()` commands remain wrapped in the `if(INSTALL_DAEMON)` conditional.
    2.  **Maintain Defaults**: Do not accidentally enable `INSTALL_DAEMON` or revert to unconditional `install()` calls during conflict resolution.

### Scenario J: Upstream CI/CD Workflows (.github)
*   **Issue**: Upstream adds or modifies files in the `.github/` directory (workflows, actions, templates).
*   **Resolution**:
    1.  **Discard All Changes**: This repository does not use upstream GitHub workflows.
    2.  **Delete the Directory**: If Git re-introduces the `.github` directory during a merge, delete it entirely.
    3.  **Rationale**: Rebranding and custom build logic (e.g., ESP32 HID tools) require a divergent deployment strategy that is incompatible with upstream CI.

### Scenario K: Linux Deployment Files (metainfo.xml)
*   **Issue**: Upstream modifies `deploy/linux/org.deskflow.deskflow.metainfo.xml` with new release notes or translations. Because we use a rebranded file, Git sees the upstream file as deleted/modified and creates a conflict.
*   **Resolution**:
    1.  **Do NOT blindly discard**: Check the diff of the upstream file (`git diff upstream-master^1 upstream-master deploy/linux/org.deskflow.deskflow.metainfo.xml`) to identify what was added (e.g., new `<release>` blocks, new `<summary xml:lang="...">` nodes).
    2.  **Port the changes**: Manually copy the new release descriptions and translation tags from the upstream `org.*` file into our local `deploy/linux/io.github.lockekk.dshare-hid.metainfo.xml`.
    3.  **Discard the original**: Once the changes are safely ported over to the rebranded file, discard the re-introduced upstream `org.deskflow.deskflow.metainfo.xml` by running `git rm`.

### Scenario L: Unit Tests (`src/unittests/`)
*   **Issue**: Upstream renames, adds, deletes, or refactors files under `src/unittests/`. Often follows architectural changes (e.g., `LanguageManagerTests` → `KeyboardLayoutManagerTests`, deleted `ClientConnectionTests`/`ServerConnectionTests` after IPC refactor).
*   **Resolution**:
    1.  **Accept upstream as-is.** Do not hand-merge or audit `src/unittests/` changes during an upstream merge. If a conflict marker appears under this tree, take the upstream version verbatim (`git checkout --theirs <path>`).
    2.  **Do not ask the user** about test renames, deletions, or framework changes — the answer is always "take upstream."
*   **Rationale**: `run_task.sh` configures every build with `-DSKIP_BUILD_TESTS=ON -DBUILD_TESTS=OFF`, and `src/CMakeLists.txt` wraps `add_subdirectory(unittests)` in `if(BUILD_TESTS)`. The unittests tree is never compiled in any release/dev workflow, so its content cannot affect the shipped artifact.
*   **Exception**: Only re-engage with unittests changes if `BUILD_TESTS` is intentionally enabled in `run_task.sh`, or if a unittests change somehow leaks into a non-test target (extremely unlikely given the CMake gating).
