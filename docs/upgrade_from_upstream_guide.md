# Deskflow to DShare-HID Rebranding Guide

This document provides a comprehensive log of all changes made to rebrand the **Deskflow** project to **DShare-HID**. It compares the state from commit `bf322cc059` (and immediately prior history) to the current `HEAD`.

Use this guide to understand the scope of divergence from upstream and to assist in resolving conflicts when pulling changes from `deskflow/deskflow`.

> **Note**: Sections 1–3 are a historical snapshot of the original rebrand.
> For what is *currently* divergent from upstream, Section 6 (Divergence
> Budget) is the source of truth.

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
*   `deploy/mac/DShare-HID.entitlements` (fork-only, added by `bf322cc059` — upstream ships no entitlements file under `deploy/mac/`)

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

### Merge Commit Message Format

*   **Rule**: Use the upstream commit's original subject line as the merge commit's subject line. Do NOT invent summaries like `Merge upstream/master <sha>: <slug>`.
*   **Body**: Preserve the upstream commit's body verbatim (if any). Then append a short trailer line: `Merged from upstream <shortsha>.` If a Scenario was applied, extend that trailer with the scenario letter and one-line reason — e.g. `Merged from upstream 570e68e910. Scenario J: kept our deletion of .github/workflows/continuous-integration.yml.`
*   **Rationale**: `git log --oneline` on `main` should read like the upstream history it tracks. A grep for an upstream subject should hit both the upstream commit and its merge commit on `main`, which is impossible when the merge commit rewrites the subject. Scenario notes belong in the body, not the subject.
*   **Recovery**: If merge commits were already recorded with the wrong subject and haven't been pushed, rebuild them with `git commit-tree` (same tree, same two parents, new message) and update `refs/heads/main` — the reflog preserves the old tips.

### Scenario A: Upstream Updates Translations
*   **Issue**: Upstream modifies, adds, or removes files under `translations/` (e.g., `translations/deskflow_<lang>.ts`, `translations/languages/*.json`, or new locales like `deskflow_ko.ts`).
*   **Git Behavior**: Often "Modify vs Delete" conflicts because we renamed the upstream `deskflow_*.ts` files to `dshare-hid_*.ts`.
*   **Resolution**:
    1.  **Discard all upstream `.ts` changes.** `git rm` any re-introduced `translations/deskflow_*.ts` files. Do not hand-port translation strings into the rebranded `dshare-hid_*.ts` files.
    2.  **Skip new locales by default.** If upstream adds a new locale (e.g., Korean `ko`), drop it unless the user explicitly asks to port it. Do not ask — just discard.
    3.  **Do not ask the user** about translations during a merge. The answer is always "discard upstream, keep ours."
*   **Rationale**: Translation `.ts` files are auto-regenerated by Qt's `lupdate` from source-code strings on every build (see `translations/CMakeLists.txt`). Source-code string changes will flow through to the `dshare-hid_*.ts` files automatically the next time the build runs. The user does not review or hand-merge translation content.
*   **Exception**: Only re-engage if the user explicitly asks to add a new locale or port a specific translation.

### File-Mapping ("Link") Table
Use this table to identify which rebranded file corresponds to an upstream file during a "Modify vs. Delete" conflict.

| Upstream Path (Deskflow)                          | Rebranded Path (DShare-HID)                              |
| :------------------------------------------------ | :------------------------------------------------------- |
| **Linux Deployment**                              |                                                          |
| `deploy/linux/org.deskflow.deskflow.metainfo.xml` | `deploy/linux/io.github.lockekk.dshare-hid.metainfo.xml` |
| `deploy/linux/org.deskflow.deskflow.desktop`      | `deploy/linux/io.github.lockekk.dshare-hid.desktop`      |
| `deploy/linux/org.deskflow.deskflow.png`          | `deploy/linux/io.github.lockekk.dshare-hid.png`          |
| `deploy/linux/flatpak/org.deskflow.deskflow.yml`  | `deploy/linux/flatpak/io.github.lockekk.dshare-hid.yml`  |
| **Translations**                                  |                                                          |
| `translations/deskflow_<lang>.ts`                 | `translations/dshare-hid_<lang>.ts`                      |
| **Icons & Assets**                                |                                                          |
| `src/apps/res/icons/deskflow-light/`              | `src/apps/res/icons/dshare-hid-light/`                   |
| `src/apps/res/deskflow.ico`                       | `src/apps/res/dshare.ico`                                |
| `src/apps/res/deskflow.qrc`                       | `src/apps/res/dshare-hid.qrc`                            |

---

