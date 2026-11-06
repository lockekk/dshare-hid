# To-scale screen layout: research and plan

Status: all five phases are merged to `main`. macOS has run them, including real crossings to a
real client, and Linux builds and passes the unit tests. See "What has and hasn't run" below.
To merge upstream changes, start with Scenario U in `docs/upgrade_from_upstream_guide.md`.
Prototype: <https://claude.ai/artifact/4LRqSNXca5jCQnSnQnUadk> (private, sample data).

## Goal

Replace the 5×3 grid on the Computers tab with a picture like the Windows or
Linux display settings:

- every server display is shown, at its real position and scale
- every client is shown to scale, labelled with its resolution
- a client is placed by dragging it onto an edge
- merging from upstream stays about as easy as it is today

## Decisions

- Placement crosses only where screens touch. Existing neighbours of the server keep
  whole-edge links until they are moved, so an upgrade doesn't change how they cross.
  A link along a whole side crosses only at the outside of the box, as the grid always did.
  Only a partial link can use an inner edge, and that is the only kind an inner edge produces.
- The tab has no settings and no text about the selected computer. Rectangles carry the
  name and size, and the shelf is the only thing under the map.
- A "Use the classic grid" button stays for the first release.
- Inner edges are in scope, on every platform (Phases 3 and 5).
- Full unit tests ship with every phase (see Tests).
- Upstream merges follow Scenario U in `docs/upgrade_from_upstream_guide.md`.

## What the research found

### 1. The core sees the server as one box

Each platform reports one rectangle covering all displays:
`MSWindowsComputer.cpp:1434` (`SM_CXVIRTUALSCREEN`), `OSXComputer.mm:1337`
(`CGRectUnion`), `XWindowsComputer.cpp:892` (root window), `EiComputer.cpp:557`
(union of regions).

`Server::onMouseMovePrimary` (`Server.cpp:1631`, test at `:1683`) switches only
when the cursor is at the edge of that box. So today **the core never crosses from
an edge inside the box**, such as the top of a shorter display beside a taller one.
Finding 4 shows how to lift this.

### 2. The core already supports partial edges

Links accept ranges: `left(0,50) = other(25,75)`. The GUI never writes them. It
writes full edges only (`ServerConfig.cpp:160`). No core change is needed for
accurate placement.

Full-edge links are the cause of the confusion on multi-monitor servers. The
fraction is measured on the box, not on a display, so entry points drift. A
return trip can also land in a part of the box with no display, which is why
`OSXComputer.mm:442` has to clamp invalid cursor positions.

### 3. Tested against the real parser

A scratch program linked to the current `libserver.a`, run with a throwaway
`HOME`, gave these results.

| Input | Result |
| :--- | :--- |
| Fractional ranges, two clients on one side, a computer header repeated | Accepted |
| `Mac left` at 20% where the range starts at 31.8% | No neighbour (correct: the cursor stops) |
| `Mac left` at 50%, range `(31.8182,100) = hp(0,75.5)` | `hp` at 20.13% (linear, as expected) |
| Ranges that only touch, `(0,50)` and `(50,100)` | Accepted |
| Link naming a computer that no longer exists | **Whole config rejected** |
| Overlapping ranges on one side | **Whole config rejected** |
| Zero-length range | **Whole config rejected** |

The last three would stop the server from starting, so the link writer must make
them impossible. The prototype's geometry passed 8 cases (chains, two on one
side, no room, whole edge plus a second client), and the same parser accepted the
text from all 8.

### 4. Inner edges can be made to work

A link can already say "this part of the top side goes to that client":
`up(0,17.19) = client`. It never fires only because the cursor stops at the top of
the shorter display and never reaches the top of the box.

So when the cursor is on a **free edge** (the first display edge seen from outside)
and a link exists there, treat it as being on the box edge. All the existing link,
mapping and switching logic then runs unchanged. Upstream's Wayland code already does
the same for barrier hits (`mapPortalActivationToComputerPosition`).

It takes two hooks:

1. `onMouseMovePrimary`, after `zoneSize` (`Server.cpp:1659`): move the position to the
   box edge when on a free edge with a link.
