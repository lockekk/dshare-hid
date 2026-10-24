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