### Per-Scenario Playbook

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
*   **Issue**: Upstream updates `docs/Configuration.md` (was `docs/user/configuration.md` before v1.27.0) with clearer wording, new configuration keys, moved keys, or removed keys.
*   **Why this file matters more than other docs**: `configuration.md` is the user-facing contract for baseline functionality (server/client/options/hotkeys/actions). Changes here usually correspond to *behavioral* changes in the source code (settings keys added, moved between sections, deprecated, semantics changed). Treat this file as an early-warning signal, not just docs.
*   **Resolution**:
    1.  **Adopt the logic/wording improvements**: Upstream often clarifies how settings work. Use their new descriptions.
    2.  **STRICT BRANDING**: Manually replace all instances of "Deskflow" or "deskflow" in the new text with **DShare-HID** or **dshare-hid**.
    3.  **Preserve Layout**: Ensure tables and links match the DShare-HID structure.
    4.  **Diff the keys, not just the prose**: Run `git diff -M <merge-base>..<upstream-ref> -- docs/Configuration.md` and enumerate what changed at the *key/option* level:
        - **Added** option → new feature in upstream; check if backing code landed and whether DShare-HID needs UI/Settings work to expose it.
        - **Removed** option → upstream deprecated/removed a feature; verify our code doesn't still rely on it (especially in `BridgeClient*`, `Settings.*`, `ServerConfig.*`).
        - **Moved** option (e.g., `protocol` moved from `[options]` → `[server]` in this last merge) → there's an underlying code refactor; track down the upstream commit (`git log -p --follow -- docs/Configuration.md`) and verify the corresponding code change merged cleanly into our tree.
        - **Semantic change** → wording change without key change can hide behavior tweaks (e.g., default value flipped, validation added). Read the upstream commit body.
    5.  **In-app help docs are user-visible.** Since v1.27.0, `docs/{HelpMain,Issues,Readme,Security,Configuration}.md` are installed into the app and rendered by Help ▸ View Help (F1). Rebrand every upstream change to them: links to deskflow.org, the deskflow/deskflow issue tracker or advisories, Matrix/IRC channels, and "continuous" builds have no DShare-HID equivalent. Support channels are GitHub Issues/Discussions on lockekk/dshare-hid and dshare.hid@gmail.com.
    6.  **ALERT THE USER** when any function/option is **added, removed, moved, or changed in semantics**. Surface the list explicitly with the upstream commit ref and a one-line "this might affect us because ___" note. Do not silently rebrand-and-commit. The user wants the chance to decide whether DShare-HID needs to adopt, ignore, or work around each change.

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
*   **`I18NTests` is removed on purpose**: it expects upstream's Korean catalog, which the fork leaves out (Scenario A), so it could never pass. If a merge brings back `src/unittests/common/I18NTests.{cpp,h}` or its `create_test` block in `src/unittests/common/CMakeLists.txt`, remove them again.
*   **Fork tests**: `src/unittests/bridge/` is the fork's own (upstream has no such directory, so it never conflicts). It is hooked in by one line at the end of the subdirectory list in `src/unittests/CMakeLists.txt`, `add_subdirectory(bridge) # dshare-hid`; after taking upstream's version of that file, add the line back.

### Scenario M: macOS DMG OpenSSL Bundling
*   **Issue**: Upstream changes anything under `deploy/mac/` (especially `post_bundle_process.cmake.in`, `deploy.cmake`, or `build_universal_openssl.sh`). The DMG bundles a locally-built universal OpenSSL from `deps/openssl-universal/` (gitignored), and the install_name handling for those libs is project-specific.
*   **Why this matters**: OpenSSL's `./Configure --prefix=...` bakes an *absolute build path* into each dylib's `LC_ID_DYLIB`. `run_task.sh` skips rebuilding OpenSSL when the libs exist, so a stale absolute path will silently survive a checkout move or an upstream merge. The resulting DMG ships executables that reference a path that no longer exists. The fix in tree (`240464c791`) replaces those absolute paths with `@rpath/...`.
*   **Resolution**:
    1.  **Preserve `@rpath` form in `deploy/mac/build_universal_openssl.sh`**: The "4.5 Make install names relocatable" block between the per-arch builds and the `lipo` step must stay. It rewrites ids + libssl's libcrypto reference to `@rpath/` and re-applies an ad-hoc codesign (because `install_name_tool` invalidates the signature). The post-`lipo` `codesign -f -s -` on the universal slices must also stay (`lipo` strips signatures).
    2.  **Preserve `@rpath` rewrite rules in `deploy/mac/post_bundle_process.cmake.in`**: The `install_name_tool -change "@rpath/libssl.3.dylib" ...` and `"@rpath/libcrypto.3.dylib" ...` lines exist for (a) libssl itself, (b) the main executable `@CMAKE_PROJECT_PROPER_NAME@`, and (c) the helper `dshare-hid-core`. The absolute-path `-change` rules in the same file are kept as harmless fallbacks for libs produced before the fix — do not delete either half during a conflict resolution.
    3.  **Verify the resulting DMG, not just the build**: After `./run_task.sh 5` finishes, run:
        ```
        otool -L build_dmg/_CPack_Packages/Darwin/DragNDrop/*/DShare-HID.app/Contents/MacOS/dshare-hid-core | grep -i ssl
        ```
        Expected: two `@loader_path/../Frameworks/libssl.3.dylib` lines (one per arch slice). Any absolute path (`/Users/...`, `/Volumes/...`) is a regression.
    4.  **Recovery if the sweep shows an absolute path**: `rm -rf deps/openssl-universal/{arm64,x86_64,universal} && ./run_task.sh 5` — a full OpenSSL rebuild from the current script guarantees clean `@rpath` install names.
*   **Note**: `macdeployqt` printing `ERROR: no file at "@rpath/libssl.3.dylib"` during the build is cosmetic — `macdeployqt` can't resolve `@rpath` itself; the real OpenSSL bundling runs *after* it in `post_bundle_process.cmake.in`. The sweep above is what tells you whether the bundle is actually correct.

### Scenario N: CMake cache `-D` variable loss after rebase / branch switch
*   **Issue**: After `git rebase`, `git checkout` between branches with diverging CMakeLists.txt, or any operation that changes timestamps on the build's input files, the CMake cache in `build/CMakeCache.txt` can lose `-D` variables that were set via `./run_task.sh 4` (configure release). The next `./run_task.sh 1` (incremental build) happily proceeds **with the variables empty**, falling into the `else` branches in build scripts and producing a working-but-misconfigured binary.
*   **Observed failure mode (CDC handshake)**: `src/CMakeLists.user.cmake` reads `DESKFLOW_CDC_PUBLIC_KEY` (the `if(DESKFLOW_CDC_PUBLIC_KEY)` branch). If the cache has it, the real public key is bin2array'd into a header and embedded. If the cache lost it (empty), the build silently falls back to a dummy public key, the ESP32 device's ECDSA signature fails verification, and the GUI logs `CDC: ECDSA verification failed (ret=0) / Handshake authentication failed`. Bridge clients then exit with code 1.
*   **Resolution**:
    1.  **After any rebase, merge, or branch-switch that touches build files, always re-run `./run_task.sh 4`** (which `rm -rf build` and re-passes `-DDESKFLOW_CDC_PUBLIC_KEY="$DESKFLOW_CDC_PUBLIC_KEY"` and `-DDESKFLOW_ESP32_ENCRYPTION_KEY="$DESKFLOW_ESP32_ENCRYPTION_KEY"` from your shell env). Then `./run_task.sh 1` rebuilds with the keys baked in.
    2.  **Do not rely on `./run_task.sh 1` after a branch operation.** Incremental builds reuse whatever the cache currently holds, including silently-empty `-D` variables.
    3.  **Verify the cache state if anything looks off**: `grep "DESKFLOW_CDC_PUBLIC_KEY\|DESKFLOW_ESP32" build/CMakeCache.txt`. Both should be present with `UNINITIALIZED=<path>` entries. Empty output means option 4 needs to run.