2. `mapToNeighbor`, after `avoidJumpZone` (`:745`): coming back, land on a real display
   just inside that edge.

Tested on the real `Server` and `Config` classes, using a scratch copy with these two
hooks, fake clients and the prototype's offset desk:

| Cursor | Today | With hooks |
| :--- | :--- | :--- |
| Top row of display 1, under a link, (350,458) | Stays on server | Client at (350,399) |
| Back down from that client | Not reachable | (350,459), on display 1 |
| Left column of display 2, above display 1, (1512,200) | Stays on server | Client at (799,150) |
| Back right from that client | Not reachable | (1513,200), on display 2 |
| Top row of display 1, no link there | Stays | Stays |
| Left column of display 2, free edge, no link | Stays | Stays |
| Seam between the displays, (1512,700) | Stays | Stays |
| Outer right edge, (4071,700) | Switches | Identical |

Whether the server sees the cursor reach a free edge depends on the platform:

| Platform | Server sees every move? | Extra work |
| :--- | :--- | :--- |
| macOS | Yes (`OSXComputer.mm:988`) | None |
| Windows | Yes, the hook relays all moves (`MSWindowsHook.cpp:572`) | None |
| X11 | Yes (`XWindowsComputer.cpp:1476`) | None |
| Wayland | No, only barrier hits | Add barriers on free edges |

Those rows come from reading the code. Nothing was run on Windows or Linux.

**Wayland, with a correction.** An earlier version of this plan said Wayland rejects
inner edges. That was wrong: it took upstream's simplification for the rule.

- The portal spec says a barrier must be at "the outside boundary of the union of all
  zones". That is the union, not its bounding box, so a notch edge qualifies. Only
  seams between adjacent displays are out.
- GNOME (`check_barrier` in mutter) accepts a barrier on a display edge, including part
  of an edge, if no other display shares that line.
- KDE (`inputcapturebarrier.cpp`) requires the whole edge of one display. So the top of
  a shorter display works there, but the free part of a taller display's side doesn't.
- Entry already works, because activation maps to the box edge. Return looks right too:
  `mapPortalReleasePosition` releases inside the barrier's own display.
- Risk: upstream's comment says one rejected barrier can fail the whole set, which would
  stop capture entirely. So Wayland needs a fallback to outer barriers only.

**What stays impossible:** a hole fully enclosed by displays, such as the inside of a
C shape. Each column or row maps to one spot on the box side, so only the first display
seen from outside can have a link there. This arrangement is rare.

### 5. Where the sizes can come from

| Data | Source | State |
| :--- | :--- | :--- |
| Server displays | `QGuiApplication::screens()` | Available in the GUI |
| Bridge client size | Active device profile | Available in the GUI |
| Network client size | Sent to the core in `DINF` (`ClientProxy1_0::recvInfo`) | Core only, logged at DEBUG |
| Number of displays on a client | Not sent, the protocol carries one box | Unavailable |

A core-to-GUI channel already exists: `ipcSendToClient`, already used by the fork
for `switchBridgeProfile` (`CoreProcess.cpp:726`). Sending client sizes through
it takes a few inserted lines. A client with several displays can only be drawn
as one rectangle.

### 6. Units differ between platforms

Qt 6 scales a screen's size on Windows but keeps its position, and it is
Per-Monitor DPI Aware V2 by default. On macOS its geometry matches the native
layout. So display rectangles need a per-platform conversion:

- macOS and Wayland: `QScreen::geometry()` as it is (logical units, as the core uses)
- Windows and X11: top-left unchanged, size multiplied by `devicePixelRatio()`

The core's Windows manifest declares no DPI awareness, but the core creates a
`QApplication` (`deskflow-core.cpp:88`), so Qt's default should apply. **This has
not been tested on Windows.**

Labels show the unit the core uses. A Retina laptop shows `1512 × 982`, not its
panel resolution, because that is what cursor movement is measured in.

### 7. Upstream

Commits in the last 12 months on `upstream/master`:

