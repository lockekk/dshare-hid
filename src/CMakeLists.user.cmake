set(ESP32_HID_TOOLS_DIR "${CMAKE_SOURCE_DIR}/submodules/esp32-hid-tools")

if(EXISTS "${ESP32_HID_TOOLS_DIR}/CMakeLists.txt")
  set(HAS_ESP32_HID_TOOLS ON)
  message(STATUS "ESP32 HID Tools: Found at ${ESP32_HID_TOOLS_DIR}")
  add_subdirectory(${ESP32_HID_TOOLS_DIR} ${CMAKE_BINARY_DIR}/submodules/esp32-hid-tools)
  include_directories(${ESP32_HID_TOOLS_DIR})
else()
  set(HAS_ESP32_HID_TOOLS OFF)
  message(STATUS "ESP32 HID Tools: Submodule not found at ${ESP32_HID_TOOLS_DIR}, disabling.")
endif()

# Handle optional custom auth key for CDC Transport
if(DESKFLOW_CDC_HANDSHAKE_KEY)
   message(STATUS "Using custom auth key for CDC Transport: ${DESKFLOW_CDC_HANDSHAKE_KEY}")

   if(NOT EXISTS "${DESKFLOW_CDC_HANDSHAKE_KEY}")
       message(FATAL_ERROR "DESKFLOW_CDC_HANDSHAKE_KEY specified but file not found: ${DESKFLOW_CDC_HANDSHAKE_KEY}")
   endif()

   # Define variables for key generation
   # We generate it in the src build directory to be easily included
   set(AUTH_KEY_INC_NAME "deskflow_auth_key.inc")
   set(AUTH_KEY_INC_PATH "${CMAKE_BINARY_DIR}/src/${AUTH_KEY_INC_NAME}")
   set(BIN2ARRAY_SCRIPT "${CMAKE_SOURCE_DIR}/cmake/bin2array.cmake")

   if(NOT EXISTS "${BIN2ARRAY_SCRIPT}")
        message(FATAL_ERROR "bin2array.cmake script not found at ${BIN2ARRAY_SCRIPT}")
   endif()

   # Create custom command to generate the header
   add_custom_command(
       OUTPUT "${AUTH_KEY_INC_PATH}"
       COMMAND "${CMAKE_COMMAND}" "-DINPUT_FILE=${DESKFLOW_CDC_HANDSHAKE_KEY}" "-DOUTPUT_FILE=${AUTH_KEY_INC_PATH}" -P "${BIN2ARRAY_SCRIPT}"
       DEPENDS "${DESKFLOW_CDC_HANDSHAKE_KEY}" "${BIN2ARRAY_SCRIPT}"
       COMMENT "Generating auth key include from ${DESKFLOW_CDC_HANDSHAKE_KEY}"
       VERBATIM
   )

   # Create a custom target so we can depend on it
   add_custom_target(generate_auth_key DEPENDS "${AUTH_KEY_INC_PATH}")

   # Add definitions and includes globally for the src tree (affects subdirs like lib/platform)
   add_compile_definitions(DESKFLOW_USE_GENERATED_AUTH_KEY)
   include_directories("${CMAKE_BINARY_DIR}/src")
else()
   message(STATUS "Using default hardcoded auth key for CDC Transport")
endif()