*   **Why this is a real trap**: There is no error or warning when the cache loses these variables. `src/CMakeLists.user.cmake` only emits `STATUS "Using default dummy public key for CDC Transport"`, which is buried in CMake output. The first symptom is at runtime, where it looks like a key-mismatch problem rather than a build-config problem. Documented here because we've hit it twice in one session.

### Scenario O: Cascading HEAD-only API renames (silent compile breakage)
*   **Issue**: Upstream renames a symbol (macro, enum value, member variable, accessor method) that is **also referenced by HEAD-only files this fork added** (bridge code, ESP32 HID extension, etc.). Because those HEAD-only files have no upstream counterpart, `git merge` sees no conflict and the file is auto-merged unchanged. The build then fails the moment that translation unit hits the compiler.
*   **Examples from the 2026-05/06 squash merge**:
    - `LOG_DEBUG1` / `LOG_DEBUG2` macros deleted by `8e469379e8` (squashed into `LOG_VERBOSE`). Broke `src/lib/client/BridgeSocketFactory.cpp`, `src/lib/client/Client.cpp`, `src/lib/platform/bridge/BridgePlatformScreen.cpp`, `src/lib/deskflow/ServerApp.cpp`.
    - `LogLevel` turned from a bare enum into a class with an inner `Level` enum by `acf865fa11`. Broke `LogLevel::Debug` references in `src/lib/platform/bridge/BridgePlatformScreen.cpp` and `src/lib/platform/bridge/CdcTransport.cpp` (and the `#include "base/Log.h"` no longer forwards the type — needed an explicit `#include "common/LogLevel.h"`).
    - `ServerConfig::m_Columns` / `m_Rows` lowercased to `m_columns` / `m_rows` by `c4f65c04a3`. Broke HEAD's bridge bonding code in `src/lib/gui/config/ServerConfig.cpp`.
    - `ServerConfig::numColumns()` public accessor removed in the same commit. Broke HEAD's `src/lib/gui/DeskflowHidExtension.cpp` — replace with `Settings::value(Settings::Server::GridWidth).toInt()`.
*   **Resolution**:
    1.  **Build immediately after staging the merge.** Don't trust "no conflicts" — that just means git didn't see textual overlap, not that the result compiles. Run `./run_task.sh 4` + `./run_task.sh 1` before committing the squash.
    2.  **For each compile error**, treat it as a real breakage of HEAD code by upstream API rename. The fix lives in HEAD's file (rename the call site, adopt the new API, add the new include). Bake those fixups into the same squash commit; they are part of the merge resolution, not a follow-up.
    3.  **Don't `--amend`** if you've already committed the squash and discovered the breakage later — `--amend` silently sweeps any other working-tree changes (e.g. regenerated `translations/dshare-hid_*.ts` from `lupdate`) into the merge commit. Use a follow-up `fixup(merge): adopt <symbol> after upstream API rename` commit on the same branch instead.
*   **Rationale**: Conflict resolution by the scenarios above only catches files git can see as touched on both sides. HEAD-only files (most of the bridge tree, DShare-HID-only extensions) are invisible to the merge — they survive the merge intact but reference symbols that may no longer exist. Compile is the only line of defence.

### Scenario P: Hybrid app bundle after raw `cmake --build` (macOS launch failure)
*   **Issue**: On macOS, `./run_task.sh 1` is *build + macdeployqt on
    `build/bin/DShare-HID.app` + codesign* as one unit. Running a raw
    `cmake --build build` **after** any task-1 deploy relinks the executable
    (whose `LC_RPATH` then points at `~/Qt/...`) inside a bundle that still
    contains the old macdeployqt output (`Resources/qt.conf`, `Frameworks/`,
    `PlugIns/`). At launch the exe loads the system QtCore while `qt.conf`
    forces the bundled cocoa plugin (linked against the bundled QtCore) —
    two Qt runtimes in one process.
*   **Symptom**: `objc[..]: Class ... is implemented in both ...QtCore...`
    warnings followed by `qt.qpa.plugin: Could not load the Qt platform plugin
    "cocoa" ... even though it was found.` The app exits immediately.
*   **Resolution**:
    1.  Raw `cmake --build build --target <lib>` is fine for compile-only
        checks during a merge session (libraries are not deployed).
    2.  Any time the app must *run* — and always at the end of a session
        before handing back — finish with `./run_task.sh 1`. Re-running
        task 1 also repairs an already-hybrid bundle.
    3.  Verify: `otool -L build/bin/DShare-HID.app/Contents/MacOS/DShare-HID | grep QtCore`
        must show `@loader_path/../Frameworks/...` (deployed) — an `@rpath/...`
        reference plus an `LC_RPATH` to `~/Qt` means the exe was relinked after
        the deploy and the bundle is hybrid.
*   **Note**: This is a build-tree state trap like Scenarios M and N, not a
    conflict scenario. First hit 2026-07-08: the app "couldn't launch at all"
    right before a planned push; no source change was at fault.

### Scenario Q: CMake keyword / plain `target_link_libraries` mixing (CMP0023)
*   **Issue**: Upstream converts a library's links to the keyword form (`target_link_libraries(gui PUBLIC ...)`), while a fork file adds links to the same target in the plain form (`target_link_libraries(gui ${LIB})`). CMake refuses to configure: one target cannot mix both signatures. No conflict marker shows this — the fork lines live in `src/lib/gui/CMakeLists.user.cmake` or in auto-merged tails of upstream CMakeLists.
*   **Resolution**: Always use the keyword form for fork links (`PRIVATE` unless a public header needs the dependency). Seen in v1.27.0 for `gui` (CMakeLists.user.cmake) and `platform` (OpenSSL link, duplicate Cocoa block).