| File | Commits |
| :--- | ---: |
| `gui/dialogs/ServerConfigDialog.cpp` | 55 |
| `server/Config.cpp` | 46 |
| `gui/config/ServerConfig.cpp` | 31 |
| `server/Server.cpp` | 31 |
| `gui/dialogs/ServerConfigDialog.ui` | 11 |
| `gui/ComputerSetupModel.cpp` | 5 |
| `gui/widgets/ComputerSetupView.cpp` | 4 |

Rewriting the dialog or `ServerConfig` would conflict constantly, so this plan
avoids both.

**Main risk: `upstream/feat/no-legacy-conf`** (`f1e2ef50fe`, 2026-09-19, not
merged). It removes the text config and its parser. Its new reader builds links
from the grid, full edges only, so partial links would stop working if it lands
as it is. `Config::connect()` with ranges survives on that branch. Upstream's
steady move of options into settings suggests something like it will land
eventually. See "If upstream removes the text config" below.

### 8. Other products

- Mouse Without Borders uses a small matrix of computers, with no scale.
- ShareMouse takes its arrangement from the display layout.
- Search results for a Symless article say Synergy 1's grid can't express some
  arrangements without a manual config, and that Synergy 3 lets you resize
  screens by dragging corners. The article itself returned 403, so this rests on
  the search summary only.

## Design

### What the user sees

The prototype shows this working.

- **Canvas**, zoomed to fit. Server displays are locked, because the OS owns them.
  The part of the box with no display is hatched.
- **Blue lines** mark where the cursor can leave. **Green lines** mark where it
  actually crosses.
- **Clients** are rectangles with name and resolution. A dashed outline means the
  size isn't known yet.
- **Not linked** shelf below the canvas: computers with no edges, reachable by
  hotkey. Bridge clients start here, which keeps today's rule of not placing a
  headless device next to the server by accident.
- **Sizes** come from the client or its Bridge profile. Unlinking is a drag to the shelf,
  the right-click menu or Delete.
- **One button** under the map, off the canvas, switches to the classic grid and back.
- **Labels** wrap to fit their rectangle and stay clear of the crossing lines and the
  device badge. What won't fit is left out, never cut: first a long name is shortened in
  the middle, then the size goes.
- Trash, new computer, double-click settings, Hotkeys and Advanced stay as they are.

### Rules

1. A client docks on a free edge of the server's displays, or on a side of another
   client. Pushed in from outside, it stops at the first edge under it, so notches need
   no special case. Until Phase 3, free edges are limited to the outside of the box.
2. Screens never overlap.
3. Links come from what touches, so the picture and the behaviour can't disagree.
4. Each client stores an anchor (`anchor`, `side`, `offset`), and positions are
   computed outward from the server. A resolution change therefore can't detach
   anything.
5. A client that no longer fits waits on the shelf and keeps its placement, so it
   returns when the display does.
6. Moving a client moves what hangs off it. Unlinking it unlinks those too.

### Data flow

```text
QScreen list ─────────┐
Bridge profile size ──┤
core shape IPC ───────┼─► DisplayLayout (pure geometry) ◄─► DisplayLayoutView
screen-layout.conf ───┘             │                            (canvas)
                                    ▼
                            link list ──► ServerConfig operator<< ──► core
```

There are no cycles. `DisplayLayout` has no UI and no I/O, so it can be tested alone.

### Keeping upstream merges easy

The grid stays exactly as it is, and stays the list of which computers exist. A
fork-owned file adds placement on top.

- `ServerConfig`, `ComputerList`, `Computer`, `ComputerSetupModel`,
  `ComputerSetupView` and the `.ui` keep their logic. The grid view is only hidden.
- Hotkeys, `ActionDialog` and `isFull` keep working from the same list. The limit
  stays at 15 computers.
- New classes live in `src/lib/gui/layout/`, built from `CMakeLists.user.cmake`,
  which the fork owns.
- Placement is stored in `<settingsPath>/screen-layout.json`, a separate file like
  `bridge-clients/*.conf`. This avoids the `m_validKeys` trap (Scenario R), and it
  survives upstream renaming setting groups, as it did with `screen_` → `computer_`.

Phase 1 touches three upstream files, insert-only, all marked `// DShare-HID: layout`:

