set(bridge_sources
    core/BridgeClientConfigManager.cpp
    core/BridgeClientConfigManager.h
    devices/UsbDeviceMonitor.cpp
    devices/UsbDeviceMonitor.h
    dialogs/BridgeClientConfigDialog.cpp
    dialogs/BridgeClientConfigDialog.h
    widgets/BridgeClientWidget.cpp
    widgets/BridgeClientWidget.h
    DeskflowHidExtension.cpp
    DeskflowHidExtension.h
)

set(AUTO_AUTH_KEY_PATH "${CMAKE_SOURCE_DIR}/firmware/build_upgrade/deskflow_auth_key.inc")
if(EXISTS "${AUTO_AUTH_KEY_PATH}")
  message(STATUS "Integrating custom auth key from: ${AUTO_AUTH_KEY_PATH}")

  configure_file("${AUTO_AUTH_KEY_PATH}" "${CMAKE_BINARY_DIR}/generated/deskflow_auth_key.inc" COPYONLY)

  target_include_directories(gui PRIVATE "${CMAKE_BINARY_DIR}/generated")
  target_compile_definitions(gui PRIVATE DESKFLOW_USE_GENERATED_AUTH_KEY)
endif()

if(UNIX AND NOT APPLE)
  list(APPEND bridge_sources
    devices/LinuxUdevMonitor.cpp
    devices/LinuxUdevMonitor.h
    devices/UsbDeviceHelper.cpp
    devices/UsbDeviceHelper.h
  )
elseif(WIN32)
  list(APPEND bridge_sources
    devices/WindowsUsbMonitor.cpp
    devices/WindowsUsbMonitor.h
    devices/UsbDeviceHelper.cpp
    devices/UsbDeviceHelper.h
  )
elseif(APPLE)
  list(APPEND bridge_sources
    devices/MacUsbMonitor.mm
    devices/MacUsbMonitor.h
    devices/UsbDeviceHelper.cpp
    devices/UsbDeviceHelper.h
  )

  find_library(IOKIT_LIBRARY IOKit)
  find_library(COREFOUNDATION_LIBRARY CoreFoundation)
  target_link_libraries(gui ${IOKIT_LIBRARY} ${COREFOUNDATION_LIBRARY})
endif()

set(ESP32_HID_TOOLS_DIR "${CMAKE_SOURCE_DIR}/submodules/esp32-hid-tools")
if(BUILD_HID AND EXISTS "${ESP32_HID_TOOLS_DIR}/CMakeLists.txt")
  message(STATUS "Enabling ESP32 HID Tools in GUI")
  list(APPEND bridge_sources
    widgets/Esp32HidToolsWidget.cpp
    widgets/Esp32HidToolsWidget.h
  )
  target_compile_definitions(gui PRIVATE DESKFLOW_ENABLE_ESP32_HID_TOOLS)

  target_include_directories(gui PRIVATE
    ${ESP32_HID_TOOLS_DIR}
  )

  target_link_libraries(gui
    flash_tool_lib
    downloader_lib
  )
endif()

target_sources(gui PRIVATE ${bridge_sources})

if(UNIX AND NOT APPLE)
  # Find libudev for USB device monitoring
  find_library(UDEV_LIBRARY udev)
  if(UDEV_LIBRARY)
    target_link_libraries(gui ${UDEV_LIBRARY})
    message(STATUS "Found libudev: ${UDEV_LIBRARY}")
  else()
    message(WARNING "libudev not found - USB device monitoring will not be available")
  endif()
endif()