### Scenario R: Settings key whitelist (`m_validGroup` / `m_validKeys`)
*   **Issue**: `Settings::cleanSettings()` runs on every settings load, including when the core opens a per-device `bridge-clients/*.conf`. It deletes any key whose group is listed in `m_validGroup` but which is itself missing from `m_validKeys`. `"bridge"` is in `m_validGroup` (so `removeUnknownComputers()` keeps the group), which means **every `Settings::Bridge::*` key and every other fork key in a listed group (`core/`, `client/`, ...) must be in `m_validKeys`**, or it silently disappears from disk.
*   **Upstream key removals bite the same way**: when upstream drops a key from `m_validKeys` (v1.27.0: `core/screenName`), it is deleted from bridge configs on the next core start. Fork readers must move to the replacement key (bridge code reads names through `BridgeClientConfigManager::readScreenName()`, which falls back to the legacy key).
*   **Renamed stored keys**: when upstream changes a stored key string without a migration (v1.27.0: `server/enableHeatbeat` → `server/enableHeartbeat`), add one in `upgradeSettings()` and call it out as a Scenario G alert.
*   **Verify** with the core binary against a throwaway `HOME` (the core runs `upgradeSettings`/`cleanSettings` before handling `--version`), never against the real `~/Library/DShare-HID`: `HOME=$tmp build/bin/DShare-HID.app/Contents/MacOS/dshare-hid-core client --link /dev/null --name Bridge-TEST --version`, then inspect `$tmp/Library/DShare-HID/bridge-clients/Bridge-TEST.conf`.

### Scenario S: Functional identity strings that auto-merge
*   **Issue**: Not every Deskflow string is cosmetic, and these merge without conflict. v1.27.0 examples: `deploy/windows/wix-patch.xml.in` gained `util:CloseApplication` entries targeting `deskflow.exe`/`deskflow-core.exe` (the MSI would close a real Deskflow install and never ours); the xdg portal global-shortcut description; the macOS power-assertion name in `OSXComputer.mm`.
*   **Resolution**: The post-merge rebrand sweep must grep the *added lines* of the whole merge, not just conflicted files: `git diff --cached <pre-merge-tip> | grep '^+' | grep -E 'Deskflow|deskflow(-core)?\.exe|deskflow\.org'`. Retarget executables to `dshare-hid.exe` / `dshare-hid-core.exe`; leave comments, `deskflow::` namespaces, SPDX headers and internal identifiers alone.

### Scenario T: Rename not detected → modify/delete
*   **Issue**: When upstream renames a file *and* rewrites much of it (v1.27.0: `ScreenSetupModel.cpp` → `ComputerSetupModel.cpp`), git may not pair the two, and reports `CONFLICT (modify/delete): <old> deleted in <upstream> and modified in HEAD`. The fork's version of the old file is left in the tree and would be built or silently dropped.
*   **Resolution**: `git diff <merge-base> HEAD -- <old path>` to list the fork hunks, port them into the renamed upstream file using the new names, then `git rm <old path>`. Preview a merge's conflict set without touching the worktree with `git merge-tree --write-tree --name-only HEAD <upstream-ref>` (rename-aware, including these modify/delete cases).

### Scenario U: To-scale screen layout hooks
*   **Issue**: The to-scale Computers tab (plan: `docs/dev/scaled-screen-layout.md`) lives in fork-owned files and reaches upstream through single inserted lines, each marked `// DShare-HID: layout`. A conflict on one of those lines is the easy case. The dangerous case is **an upstream change that merges cleanly and breaks a hook**. Git says nothing, `merge_next_upstream.sh` doesn't stop, and the result can be a server that refuses to start or inner edges that quietly stop crossing.
*   **Status**: Merged to `main` on 2026-11-06. `grep -rn 'DShare-HID: layout' src/lib` lists every hook.

**Merging upstream with these hooks, step by step.** This is the whole job. The rest of this scenario is reference for when a step sends you there.

1.  **Note where you start:** `git rev-parse --short HEAD` on `main`. This is `<pre-merge-tip>`.
2.  **Merge** with one two-parent merge, or commit by commit with `merge_next_upstream.sh --all`, which stops when a person is needed.
3.  **If a line marked `// DShare-HID: layout` conflicts:** take upstream's lines, then put the hook back at its anchor from the table below. Never edit an upstream line to make a hook fit.
4.  **If upstream renamed something the fork uses:** git can't see this. Search for the old name in fork files before committing (Scenario O). In the 2026-11-06 merge, upstream replaced `lineHostname` with `cbHostname`, and the search came back empty, so the merge was safe.
5.  **Run the gate:** `TEST_BUILD=/some/folder/outside/the/repo .agent/skills/merge-upstream-master/verify_merge.sh <pre-merge-tip>`. It does the rebrand check, the build, the DMG check and the watch check, then runs the layout tests if a hooked file changed. If it prints `layout tests OK`, the hooks survived.
6.  **Read what upstream did to any hooked file it changed,** even with no conflict. `verify_merge.sh` lists them. Check them against "Upstream changes to look for" below.
7.  **Keep the regenerated `translations/dshare-hid_*.ts`** and commit them, with new strings left unfinished.
8.  **Build on Linux if the portal code changed.** `PortalInputCapture` and the X11 code don't compile on macOS, so nothing on the Mac will catch a mistake there. See "Building on Linux" below.

**Three traps that have happened:**

*   **Don't run `clang-format` on a whole upstream file.** It rewraps upstream's own lines, which breaks the insert-only rule and causes conflicts at the next merge. It did this to `PortalInputCapture.cpp`. Format fork-owned files only. After touching an upstream file, `git diff main -- <file> | grep '^-'` must show no upstream lines.
*   **Keep a hook on one line.** A hook wrapped over two lines is easy to half-remove, and only one of the lines carries the marker.
*   **Add a new hook in three places:** the table below, `kExpected` in `LayoutHooksTests.cpp`, and the watch list in both `verify_merge.sh` and the watch check here. `LayoutHooksTests` fails if the count or the watch list is out of step.