| Upstream file | Inserted | Purpose |
| :--- | :--- | :--- |
| `gui/config/ServerConfig.cpp` | include, `adjacentComputerIndex` early return (`:142`), `writeLinks` call before the links `end` (`:176`) | Upstream's loop then prints only computer names, and the hook adds the links. The parser accepts the repeated headers (tested). |
| `gui/dialogs/ServerConfigDialog.cpp` | include, one `install()` call after `initConnections()` (`:54`), one check in `isGeneralConfigModified()` (`:658`) | Puts the canvas in the grid's place and enables Save on layout-only changes |
| `gui/ComputerSetupModel.h` | forward declaration, one `friend` (`:25`) | Add and remove computers, as the grid view does |

No edits are needed for the rest:

- `install()` connects to `QDialog::accepted` and the button box `reset` signal itself.
- The trash and new-computer widgets keep working because the canvas uses the same
  MIME type.
- `onScreenDeleted` still fires because the canvas emits the hidden view's public
  `screenDeleted` signal.

Merging is covered by **Scenario U** in `docs/upgrade_from_upstream_guide.md`: every hook
with its anchor, what breaks if it is lost, the upstream changes to watch for, and the test
gate. Any commit that adds or moves a hook updates that table in the same commit.

### If upstream changes this code

| Upstream change | Effect | Response |
| :--- | :--- | :--- |
| Edits the dialog or `ServerConfig` | Conflict only if the lines beside a hook change | Keep upstream, re-add the hook |
| Rewrites the grid view or model | None in the view, possibly the `friend` line | Re-add the line |
| Adds or moves a tab | None | Nothing |
| Rewrites `onMouseMovePrimary` or `mapToNeighbor` | Conflict at one hook line | Keep upstream, re-add the line |
| Ships its own scaled layout | Hooks become unnecessary | Compare, then adopt or keep ours |

**If upstream removes the text config:** the two `ServerConfig.cpp` hooks go away.
Add one line in `Config::addLinksFromSettings` that calls a small fork-owned reader
in `src/lib/server/`, which feeds the same link list to `Config::connect()`. The
geometry, the canvas and the stored file don't change, because `DisplayLayout`
produces a link list and only the last step turns it into text. This is recorded
in the guide now, and built only if the branch lands.

## Plan

This is over the 8-file mark: 12 new source files plus tests, 3 upstream files in
Phase 1 and 8 more across the later phases, all insert-only. Each phase can ship alone.

### Phase 1: the to-scale editor

Everything visible, using sizes the GUI already knows.

New files:

- `gui/layout/DisplayLayout.{h,cpp}`: `reach`, `solve`, `links`, `snap`, ported
  from the prototype, QtCore only
- `gui/layout/DisplayLayoutStore.{h,cpp}`: `screen-layout.json`, `isActive`,
  `writeLinks`, rename
- `gui/layout/DisplayMonitors.{h,cpp}`: `QScreen` to core units, per platform
- `gui/layout/DisplayLayoutView.{h,cpp}`: canvas, shelf, `install()`

Custom `QWidget` painting, because there are at most 16 rectangles and all the
real logic is in `DisplayLayout`. `QGraphicsView` would add little here.

Behaviour:

- **Sizes:** what the client or its Bridge profile reported, else 1920×1080 drawn dashed.
- **First use:** with no `screen-layout.json`, the upstream grid path runs untouched.
  Opening the dialog builds a layout from the grid. Nothing changes until Save.
- **Seeding:** the server's four neighbours keep whole-edge links, so their output
  matches today's text. Neighbours further out chain from them. Cells that touch
  nothing go to the shelf. Links between two clients that no longer touch at real
  size are dropped, and the canvas shows the result before Save.
- **Writer guarantees:** it skips computers no longer in the list, skips ranges under
  48 units, and ignores a whole-edge link when a side is shared. All three map to the
  rejected cases in finding 3.
- **New computers:** the default rule runs in the view and in the writer, so a
  computer added elsewhere gets the same treatment. Network clients take the first
  free side (left, right, top, bottom), and Bridge clients go to the shelf.
- **Bonding** must keep working. `applyProfileScreenBonding` and
  `updateBondedScreenLocations` (`DeskflowHidExtension.cpp:349`, `:366`) go through the
  store when it is active. They store an exact placement per profile and still write
  the old grid offset, so the classic grid stays correct.
