# SPDX-FileCopyrightText: 2024 Chris Rizzitello <sithlord48@gmail.com>
# SPDX-License-Identifier: MIT

# HACK This is set when the files is included so its the real path
# calling CMAKE_CURRENT_LIST_DIR after include would return the wrong scope var
set(MY_DIR ${CMAKE_CURRENT_LIST_DIR})
set(OSX_BUNDLE ${BUILD_OSX_BUNDLE})

if(CMAKE_OSX_ARCHITECTURES MATCHES ";")
  set(OS_STRING "macos-universal")
else()
  set(OS_STRING "macos-${BUILD_ARCHITECTURE}")
endif()

if(NOT DEFINED OSX_CODESIGN_IDENTITY)
  set(OSX_CODESIGN_IDENTITY "-")
endif()

if (OSX_BUNDLE)
  find_package(OpenSSL QUIET)
  set(OSSL_MOD_PATH "")
  if (OPENSSL_FOUND)
      get_filename_component(OPENSSL_LIB_DIR "${OPENSSL_CRYPTO_LIBRARY}" DIRECTORY)
      if (EXISTS "${OPENSSL_LIB_DIR}/ossl-modules")
          set(OSSL_MOD_PATH "${OPENSSL_LIB_DIR}/ossl-modules")
          message(STATUS "Deployment will bundle OpenSSL modules from: ${OSSL_MOD_PATH}")
      endif()
  endif()

  # Configure the bundle post-processing script
  configure_file(
      "${MY_DIR}/post_bundle_process.cmake.in"
      "${CMAKE_CURRENT_BINARY_DIR}/post_bundle_process_generated.cmake"
      @ONLY
  )

  # Run the configured script at install time
  install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/post_bundle_process_generated.cmake")

  set(CPACK_PACKAGE_ICON "${MY_DIR}/dmg-volume.icns")
  set(CPACK_DMG_BACKGROUND_IMAGE "${MY_DIR}/dmg-background.tiff")
  set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${MY_DIR}/generate_ds_store.applescript")
  set(CPACK_DMG_VOLUME_NAME "${CMAKE_PROJECT_PROPER_NAME}")
  set(CPACK_DMG_SLA_USE_RESOURCE_FILE_LICENSE ON)
  set(CPACK_GENERATOR "DragNDrop")
endif()