**Hooks.** Find them by anchor, never by line number. One row is one inserted line unless it says otherwise.

| Phase | Upstream file | Anchor | Hook | If it is lost or misplaced |
| :---: | :--- | :--- | :--- | :--- |
| 1 | `gui/config/ServerConfig.cpp` | First line of `adjacentComputerIndex()` | Return `-1` while the layout is active | Grid links and layout links are both written. Overlapping ranges, **the server won't start** |
| 1 | `gui/config/ServerConfig.cpp` | Before `end` of `section: links` in `operator<<` | `writeLinks(outStream, config)` | Computers have no links, and the cursor can't leave the server |
| 1 | `gui/config/ServerConfig.cpp` | After `m_computers[index].setName(newName);` in `renameScreen()` | `DisplayLayoutStore::rename(...)` | A renamed device loses its place and goes to the shelf |
| 1 | `gui/dialogs/ServerConfigDialog.cpp` | After `initConnections();` in the constructor | `install(...)` | The old grid shows and layout edits can't be made |
| 1 | `gui/dialogs/ServerConfigDialog.cpp` | First line of `isGeneralConfigModified()` | Return `true` if the layout changed | Save stays disabled for layout-only changes |
| 1 | `gui/ComputerSetupModel.h` | Beside `friend class ServerConfigDialog;` | Forward declaration and `friend` | Compile error |
| 2 | `server/Server.cpp` | After `sendConnectedClientsIpc();` in the client-connected handler | Send the client's size | Sizes stay as last stored |
| 2 | `server/Server.cpp` | In `handleShapeChanged()` | Send the new size | A resized client stays drawn at its old size |
| 2 | `server/Server.h` | Beside `sendConnectedClientsIpc` | Declaration | Compile error |
| 2 | `gui/core/CoreProcess.{cpp,h}` | Beside `switchBridgeProfile` | Branch and signal | The GUI ignores client sizes, which stay as last stored |
| 2 | `platform/CMakeLists.txt` | Beside the bridge sources | `DisplayRects.{h,cpp}` | Link error |
| 3 | `server/Server.cpp` | After `int32_t zoneSize = getJumpZoneSize(m_active);` in `onMouseMovePrimary()` | Move the position to the box edge on a free edge with a link | Inner edges quietly stop crossing |
| 3 | `server/Server.cpp` | After `avoidJumpZone(dst, srcSide, x, y);` in `mapToNeighbor()` | Land on a real display | The return lands where no display exists |
| 3 | `server/Server.cpp` | In `handleShapeChanged()`, when the client is primary | Refresh the display list | Inner edges are wrong after docking until the core restarts |
| 3 | `server/Server.cpp` | After `getNeighbor(...)` in `handleSwitchInDirectionEvent()`, plus the helper at the end of the file | `nearestNeighbor(...)` when the cursor's row has no link | The "switch left/right" hotkey does nothing unless the cursor is level with a partial link |
| 3 | `server/Server.h` | Beside `sendComputerShapeIpc` | Three declarations | Compile error |
| 3 | `server/CMakeLists.txt` | Beside `BridgeProfileAction.cpp` | `DisplayEdges.{h,cpp}` | Link error |
| 3 | `deskflow/ServerApp.cpp` | First line of `openServerComputer()` | `DisplayEdges::setSource(...)` | The server has no display list, so inner edges quietly stop crossing |
| 3 | `platform/OSXComputer.mm` | In `leave()`, after `avoidHesitatingCursor();` and before the mouse is captured, plus the helper at the end of the file | `hideOffTheDock();` | A crossing through the Dock leaves the server cursor showing, and it follows the mouse. No test can catch this but the hook check, so keep the order |
| 3 | `platform/OSXComputer.h` | Beside `hideCursor()` | One declaration | Compile error, on macOS only |
| 5 | `platform/PortalInputCapture.cpp` | After the zone loop in `handleZonesChanged()`, before the barrier list is built | `addFreeEdgeBarriers(session, id)` | Wayland stays on outer edges, and a client in a notch can't be reached |
| 5 | `platform/PortalInputCapture.cpp` | After `..._set_pointer_barriers_finish` in `handleSetPointerBarriers()` | `if (retryWithoutFreeEdges(error)) return;` | A refused barrier can stop input capture altogether |
| 5 | `platform/PortalInputCapture.cpp` | In `getPortalBounds()`, after the empty check | `if (boundsFromAllZones(bounds)) return true;` (two lines) | A display with no barrier, or one the compositor refused, shrinks the desktop, and crossings land in the wrong place |
| 5 | `platform/PortalInputCapture.h` | Beside `handleZonesChanged`, `getPortalBounds` and `m_barrierInfo`, plus `<QRect>` | Three declarations, three members, one include | Compile error, on Linux only |
| 5 | `platform/CMakeLists.txt` | Beside the bridge sources | `PortalBarriers.{h,cpp}` | Link error |

New helper functions go at the end of `Server.cpp`, `PortalInputCapture.cpp` and `OSXComputer.mm`, so they stay out of upstream's way.

**Fork-owned, can't conflict:** `src/lib/gui/layout/`, `src/lib/platform/DisplayRects.*`, `src/lib/platform/PortalBarriers.*`, `src/lib/server/DisplayEdges.*`, `src/lib/common/ComputerShapeMessage.h`, `src/lib/gui/CMakeLists.user.cmake`, `src/unittests/bridge/`.

**Upstream's grid stays compiled and unmodified.** `ComputerSetupView`, `ComputerSetupModel.cpp`, `ComputerList`, `Computer`, `NewComputerWidget`, `RemoveComputerWidget` and `ServerConfigDialog.ui` carry no layout changes, so always take upstream's version of their code. Keep only the older fork hooks listed in §6.

