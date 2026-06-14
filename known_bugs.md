# Known Bugs

Bugs we're aware of but haven't fixed yet. Each entry: what breaks, when it breaks, and the current workaround. Add new entries at the bottom.

---

## macOS: dev build refuses to launch when checkout lives under `/Volumes/`

**Symptom.** `./run_task.sh 3` (launch dev build) shows a dialog "Please drag DShare-HID to the Applications folder, and open it from there." and exits immediately. The same dialog fires for any direct launch of `build/bin/DShare-HID.app` when the checkout path starts with `/Volumes/` (external disks, mounted images, network volumes).

**Root cause.** [src/apps/deskflow-gui/deskflow-gui.cpp:155](src/apps/deskflow-gui/deskflow-gui.cpp#L155) refuses to launch if `QApplication::applicationDirPath()` starts with `/Volumes/`:

```cpp
if (app.applicationDirPath().startsWith("/Volumes/")) {
  // ... show "drag to Applications" message and exit
  return 1;
}
```

This is **upstream code** (introduced 2024-07-02 by Nick Bolton, `61699b7215a` in upstream/master). It's intended to catch end users who mount the shipped DMG and try to launch the app from inside the mounted DMG volume. The check is a prefix-only string match — it can't distinguish a real DMG mount from any other path under `/Volumes/`, so it false-positives on:

- dev checkouts on external disks (this repo, currently at `/Volumes/Ext/checkouts/deskflow-hid`)
- any USB drive, Time Machine disk, network share mounted under `/Volumes/`

Note also that modern macOS App Translocates downloaded DMGs to `/private/var/folders/.../AppTranslocation/...`, so this check often misses its intended target anyway.

**Why we hit it but upstream doesn't.** Upstream developers build under `$HOME` (`/Users/<them>/...`), where the check never fires. Any upstream developer who cloned into `/Volumes/...` would hit the same dialog. The bug has been latent in upstream since 2024.

**When you hit it.** Anytime this repo is checked out under `/Volumes/...`. The previous checkout at `/Users/lockehuang/checkouts/deskflow-hid/` accidentally dodged the check.

**Workarounds.**

1. Move the checkout back under `$HOME` (`~/checkouts/deskflow-hid`).
2. Symlink the built `.app` somewhere outside `/Volumes/` and launch from there.
3. Patch the check locally (do not commit).

**Possible fixes (not yet applied).**

- **Delete the check from this fork.** macOS already warns users when a translocated app tries to grant accessibility/screen recording permissions, so the protection is largely redundant. One-line change.
- **Gate via CMake on bundle vs dev build.** Wrap the check in `#ifdef DESKFLOW_BUNDLE_BUILD` and have `run_task.sh 5` (DMG bundling) set it via `-DDESKFLOW_BUNDLE_BUILD=ON`. Preserves the upstream UX for shipped DMGs.
- **Refine to read-only mount check** via `statfs()` / `MNT_RDONLY`. Universal but still a workaround — the app shouldn't be inspecting its own path at all.

Tracking: report upstream when fixed locally so they can adopt.
