---
name: merge-upstream-master
description: Skill to merge upstream master into a local branch following rebranding rules.
---
# Merge Upstream Master Skill

This skill is used when merging changes from the official Deskflow upstream repository into the rebranded DShare-HID repository.

## Prerequisites
1.  The user must specify:
    *   The **upstream master** reference (e.g., `upstream/master`).
    *   The **local branch** to merge into (e.g., `main` or `upgrading-main`).
2.  The file `docs/trademark_rebranding_guide.md` must be available in the repository.

## Workflow

### 1. Preparation & Planning
*   Read `docs/trademark_rebranding_guide.md` to understand the rebranding rules and specific file mappings.
*   Fetch the latest changes from the upstream remote.
*   Create an `implementation_plan.md` that outlines:
    *   The merge operation.
    *   Specific rebranding rules to apply during the merge and conflict resolution.
    *   The strategy for handling `.github` files (discard all upstream changes).
*   **PAUSE**: Request user approval of the implementation plan using `notify_user`.

### 2. Execution (Incremental Merge Strategy)
Due to the complexity and volume of upstream changes, an **incremental merge strategy** must be used:
1.  **Iterate Commits**: Identify all upstream commits and merge them **one by one** on a temporary branch (e.g., `temp-incremental-merge`).
2.  **Conflict Resolution**: For each commit, resolve conflicts by prioritizing DShare-HID rebranding.
    *   **Translations**: Ignore conflicts in `.ts` files during the incremental loop to avoid build blocks; perform a final rebranding sweep on `.ts` files after the loop.
3.  **Build Verification**: Run `./run_task.sh build` after **every single commit merge**.
    *   If the build fails, fix the regression immediately before proceeding to the next commit.
4.  **Finalize to Target Branch**: Once all commits are successfully merged and verified on the temporary branch:
    *   Switch to the target local branch (e.g., `upgrading-main`).
    *   Apply the final state as a **single, clean merge commit** (e.g., using `git read-tree` from the temp branch).
5.  **Clean History**: Ensure the local branch history reflects a deliberate merge from upstream, preserving the verified work.

### 3. Verification & Cleanup
1.  **Post-Merge Sweep**: Perform a final sweep for any re-introduced "Deskflow" strings (case-insensitive) in the source code and configuration.
2.  **Translation Refresh**: Perform a final refresh of translation files (`.ts`), ensuring "DShare-HID" nomenclature is used in the UI and removing unsupported languages (like `ko.ts`).
3.  **Final Build**: Run a final `./run_task.sh build` to confirm the entire project is stable.

### 4. Reporting
*   Provide a detailed report in `walkthrough.md` explaining:
    *   What conflicts were encountered during the incremental stages.
    *   How API regressions or visibility changes were resolved.
    *   Verification results of the iterative build tests.

### 5. Continuous Improvement
*   Assess if the merge revealed new rebranding scenarios not covered by `doc/trademark_rebranding_guide.md`.
*   If new scenarios exist, propose updates to the guide or update it directly if instructed.