*   **Resolution**:
    1.  **Conflict on a hook line:** take upstream's lines, then re-add the hook at its anchor with the same marker. Never edit an upstream line to make a hook fit, and never fold a hook into an upstream statement.
    2.  **Anchor gone or moved:** read what upstream did, put the hook where it keeps the same meaning, and update the anchor in the table above in the same commit. The next two tables say what each hook must keep doing.
    3.  **Check clean merges too:** after each session, run the watch check below. A clean merge of a watched file still needs the gate.
    4.  **Do not ask the user** about ordinary conflicts here. **Stop and ask** only for the rows marked so below.

**Watch check.** `<pre-merge-tip>` is the commit before this session's first merge:

```sh
git diff --name-only <pre-merge-tip> HEAD -- \
  src/lib/gui/config/ServerConfig.cpp src/lib/gui/dialogs/ServerConfigDialog.cpp \
  src/lib/gui/ComputerSetupModel.h src/lib/gui/ComputerSetupModel.cpp \
  src/lib/gui/widgets/ComputerSetupView.cpp src/lib/gui/widgets/NewComputerWidget.cpp \
  src/lib/gui/widgets/RemoveComputerWidget.cpp src/lib/gui/core/CoreProcess.cpp \
  src/lib/gui/core/CoreProcess.h src/lib/gui/ipc/IpcClient.cpp \
  src/lib/server/Server.cpp src/lib/server/Server.h src/lib/server/Config.cpp \
  src/lib/server/CMakeLists.txt src/lib/deskflow/ServerApp.cpp src/lib/deskflow/ipc \
  src/lib/platform/CMakeLists.txt src/lib/platform/PortalInputCapture.cpp \
  src/lib/platform/PortalInputCapture.h src/lib/platform/OSXComputer.mm \
  src/lib/platform/OSXComputer.h
```

No output means nothing to do. Any output means read the table below for those files, then run the gate.

**Upstream changes to look for.** These usually merge without a conflict. Each one that can stop the server starting has a test in the gate.

| Upstream change | What breaks | What to do |
| :--- | :--- | :--- |
| Writes links without `adjacentComputerIndex()`, or adds a second writer | Both sets of links are written and the server won't start | Move the suppress hook so upstream's neighbour lines aren't written |
| Changes the `section: links` grammar | Generated text is rejected | Update `DisplayLayout::linkText()` |
| Changes how ranges are parsed | Today's parser uses `strtod`, which follows `LC_NUMERIC`, so ranges are written as `318056e-4` with no decimal point. A plain `31.8056` stops the server starting under a comma locale | Keep `DisplayLayout::formatPercent()` locale-proof |
| Removes the text config (`feat/no-legacy-conf`) | `writeLinks` goes away | Follow "If upstream removes the text config" below |
| Changes `mapToFraction`, `mapToPixel`, `getNeighbor` or `mapToNeighbor` | Crossing points or landings drift | Run `ServerInnerEdgeTests`, then fix the helpers, not upstream's code |
| Changes `Config::getNeighbor`, or how a link along a whole side is stored | `projectOntoFreeEdge()` tells a whole-side link from a partial one by asking for the neighbour at both ends of the side. If that stops working, grid users get crossings on inner edges | Keep `linkAlongAWholeSideStaysAtTheOutside` passing |
| Changes `handleSwitchInDirectionEvent` or `jumpToComputer` | The hotkey stops finding partial links | Keep the `nearestNeighbor` line straight after `getNeighbor` |
| Changes `OSXComputer::leave()`, `hideCursor()` or the tap in `handleCGInputEvent` | The server cursor stays on show after a crossing through the Dock. **No automated test covers this.** `LayoutHooksTests` only checks the call is there and in order | `hideOffTheDock()` must stay before `CGAssociateMouseAndMouseCursorPosition(false)`, and before `m_isOnComputer` turns false. Then cross through the Dock by hand: the Dock moves to whichever display you push against, so find it first |
| Changes `getPortalBounds` or how the portal maps positions | Crossings land in the wrong place on Wayland | Keep the hook after the empty check, and check `allZones()` still matches what upstream expects |
| Moves the edge test in `onMouseMovePrimary` | The hook no longer runs before it | Keep the hook after `m_x` and `m_y` are saved and before the edge test |
| Changes the jump zone or switch-wait logic | Bounce on return, or a missed switch | Keep the landing `zoneSize` pixels inside the edge |
| Changes `mimeType()`, the `-1 -1 computer` stream in `NewComputerWidget`, or the trash drop | New computer or trash stops working on the canvas | Match it in `DisplayLayoutView` |
| Renames `computerSetupView`, `m_computerSetupModel`, the `reset` signal or `screenDeleted` | Compile error, or Reset and bridge cleanup stop | Update `install()` |
| Renames classes (like Screen → Computer) | Compile errors in fork files | Scenarios O and T: rename in fork files, keep the markers |
| Changes `ipcSendToClient` or the `=` message format | Sizes stop arriving | Update the message in the Phase 2 code |
| Changes barrier building in `handleZonesChanged` | Free-edge barriers duplicate or fail | Keep the hook after upstream's loop, and keep the fallback. Upstream's loop adds the edges on the bounding box and `freeEdgeBarriers()` adds the rest, so if upstream starts adding inner edges itself, drop ours. **This code only compiles on Linux, so build there after any change** |
| **Stop and ask:** upstream replaces the grid, or `ComputerList` stops being the computer list | The layout has nothing to attach to | Report it, and don't improvise |
| **Stop and ask:** upstream adds its own multi-monitor edges or a scaled layout | Two systems would do the same job | Report it, because the user decides whether to adopt theirs and drop these hooks |

**If upstream removes the text config** (`upstream/feat/no-legacy-conf`, not merged as of 2026-11-06): the merge deletes `operator<<`, so the two `ServerConfig.cpp` hooks go with it.

