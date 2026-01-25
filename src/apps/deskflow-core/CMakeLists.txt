# SPDX-FileCopyrightText: 2024 - 2025 Chris Rizzitello <sithlord48@gmail.com>
# SPDX-FileCopyrightText: 2012 - 2024 Symless Ltd
# SPDX-FileCopyrightText: 2009 - 2012 Nick Bolton
# SPDX-License-Identifier: MIT

set(target ${CMAKE_PROJECT_NAME}-core)

add_executable(${target}
  "deskflow-core.cpp"
  CoreArgs.h
  CoreArgParser.h
  CoreArgParser.cpp
)

set_target_properties(${target} PROPERTIES OUTPUT_NAME "dshare-hid-core")

if(WIN32)
  # Generate rc file
  set(EXE_DESCRIPTION "${CMAKE_PROJECT_PROPER_NAME} combined server and client application")
  set(EXE_ICON "IDI_DESKFLOW ICON DISCARDABLE \"${CMAKE_SOURCE_DIR}/src/apps/res/dshare-hid.ico\"")
  configure_file(${CMAKE_SOURCE_DIR}/src/apps/res/windows.rc.in ${target}.rc)

  target_sources(${target} PRIVATE
    "deskflow-core.exe.manifest"
    ${CMAKE_SOURCE_DIR}/src/apps/res/dshare-hid.ico
    ${CMAKE_CURRENT_BINARY_DIR}/${target}.rc
  )
endif()

target_link_libraries(
  ${target}
  arch
  base
  client
  io
  mt
  net
  platform
  server
  app
  ${libs})

install(
  TARGETS ${target}
  RUNTIME_DEPENDENCY_SET coreDeps
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(BUILD_OSX_BUNDLE)
  set_target_properties(${target} PROPERTIES
    INSTALL_RPATH "@loader_path/../Libraries;@loader_path/../Frameworks"
    RUNTIME_OUTPUT_DIRECTORY $<TARGET_BUNDLE_CONTENT_DIR:${CMAKE_PROJECT_PROPER_NAME}>/MacOS
  )
elseif (WIN32)
  install(RUNTIME_DEPENDENCY_SET coreDeps
    PRE_EXCLUDE_REGEXES ${WIN32_PRE_EXCLUDE_REGEXES}
    POST_EXCLUDE_REGEXES ${WIN32_POST_EXCLUDE_REGEXES}
    RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )
else()
  generate_app_man(${target} "${CMAKE_PROJECT_DESCRIPTION}")
endif()