- **Renames:** handled where they happen, in the view and in `renameScreen` (`:296`).
- **Escape hatch:** a "Use the classic grid" button for the first release.
- **Merge gate:** `verify_merge.sh` gains Scenario U's watch check, so a clean merge that
  touches a hooked file runs the layout tests. The `layout-tests` target comes with it.
- **External config:** unchanged, the tab is already disabled in that mode.
- **Windows side effect:** the hook swallows moves into the last pixel row or column of
  any side that has a link (`MSWindowsHook.cpp:617`). With partial links, the unlinked
  part of that side therefore stops one pixel short. This is cosmetic.

### Phase 2: real sizes from the core

- New `platform/DisplayRects.{h,cpp}`: `displayRects()` lists the displays with
  `EnumDisplayMonitors`, `CGGetActiveDisplayList` or Xinerama, the same sources and units as the
  core's box. It returns an empty list on Wayland until Phase 5.
- **Changed from the first plan:** the GUI calls `displayRects()` directly, and it no longer comes
  over IPC. `persistServerConfig` writes the links before the core starts, so a list reported by
  the core would arrive too late for the config that needs it. Both processes are `QApplication`s
  with the same DPI awareness, so they get the same numbers. The Qt conversion stays as the
  fallback where `displayRects()` is empty.
- Core: a new `sendComputerShapeIpc` appended to `Server.cpp`, called after
  `sendConnectedClientsIpc()` (`:271`) and from `handleShapeChanged` (`:1146`), plus
  a declaration in `Server.h`. It reports client sizes.
- GUI: one `CoreProcess.cpp` branch and one signal, following `switchBridgeProfile`.
- The size is saved, so an offline client keeps its last size.
- The GUI connects to the core's IPC in service mode too, so sizes arrive the same way.
  That path hasn't run on Windows yet.

### Phase 3: inner edges on macOS, Windows and X11

- New `server/DisplayEdges.{h,cpp}`: pure geometry for finding the free edge at a column or row,
  plus moving a position to the box edge and landing on a display. The prototype's `reach` is
  the GUI side of the same rule.
- `Server.cpp`: the two hook lines from finding 4, a third line in `handleShapeChanged` to refresh
  the display list, and helpers appended at the end. `Server.h`: declarations.
- `ServerApp.cpp`: one line hands `DisplayEdges` the `displayRects()` function, because the server
  library doesn't link the platform library.
- **Added to the first plan:** if the displays don't add up to the core's box exactly, inner edges
  turn off and it logs a warning once, so a list in other units can't cross in the wrong place.
- GUI: `reach()` uses free edges wherever `displayRects()` has a list. On Wayland it is empty, so
  the editor stays on outer edges.
- Config text doesn't change. The real parser accepted all 26 generated layouts.
- A computer that was docked in a notch and loses its display follows rule 5.

### Phase 4: follow display changes while running

- `gui/layout/DisplayLayoutFollow.{h,cpp}` watches `screenAdded`, `screenRemoved`, `geometryChanged`
  and client sizes from the core, and waits for 2 seconds of quiet.
- It restarts the core only if the links would really change, comparing them with what was last
  written. No upstream file changes: `DeskflowHidExtension.cpp` connects it.
- A client whose size was unknown can cause one restart when it first connects, if that moves its
  links. After that its size is stored, so the same client causes none.
- It uses `m_coreProcess.restart()`, as Bonding does on `main`. When
  `feat/linux-reload-config-on-layout-change` merges, this call should become its reload, so GNOME
  50 and older don't ask for input capture again.

### Phase 5: inner edges on Wayland

- New `platform/PortalBarriers.{h,cpp}`: which free edges need a barrier, with inclusive coordinates.
  It is pure code, so it builds and is tested on every platform.
- `PortalInputCapture.cpp`: one inserted call after the zone loop adds them, and one more after
  `set_pointer_barriers_finish` falls back to outer edges if the portal refuses the whole set, so
  capture never stops because of this feature. A barrier refused alone just drops out.
- No `Server` change: a barrier hit already maps to the box edge, and the release code already lands
  inside the barrier's own display. `DisplayEdges` stays off on Wayland.