1.  On Save, write the link list from `DisplayLayout` to `<settingsPath>/screen-layout-links.conf`.
2.  Add a fork-owned reader in `src/lib/server/` that calls `Config::connect()` with the ranges.
3.  Insert one line at the top of `Config::addLinksFromSettings()`: if the reader loaded links, return, so upstream's full-edge grid links aren't added.
4.  Update the hook table and call it out as a Scenario G alert.

**Gate.** Run this from a scratch build folder, because `run_task.sh` always turns tests off. Point `TEST_BUILD` outside the repo. On macOS, also pass `-DCMAKE_OSX_SYSROOT="$(xcrun --show-sdk-path)"` when configuring. The temporary `HOME` keeps tests away from real settings (see Scenario R):

```sh
cmake -S . -B "$TEST_BUILD" -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH" -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build "$TEST_BUILD" -j8 --target layout-tests
env -u XDG_CONFIG_HOME -u XDG_STATE_HOME HOME="$(mktemp -d)" \
  ctest --test-dir "$TEST_BUILD/src/unittests" --no-tests=error --output-on-failure \
  -R 'DisplayLayout|DisplayMonitors|DisplayEdges|DisplayRects|ComputerShapeIpc|ServerInnerEdge|LayoutHooks|PortalBarrier'
```

Keep `--target layout-tests`: a test build's default target includes upstream's `run_tests`, which runs the whole suite under your real `HOME`. Keep `--test-dir` on `src/unittests` and keep `--no-tests=error`. `enable_testing()` is in that folder, so `ctest` from the build root finds nothing and still exits 0, which looks like a pass. The `layout-tests` target exists from Phase 1 and builds only the layout tests, because the upstream tests aren't kept compiling here (Scenario L).

Every test must pass. `LayoutHooksTests` fails if a marker goes missing, a hook leaves its anchor, two hooks swap order, or a hooked file drops off the watch list. Then finish with `verify_merge.sh` as usual.

`OSXKeyStateTests` isn't part of this gate, and it can fail by itself while DShare-HID is running on the same Mac: it sends real key presses, and a running core sees them.

**Building on Linux.** Run this on a Linux machine, in a folder outside any other checkout. Use the targets shown: a test build's default target would run the suite under your real `HOME`. Fetch submodules recursively, or configure fails on `esp-serial-flasher`:

```sh
git clone --branch main git@github.com:lockekk/dshare-hid.git src
git -C src submodule update --init --recursive --depth 1
cmake -S src -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
TESTS=$(ctest --test-dir build/src/unittests -N | sed -n 's/.*Test *#[0-9]*: //p' | tr '\n' ' ')
cmake --build build -j6 --target dshare-hid dshare-hid-core app_translations $TESTS
env -u XDG_CONFIG_HOME -u XDG_STATE_HOME -u DISPLAY -u WAYLAND_DISPLAY HOME="$(mktemp -d)" QT_QPA_PLATFORM=offscreen \
  ctest --test-dir build/src/unittests --no-tests=error --output-on-failure
```

Every test should pass.

*   **Merge commit**: name Scenario U, the hook re-added or moved, and any row of the upstream-changes table that applied.
*   **Tests are fork-owned**: they live in `src/unittests/bridge/`, so Scenario L's "take upstream" never touches them. After taking upstream's `src/unittests/CMakeLists.txt`, restore its `add_subdirectory(bridge)` line, as Scenario L says, or the gate finds no tests.

## Automation

`.agent/skills/merge-upstream-master/merge_next_upstream.sh` merges the next
upstream commit (or `--all` for the whole backlog), auto-applies the
deterministic Scenarios A/B/J/L, formats the merge-commit message per the rule
above, and stops with a template (`.git/MERGE_MSG_TEMPLATE`) whenever a human
scenario (C–M) is hit. `verify_merge.sh` is the session gate: rebrand grep +
`./run_task.sh 1` + the Scenario M DMG sweep (pass the pre-merge tip as the
base ref when not merging onto `main`).

`verify_merge.sh` also runs Scenario U's watch check, and its test gate when a merge
touched a hooked file, because a hook can break in a merge with no conflict.

The script assumes the target branch is `main` and the source is
`upstream/master`. To merge a release tag or into another branch, do it by
hand. The default is **one two-parent merge** of the whole backlog or tag
(`git merge --no-ff --no-commit <target>`, resolve, build, commit); v1.27.0
merged 210 commits this way. Commit-by-commit merging with the script is
optional. Never use `git merge --squash`: a squash commit has one parent,
so git keeps the old merge-base and the next sync re-conflicts on every one of
those commits.

---

## 5. One-time git config (per clone)

Run these once in every clone; they are not committable:

```
git config rerere.enabled true      # record & replay conflict resolutions
git config rerere.autoUpdate true   # auto-stage files rerere resolved
git config merge.ours.driver true   # activates ".gitattributes: README.md merge=ours"
```

Without `merge.ours.driver`, the `merge=ours` attribute on README.md is silently
inert. With rerere enabled, recurring conflicts (rebrand strings, INSTALL_DAEMON
block, Scenario M files) resolve themselves after the first time.

## 6. Divergence Budget

The 2026-07-08 convergence audit (branch `chore/upstream-convergence`) reverted
every *accidental* divergence from upstream: SPDX-header drift, the dead
SYSAPI_*/WINAPI_CARBON macro family (upstream f2a54f4af0 replaced it with
Q_OS_*), and the lost log-SearchWidget integration. What remains is
**intentional** and is the complete list of upstream files this fork modifies.

**Rule for future merges**: when resolving a conflict in a file NOT listed
below, prefer upstream ("theirs") — our side is probably drift. When a file
listed below conflicts, protect the named fork content and take upstream for
everything else. Do not let new never-listed divergence accumulate silently:
either add it here (with its reason) in the same commit, or revert it.

