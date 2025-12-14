set(bridge_sources
    core/BridgeClientConfigManager.cpp
    core/BridgeClientConfigManager.h
    devices/UsbDeviceMonitor.cpp
    devices/UsbDeviceMonitor.h
    dialogs/BridgeClientConfigDialog.cpp
    dialogs/BridgeClientConfigDialog.h
    widgets/BridgeClientWidget.cpp
    widgets/BridgeClientWidget.h
    widgets/Esp32HidToolsWidget.cpp
    widgets/Esp32HidToolsWidget.h
    DeskflowHidExtension.cpp
    DeskflowHidExtension.h
)

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
endif()

target_sources(gui PRIVATE ${bridge_sources})

target_include_directories(gui PRIVATE
  ${CMAKE_SOURCE_DIR}/submodules/esp32-hid-tools
)

target_link_libraries(gui
  flash_tool_lib
  downloader_lib
)

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