- The GUI turns free edges on for Wayland, using Qt's display list.
- GNOME takes part of an edge. KDE takes only a display's whole edge, so the top of a short display
  works there, but the free part of a tall display's side doesn't: that barrier drops out and the
  cursor stops there. The editor can't tell, so it still shows a crossing.

## Tests

Every phase ships with full unit tests. **A phase isn't done until its tests pass and it
meets these rules:**

- Every geometry rule, every hook, every case the core rejects, and every testable row of
  Scenario U's upstream-changes table has a named test.
- Pure code (`DisplayLayout`, `DisplayEdges`, the `DisplayMonitors` conversion, the store and
  the link writer) has 100% line and branch coverage.
- Every user action in the view has a test. Any line left uncovered is listed with its reason
  in the commit message.
- Tests never touch real settings: each one uses a `QTemporaryDir` and `Settings::setSettingsFile`.
- A bug found later gets a failing test before its fix.

All tests live in `src/unittests/bridge/`, which never conflicts with upstream.

### Phase 1

| Test | Must cover |
| :--- | :--- |
| `DisplayLayoutTests` | **`reach`:** single display, side by side, offset, L shape, stacked, three displays, mirrored (same rectangle twice), negative origin, empty list. **`solve`:** all four sides, chains three deep, a missing anchor, an anchor cycle (must park, not hang), no room, return when room comes back, resolution change, display removed and restored, zero or invalid size, 65535-wide desktop, 15 computers. **`links`:** ranges within 0–100, start below end, no overlap on a side, a way back for every link, whole edge alone, whole edge then a second client, touching ranges, overlap under 48 units skipped, a client smaller than 48 units. **`snap`:** nearest edge, top, middle and bottom magnets, own subtree skipped, no free spot. **`linkText`:** golden text, `(0,100)` left out, four decimals, a `.` decimal point under a comma locale |
| `DisplayLayoutSeedTests` | Grid to layout: four neighbours, chains, isolated cells, a grid cycle (the real layout that prompted this work), bridge clients to the shelf, text identical to upstream's for adjacent computers, nothing written before Save |
| `DisplayLayoutStoreTests` | Missing file means inactive, save and load round trip, corrupt file, unknown version, rename from both routes, orphans dropped, computer removed, classic-grid switch, Cancel writes nothing, portable path |
| `DisplayMonitorsTests` | Conversion for each platform from a fake screen list: DPR 1, 1.25, 1.5 and 2, mixed scaling, macOS unchanged, position kept on Windows |
| `DisplayLayoutWriterTests` | Real `ServerConfig` writer into the real `Config` parser: inactive output byte-identical to upstream's, active output accepted, removed computer, stale placement, tiny overlap, and 1,000 seeded random layouts that must all parse |
| `DisplayLayoutViewTests` | Offscreen, following `BridgeClientConfigDialogTests`: drag, snap, drop on shelf, drag from shelf, trash, new computer, double-click settings, rename, arrow keys, Delete, reported size, nothing under the map but the shelf, labels never cut, Save on a layout-only change, Cancel, Reset, classic-grid button, external config, Bridge removal prompt through `screenDeleted` |
| `DisplayLayoutBondingTests` | Exact placement per profile, profiles with only the old offset, old offset still written, profile switch moves the client, classic grid still correct |
| `LayoutHooksTests` | Marker count per upstream file against Scenario U's table, and active output has no upstream neighbour lines. Fails if a merge drops a hook |

### Later phases

| Phase | Test | Must cover |
| :---: | :--- | :--- |
| 2 | `DisplayRectsTests` | Union of displays equals the core's box, a list is returned, empty on Wayland |
| 2 | `ComputerShapeIpcTests` | Message format, client connected, client resized, server display list, malformed message ignored, size stored for an offline client, fallback to Qt without a core, daemon mode |
| 3 | `DisplayEdgesTests` | Free edge at a column or row for every `reach` shape, enclosed hole stays closed, empty list changes nothing |
| 3 | `ServerInnerEdgeTests` | Real `Server` and `Config`: all eight rows of finding 4, all four sides, both edges of a notch corner, jump zone above 1, switch delay, double tap, cursor lock, an offline client skipped, a chain from a notch, list refresh on shape change.|
| 4 | `DisplayLayoutFollowTests` | Debounce, no apply when links are unchanged, one apply when they change, dock and undock, client resize |
| 5 | `PortalBarrierTests` | Pure barrier builder: inclusive coordinates, whole and part edges, seams skipped, the KDE whole-edge rule |

