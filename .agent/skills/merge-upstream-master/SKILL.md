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
2.  The file `doc/trademark_rebranding_guide.md` must be available in the repository.

## Workflow

### 1. Preparation & Planning
*   Read `doc/trademark_rebranding_guide.md` to understand the rebranding rules and specific file mappings.
*   Fetch the latest changes from the upstream remote.
*   Create an `implementation_plan.md` that outlines:
    *   The merge operation.
    *   Specific rebranding rules to apply during the merge and conflict resolution.
    *   The strategy for handling `.github` files (discard all upstream changes).
*   **PAUSE**: Request user approval of the implementation plan using `notify_user`.

### 2. Execution
*   Perform the `git merge`.
*   Resolve conflicts by prioritizing DShare-HID rebranding as defined in the guide.
*   Perform a post-merge sweep for any re-introduced "Deskflow" strings (case-insensitive) in the source code and configuration.
*   Fix any build regressions introduced by the merge (e.g., resource file names, bundle names).

### 3. Reporting
*   Provide a detailed report in `walkthrough.md` explaining:
    *   What conflicts were encountered.
    *   How each conflict was resolved.
    *   Any rebranding fixes applied to auto-merged files.

### 4. Continuous Improvement
*   Assess if the merge revealed new rebranding scenarios not covered by `doc/trademark_rebranding_guide.md`.
*   If new scenarios exist, propose updates to the guide or update it directly if instructed.
