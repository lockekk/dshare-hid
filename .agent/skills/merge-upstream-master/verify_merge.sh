#!/usr/bin/env bash
# Post-merge session gate. Run before pushing after upstream merges.
#   1. Rebrand grep over files touched since a base ref (default origin/main).
#   2. Build via ./run_task.sh 1 — on macOS this also re-runs macdeployqt +
#      codesign so the build-tree app bundle stays launchable (Scenario P:
#      a raw `cmake --build build` inside a deployed bundle leaves a hybrid
#      that loads two Qt copies and cannot start).
#   3. If a DMG tree exists, the OpenSSL install-name sweep (Scenario M).
#   4. If a merge touched a file with a layout hook, the layout tests (Scenario U):
#      a hook can break in a merge with no conflict.
#
# Usage: verify_merge.sh [base-ref]
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
base="${1:-origin/main}"
fail=0

echo "=== 1/4 rebrand watch (vs $base)"
touched=$(git diff --name-only "$base"...HEAD)
if [ -n "$touched" ]; then
  hits=$(echo "$touched" | xargs grep -ln 'Deskflow' 2>/dev/null \
        | xargs grep -n 'Deskflow' 2>/dev/null \
        | grep -vE 'deskflow::|DeskflowException|mouse and keyboard sharing|SPDX|deskflow/' | head -10)
  if [ -n "$hits" ]; then
    echo "review these (user-visible identity strings must say DShare-HID):"
    echo "$hits" | sed 's/^/    /'
  else
    echo "clean"
  fi
else
  echo "no changes vs $base"
fi

echo "=== 2/4 build (+ macdeployqt/codesign on macOS)"
if ./run_task.sh 1 | tail -3 | grep -q "Build complete"; then
  echo "build OK"
else
  echo "BUILD FAILED"
  fail=1
fi

echo "=== 3/4 DMG OpenSSL sweep"
core=$(ls build_dmg/_CPack_Packages/Darwin/DragNDrop/*/DShare-HID.app/Contents/MacOS/dshare-hid-core 2>/dev/null | head -1)
if [ -n "$core" ]; then
  ssl=$(otool -L "$core" | grep -i ssl)
  echo "$ssl" | sed 's/^/    /'
  if echo "$ssl" | grep -qE '/(Users|Volumes|tmp)/'; then
    echo "REGRESSION: absolute OpenSSL path in DMG binary (see guide Scenario M)"
    fail=1
  else
    echo "sweep OK"
  fi
else
  echo "no DMG tree (run ./run_task.sh 5 for release verification)"
fi

echo "=== 4/4 layout hooks (Scenario U)"
hooked=$(git diff --name-only "$base"...HEAD -- \
  src/lib/gui/config/ServerConfig.cpp src/lib/gui/dialogs/ServerConfigDialog.cpp \
  src/lib/gui/ComputerSetupModel.h src/lib/gui/ComputerSetupModel.cpp \
  src/lib/gui/widgets/ComputerSetupView.cpp src/lib/gui/widgets/NewComputerWidget.cpp \
  src/lib/gui/widgets/RemoveComputerWidget.cpp src/lib/gui/core/CoreProcess.cpp \
  src/lib/gui/core/CoreProcess.h src/lib/gui/ipc/IpcClient.cpp \
  src/lib/server/Server.cpp src/lib/server/Server.h src/lib/server/Config.cpp \
  src/lib/server/CMakeLists.txt src/lib/deskflow/ServerApp.cpp src/lib/deskflow/ipc \
  src/lib/platform/CMakeLists.txt src/lib/platform/PortalInputCapture.cpp \
  src/lib/platform/PortalInputCapture.h src/lib/platform/OSXComputer.mm \
  src/lib/platform/OSXComputer.h)
if [ -z "$hooked" ]; then
  echo "no hooked file changed"
else
  echo "$hooked" | sed 's/^/    /'
  # a build of its own, because run_task.sh always turns tests off
  test_build="${TEST_BUILD:-${TMPDIR:-/tmp}/dshare-layout-tests}"
  prefix=$(sed -n 's/^CMAKE_PREFIX_PATH:[A-Z]*=//p' build/CMakeCache.txt 2>/dev/null | head -1)
  sysroot=()
  if [ "$(uname)" = "Darwin" ]; then
    sysroot=(-DCMAKE_OSX_SYSROOT="$(xcrun --show-sdk-path)")
  fi
  if cmake -S . -B "$test_build" -DCMAKE_PREFIX_PATH="$prefix" "${sysroot[@]}" -DCMAKE_BUILD_TYPE=Debug \
       -DBUILD_TESTS=ON > /dev/null \
     && cmake --build "$test_build" -j8 --target layout-tests > /dev/null \
     && env -u XDG_CONFIG_HOME -u XDG_STATE_HOME HOME="$(mktemp -d)" \
          ctest --test-dir "$test_build/src/unittests" --no-tests=error --output-on-failure \
          -R 'DisplayLayout|DisplayMonitors|DisplayEdges|DisplayRects|ComputerShapeIpc|ServerInnerEdge|LayoutHooks|PortalBarrier'; then
    echo "layout tests OK"
  else
    echo "LAYOUT TESTS FAILED: see Scenario U in docs/upgrade_from_upstream_guide.md"
    fail=1
  fi
fi

exit "$fail"