### Running them

These commands were checked on macOS with an existing test. `TEST_BUILD` is a folder outside
the repo. On macOS, also pass `-DCMAKE_OSX_SYSROOT="$(xcrun --show-sdk-path)"` when configuring.

```sh
cmake -S . -B "$TEST_BUILD" -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH" -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build "$TEST_BUILD" -j8 --target layout-tests
env -u XDG_CONFIG_HOME -u XDG_STATE_HOME HOME="$(mktemp -d)" \
  ctest --test-dir "$TEST_BUILD/src/unittests" --no-tests=error --output-on-failure \
  -R 'DisplayLayout|DisplayMonitors|DisplayEdges|DisplayRects|ComputerShapeIpc|ServerInnerEdge|LayoutHooks|PortalBarrier'
```

- Phase 1 adds the `layout-tests` target in `src/unittests/bridge/CMakeLists.txt`.
- Run `ctest` from `src/unittests`. From the build root it finds no tests and still exits 0.
- `--no-tests=error` makes an empty run fail.
- Always pass `--target layout-tests`. A test build's default target includes upstream's
  `run_tests`, which runs the whole suite under your real `HOME`, so tests can touch real settings.

To measure coverage, add these to the configure line, because `-DENABLE_COVERAGE=ON` needs
`gcovr`, which isn't installed here:

```sh
-DCMAKE_CXX_FLAGS="-fprofile-instr-generate -fcoverage-mapping" \
-DCMAKE_EXE_LINKER_FLAGS="-fprofile-instr-generate -fcoverage-mapping"
```

Then run `ctest` with `LLVM_PROFILE_FILE="$TEST_BUILD/cov/%p.profraw"` and report per test:

```sh
xcrun llvm-profdata merge -sparse "$TEST_BUILD"/cov/*.profraw -o "$TEST_BUILD/cov/all.profdata"
xcrun llvm-cov report <test binary> -instr-profile="$TEST_BUILD/cov/all.profdata" <source file>
```

Build gate: `.agent/skills/merge-upstream-master/verify_merge.sh`, ending with
`./run_task.sh 1` on macOS.

### Manual checks

| Machine | Check |
| :--- | :--- |
| Mac with two offset displays | Cursor crosses only on green, returns at the same height, stops on the rest |
| Mac | Two clients on one side, each reached on its own part of the edge |
| Mac, Windows, X11 | Client in a notch: crosses at the step, returns onto the display, no bounce |
| Mac, Windows, X11 | Client touching both sides of a notch: both edges cross |
| GNOME Wayland | Notch barrier accepted, and input capture still engages if it isn't |
| Windows, displays at 100% and 150% | Rectangles match the display settings, entry height is right. **Test this first.** |
| Linux Wayland and X11 | Same, plus no extra portal prompt from opening or saving the dialog |
| Any | Bridge client on the shelf reached by hotkey, bonded profile switch moves it |
| Laptop | Dock and undock, nothing lost |
| Any | Existing grid user: identical behaviour until the first Save |
| Any | Delete `screen-layout.json`: back to the old grid, nothing lost |

## Not building

- Crossing into a hole enclosed by displays, such as inside a C shape.
- Showing a client's separate displays. This needs a protocol change.
- Moving server displays from this dialog. The OS owns them.
- Separate saved layouts for docked and undocked.
- Rewriting `ServerConfig` or `ComputerSetupModel`.
- Translating new strings. They stay `unfinished` until requested.

## Assumptions

- **Most fragile:** the GUI's display rectangles match the core's box on Windows with
  mixed scaling. If they don't, ranges are off by a proportion, and crossings land in
  the wrong place rather than failing. Phase 2 removes this by using the same system calls
  as the core, but only a Windows test can confirm it.
