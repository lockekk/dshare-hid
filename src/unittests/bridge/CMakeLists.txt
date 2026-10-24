# dshare-hid -- created by locke.huang@gmail.com

# the bridge's device request protocol (platform/bridge/BridgeRequests.h)
create_test(
  NAME BridgeRequestsTests
  DEPENDS platform
  LIBS base arch
  SOURCE BridgeRequestsTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/platform"
)

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

# the notice to pair the hosts again after firmware 1.2.1 (gui/core/BridgeFirmwareNotice.h)
create_test(
  NAME BridgeFirmwareNoticeTests
  DEPENDS gui
  SOURCE BridgeFirmwareNoticeTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/gui"
)
