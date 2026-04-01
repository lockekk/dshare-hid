---
name: merge-upstream-master
description: Skill to merge upstream master into a local branch following rebranding rules.
---
# Merge Upstream Master Skill

This skill is used when merging changes from the official Deskflow upstream repository into the rebranded DShare-HID repository.

## Prerequisites
1.  **User Manual Merge**: The user should manually initiate the merge from upstream master (e.g., `git merge upstream-master`).
2.  **Conflict State**: The repository is currently in a state with merge conflicts that need to be resolved.
3.  The file `docs/trademark_rebranding_guide.md` must be available in the repository.

## Workflow

### 1. Conflict Resolution
*   Read `docs/trademark_rebranding_guide.md` to understand the rebranding rules and specific file mappings.
*   Resolve Git conflicts by prioritizing DShare-HID rebranding instructions.
    *   **Scenario J**: If the merge reports a conflict in the `.github/` directory, discard the upstream changes and keep the local state (deleted/modified as per rebranding requirements).
*   Use `git status` to ensure all conflicts are addressed.

### 2. Targeted Rebranding Sweep
*   Identify all files that were modified or added as part of the current merge operation (e.g., using `git status` or `git diff --name-only --cached`).
*   Perform a rebranding sweep **ONLY** on the files identified in the previous step.
*   **CRITICAL**: Do NOT spread rebranding to any files that were not part of the upstream merge's changed file list.
*   Update any re-introduced "Deskflow" strings (case-insensitive) to "DShare-HID" in those specific files, adhering to the guide's exceptions (comments, SPDX, etc.).

### 3. Verification & Cleanup
1.  **Build Verification**: Run `./run_task.sh build` to ensure the project compiles correctly with the merged changes.
2.  **Finalize Merge**: Once the build is verified, finalize the merge with a commit if not already automatically done by Git.
3.  **Cleanup**: Remove any temporary files or artifacts created during the process.

### 4. Reporting
*   Provide a brief report of which conflicts were resolved and any targeted rebranding applied.