- Inner edges behave on real hardware as they do in `ServerInnerEdgeTests`, against the
  real `Server` with a simulated display list. macOS is confirmed. If another platform's
  cursor doesn't report the last row of a free edge, that platform stays on outer edges.
  Nothing else depends on it.
- `feat/no-legacy-conf` doesn't land during this work. If it does, apply the fallback
  above before Phase 1 ships. The inner-edge hooks don't depend on the text config.

## Rollback

No data is migrated. The grid in `internalConfig` is never rewritten, so "Use the
classic grid" or deleting `screen-layout.json` restores the old behaviour. Reverting
the commits leaves one unused file behind.

## What has and hasn't run

| Part | macOS | Windows | X11 | Wayland |
| :--- | :--- | :--- | :--- | :--- |
| Whole app builds | Yes | **Never built** | Yes, on Ubuntu 26.04 | Same build as X11 |
| Unit tests | All 51 | Not run | All 53 | Same build as X11 |
| Editor, links, store, Bonding | Unit tests | Not built | Unit tests | Unit tests |
| `displayRects()` | Real displays | Syntax check only | Virtual X server with Xinerama | Empty by design |
| Inner edges in the server | Real `Server`, simulated displays | Not built | Real `Server`, simulated displays | Not used |
| Barriers (`PortalBarriers`) | Unit tests | n/a | n/a | Unit tests |
| Portal hooks, including the retry without free edges | n/a | n/a | n/a | Built, never run |
| A real cursor crossing | Real client, through the Dock, off it and on an inner edge | **Never run** | **Never run** | **Never run** |

`OSXKeyStateTests` also fails now and then on a Mac with DShare-HID running, because it sends real
key presses. It has nothing to do with the layout.

To run the X11 display code without an X session, on a machine with `Xvfb`:

```sh
Xvfb :77 -screen 0 1600x900x24 -screen 1 1280x1024x24 +xinerama -nolisten tcp &
DISPLAY=:77 XDG_SESSION_TYPE=x11 QT_QPA_PLATFORM=offscreen "$TEST_BUILD/src/unittests/bridge/DisplayRectsTests"
```

Try the manual checks above before merging, starting with Windows at mixed scaling, then Wayland
on GNOME 50.

### When the displays change

macOS and X11 report every display change, so `DisplayEdges::refresh()` runs from
`handleShapeChanged()`. Windows only reports a change of the whole box, so a display moved inside
the same box would leave an old list. There, `project()` reads the displays again once a second
while the mouse moves on the server. This hasn't run on Windows.

### Crossing through the Dock on macOS

Inner edges made the bottom of a display a crossing point, and the Dock sits there. macOS ignores
`CGDisplayHideCursor` while the pointer is over the Dock's icons, with no error. A warp doesn't
help, because the Dock only lets go when a real move reaches it, and the tap swallows moves once
the server has left. `CGAssociateMouseAndMouseCursorPosition(false)` doesn't hold from a background
process either, so the cursor stayed on show and followed the mouse.

`OSXComputer::hideOffTheDock()` checks whether the hide took. If not, it posts one real move to the
middle of the main display and hides again, before the tap starts swallowing. A crossing anywhere
else is unchanged. The Dock is the only place on this Mac where the hide failed.

To test this without a hand on the mouse, use `IOHIDPostEvent` with
`kIOHIDSetRelativeCursorPosition`. Events from `CGEventPost` don't reproduce it. While the tap
swallows moves, `CGEventCreate(NULL)` and `CGCursorIsVisible` are the only readings, and the first
goes stale, so it can read as frozen while the real pointer moves.

## Limits of this research

These describe the research, before anything was built. "What has and hasn't run" has the
current state.

- The parser and server tests ran on macOS only. Nothing was run on Windows or Linux.
- The server test used fake clients and a simulated display list. No real cursor was moved.
- The GNOME and KDE rules come from reading their current source. The GNOME reading
  covered `check_barrier` only, and KWin's own checks were not read.
- Upstream refs are from the last fetch (`upstream/master` at 2026-10-25), not refreshed.
- GitHub wasn't queried directly, and web search found no upstream issue for this, so
  one may exist.
