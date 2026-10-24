# dshare-hid -- created by locke.huang@gmail.com

# the bridge's device request protocol (platform/bridge/BridgeRequests.h)
create_test(
  NAME BridgeRequestsTests
  DEPENDS platform
  LIBS base arch
  SOURCE BridgeRequestsTests.cpp
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib/platform"
)
