# SPDX-FileCopyrightText: 2024 Deskflow Developers
# SPDX-FileCopyrightText: 2012 - 2024 Symless Ltd
# SPDX-FileCopyrightText: 2009 - 2012 Nick Bolton
# SPDX-License-Identifier: MIT

include_directories(./lib)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/lib)
include_directories(${CMAKE_SOURCE_DIR}/submodules/esp32-hid-tools)

add_subdirectory(lib)
add_subdirectory(apps)
add_subdirectory(${CMAKE_SOURCE_DIR}/submodules/esp32-hid-tools ${CMAKE_BINARY_DIR}/submodules/esp32-hid-tools)

option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
  add_subdirectory(unittests)
endif()

