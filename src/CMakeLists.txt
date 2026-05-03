# SPDX-FileCopyrightText: 2024 Deskflow Developers
# SPDX-FileCopyrightText: 2012 - 2024 Symless Ltd
# SPDX-FileCopyrightText: 2009 - 2012 Nick Bolton
# SPDX-License-Identifier: MIT

# Include user-specific configuration early so HID options are available to all targets
include(CMakeLists.user.cmake OPTIONAL)

add_subdirectory(lib)
add_subdirectory(apps)

option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
  add_subdirectory(unittests)
endif()

