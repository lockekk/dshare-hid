# dshare-hid -- created by dshare.hid@gmail.com

# the bridge's device request protocol (platform/bridge/BridgeRequests.h)
create_test(
  NAME BridgeRequestsTests
  DEPENDS platform
  LIBS base arch
  SOURCE BridgeRequestsTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/platform"
)

# the server's key events as the HID reports a bridge client sends (platform/bridge/BridgeKeyboard.h),
# for each kind of computer and both keyboard layouts
create_test(
  NAME BridgeKeyboardTests
  DEPENDS platform
  LIBS base arch
  SOURCE BridgeKeyboardTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/platform"
)

# the active profile as the device reports it (platform/bridge/CdcTransport.h), and the bridge
# screen's size that follows it (platform/bridge/BridgePlatformScreen.h), against a fake dongle
# on a pseudo-terminal
if(UNIX)
  create_test(
    NAME BridgeActiveProfileTests
    DEPENDS platform
    LIBS base arch
    SOURCE BridgeActiveProfileTests.cpp
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/platform"
  )
endif()

if(WIN32)
  set(server_extra_libs version app mt net)
endif()

# the server's hotkey action for the bridge profile shortcuts (server/BridgeProfileAction.h)
create_test(
  NAME BridgeProfileActionTests
  DEPENDS server
  LIBS base arch ${server_extra_libs}
  SOURCE BridgeProfileActionTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/server"
)

# the profile backup file (gui/core/BridgeProfileBackup.h)
create_test(
  NAME BridgeProfileBackupTests
  DEPENDS gui
  SOURCE BridgeProfileBackupTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

# the device requests behind backup, restore and the Configure dialog's OK
# (gui/core/BridgeDeviceService.h), against a fake device
create_test(
  NAME BridgeDeviceServiceTests
  DEPENDS gui
  SOURCE BridgeDeviceServiceTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)

# the Configure dialog's profiles: edits, Reset, Backup, Restore and OK
# (gui/dialogs/BridgeClientConfigDialog.h), against a fake device
create_test(
  NAME BridgeClientConfigDialogTests
  DEPENDS gui
  SOURCE BridgeClientConfigDialogTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
# its QApplication needs the offscreen platform plugin, which on Windows Qt looks for beside the
# test, where nothing deploys it: point Qt at its own plugins instead
if(WIN32 AND TARGET Qt6::QOffscreenIntegrationPlugin)
  file(GENERATE
    OUTPUT "$<TARGET_FILE_DIR:BridgeClientConfigDialogTests>/qt.conf"
    CONTENT "[Paths]\nPlugins = $<TARGET_FILE_DIR:Qt6::QOffscreenIntegrationPlugin>/..\n"
  )
endif()

# the notice to pair the hosts again after firmware 1.2.1 (gui/core/BridgeFirmwareNotice.h)
create_test(
  NAME BridgeFirmwareNoticeTests
  DEPENDS gui
  SOURCE BridgeFirmwareNoticeTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