### Identity / rebrand (protect per Scenarios A–K)
All files carrying DShare-HID naming, URLs, bundle IDs, icons, installer
artwork (`deploy/mac/dmg-*`, `deploy/windows/wix-*.png` are branded binaries —
never take upstream's), translations, README.md, and docs. See Sections 1–3.

### Build / packaging
- `CMakeLists.txt` (root) — project identity + fork build additions
- `cmake/Libraries.cmake` — Qt `Concurrent` component; `HAVE_FORMAT` disabled on APPLE
- `deploy/CMakeLists.txt` — `CPACK_STRIP_FILES` off on APPLE
- `deploy/mac/deploy.cmake`, `deploy/mac/post_bundle_process.cmake.in`,
  `deploy/mac/build_universal_openssl.sh` — Scenario M (OpenSSL @rpath bundling,
  universal build, codesign)
- `src/apps/deskflow-daemon/CMakeLists.txt` — `INSTALL_DAEMON` toggle (Scenario I)
- `src/lib/gui/CMakeLists.txt` — `Qt6::Concurrent`, WIN32 links, `include(CMakeLists.user.cmake OPTIONAL)` (Scenario Q)
- `deploy/windows/wix-patch.xml.in` — firewall/run/close-on-install targets use `dshare-hid*.exe` (Scenario S)

### Bridge/HID feature hooks in upstream files
- `src/lib/deskflow/ProtocolTypes.{h,cpp}` — `kMsgDIdentity` bridge handshake
- `src/lib/deskflow/App.h` — `virtual handleComputerError`
- `src/lib/deskflow/ClientApp.h` — virtual connection handlers, protected `getSocketFactory`
- `src/lib/deskflow/ClientApp.cpp` — `m_clientComputer` reset guards, `[BRIDGE]` name suffix, daemonName
- `src/lib/deskflow/ServerApp.cpp` — bridge restart-on-resume/reset, Quit on `ComputerError`
- `src/lib/deskflow/PlatformComputer.h` (was PlatformScreen.h) — `applyClientScrollModifier` hook
- `src/lib/deskflow/unix/AppUtilUnix.cpp` — mac keyboard-layout main-thread dispatch
- `src/lib/common/Settings.{cpp,h}` (Bridge keys — all must be in `m_validKeys`, see Scenario R; enableHeatbeat migration), `Constants.h.in`, `UrlConstants.h` (fork URLs + `kUrlWiki`)
- `src/lib/client/Client.cpp`, `src/lib/client/CMakeLists.txt`
- `src/lib/server/{Server.cpp,BaseClientProxy.h,ClientProxy.{cpp,h},ClientProxyUnknown.cpp}`
- `src/lib/gui/`: MainWindow.{cpp,h,ui} (among others: the constructor shows
  the log at each start — upstream keeps a window whose log was hidden at a
  fixed size, even after a restart, so it can't be resized or zoomed),
  ComputerSetupModel.{cpp,h}
  (`addComputer(..., awayFromServer)`), config/ComputerList.{cpp,h}
  (`addComputerAwayFromServer`), widgets/ComputerSetupView.{cpp,h}
  (`screenDeleted` signal, and `setRemoveConfirmation`/`confirmRemove`,
  asked in `startDrag()` before the source cell is cleared),
  ServerConfig.{cpp,h} (bridge placement,
  `renameScreen`, `moveScreenRelativeToServer`), ServerConfigDialog.{cpp,h,ui}
  (`onScreenDeleted`, `confirmRemoveScreen` set after `initConnections();`), CoreProcess.{cpp,h} (DuplicateServer only — the rest is
  upstream's), SettingsDialog.cpp, AboutDialog.*, StyleUtils.h,
  LogWidget.cpp (h-scroll preservation in `appendLine` only — the SearchWidget
  wiring is upstream's, keep it)
- `src/apps/deskflow-core/` — CoreArgs.h, CoreArgParser.{cpp,h},
  deskflow-core.cpp (bridge CLI: --link, screen size, --name handling)
- `src/lib/platform/CMakeLists.txt` (bridge sources, OpenSSLCompat, keyword-form links), `EiComputer.cpp` (shut down on EI disconnect), `MSWindowsWatchdog.cpp`
- `src/unittests/common/CMakeLists.txt` — `I18NTests` removed (see Scenario L)
- `src/unittests/CMakeLists.txt` — the one line `add_subdirectory(bridge) # dshare-hid`; the fork's own tests live in `src/unittests/bridge/` (see the Scenario L exception)

### To-scale screen layout hooks (Scenario U)
`grep -rn 'DShare-HID: layout' src/lib` lists them. Scenario U has each hook's anchor
and what breaks without it.
- `src/lib/gui/config/ServerConfig.cpp`, `src/lib/gui/dialogs/ServerConfigDialog.cpp`, `src/lib/gui/ComputerSetupModel.h` — Phase 1
- `src/lib/server/Server.{cpp,h}`, `src/lib/gui/core/CoreProcess.{cpp,h}`, `src/lib/platform/CMakeLists.txt` — Phase 2
- `src/lib/server/Server.{cpp,h}`, `src/lib/server/CMakeLists.txt`, `src/lib/deskflow/ServerApp.cpp` — Phase 3
- `src/lib/platform/PortalInputCapture.{cpp,h}`, `src/lib/platform/CMakeLists.txt` — Phase 5
- `src/lib/platform/OSXComputer.{mm,h}` — Phase 3, hiding the cursor over the Dock

### Fork bugfixes (keep; candidates to upstream as PRs)
- `src/lib/arch/win32/ArchMultithreadWindows.cpp` — recursive-lock deadlock fix
- `src/lib/arch/win32/ArchNetworkWinsock.cpp` — IPV6_V6ONLY only for INet6
- `src/lib/net/SecureUtils.cpp` — OpenSSL error handling, binary fopen mode
- `src/lib/net/SocketMultiplexer.cpp` — `Thread::testCancel` in lock-wait loops
- `src/lib/server/Server.cpp` — `nearestNeighbor()`: `handleSwitchInDirectionEvent` looks only at the cursor's row, so a partial link written by hand fails upstream too
- `src/lib/platform/OSXKeyState.cpp` — `IOMainPort` (IOMasterPort deprecated)
- `src/lib/gui/dialogs/SettingsDialog.cpp` — resetToDefault radio-button fix
