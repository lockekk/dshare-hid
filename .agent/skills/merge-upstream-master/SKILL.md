---
name: merge-upstream-master
description: Skill to merge upstream deskflow/deskflow commits into DShare-HID main following rebranding rules.
---
# Merge Upstream Master Skill

Used when pulling changes from the official Deskflow upstream repository into
the rebranded DShare-HID repository. The detailed playbook (Scenarios A–P,
rebrand keyword table, file mappings, divergence budget) lives in
`docs/upgrade_from_upstream_guide.md` — read it in full before resolving any
conflict.

## Hard rule

Merge **one upstream commit at a time**, each as its own merge commit on
`main`. Never `git merge upstream/master` or a range.

## Workflow

### 1. Merge loop
Run `.agent/skills/merge-upstream-master/merge_next_upstream.sh` (or with
`--all` for the whole backlog). It merges the next commit, auto-applies the
deterministic scenarios (A: drop re-introduced `deskflow_*.ts`; J: drop
`.github`; L: take upstream `src/unittests`; B: relocate new icons into
`dshare-hid-light`), writes the merge-commit message in the mandated format
(upstream subject + body + `Merged from upstream <sha>.` trailer), and exits 2
with a template in `.git/MERGE_MSG_TEMPLATE` when a human scenario (C–M) is
hit.

### 2. Human conflict resolution (when the script stops)
*   Resolve strictly per Scenarios C–M in the guide. The divergence budget
    (guide §6) is the source of truth: unlisted divergence is drift — prefer
    upstream; listed files have named fork content to protect.
*   Rebrand sweep **only** over the files this merge touched
    (`git diff --name-only --cached`). Do NOT spread rebranding elsewhere.
*   Commit with `git commit -F .git/MERGE_MSG_TEMPLATE`, appending the
    scenario notes for what you resolved.

### 3. Verification
Run `.agent/skills/merge-upstream-master/verify_merge.sh` — rebrand grep,
`./run_task.sh 1` build (on macOS this re-runs macdeployqt + codesign; see
Scenario P for why raw cmake builds must not be the session's last build),
and the Scenario M DMG OpenSSL sweep when a DMG tree exists.

### 4. Reporting
Report which commits were merged, conflicts resolved and scenarios applied,
and call out any **Scenario G alert** (configuration option added / removed /
moved / semantics changed) so the user can decide whether DShare-HID adopts it.
