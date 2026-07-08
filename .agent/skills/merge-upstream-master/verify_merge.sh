#!/usr/bin/env bash
# Post-merge session gate. Run before pushing after upstream merges.
#   1. Rebrand grep over files touched since a base ref (default origin/main).
#   2. Build via ./run_task.sh 1 — on macOS this also re-runs macdeployqt +
#      codesign so the build-tree app bundle stays launchable (Scenario P:
#      a raw `cmake --build build` inside a deployed bundle leaves a hybrid
#      that loads two Qt copies and cannot start).
#   3. If a DMG tree exists, the OpenSSL install-name sweep (Scenario M).
#
# Usage: verify_merge.sh [base-ref]
set -uo pipefail
cd "$(git rev-parse --show-toplevel)"
base="${1:-origin/main}"
fail=0

echo "=== 1/3 rebrand watch (vs $base)"
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

echo "=== 2/3 build (+ macdeployqt/codesign on macOS)"
if ./run_task.sh 1 | tail -3 | grep -q "Build complete"; then
  echo "build OK"
else
  echo "BUILD FAILED"
  fail=1
fi

echo "=== 3/3 DMG OpenSSL sweep"
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

exit "$fail"
