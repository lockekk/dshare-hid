# SPDX-FileCopyrightText: 2024 Chris Rizzitello <sithlord48@gmail.com>
# SPDX-FileCopyrightText: 2012 - 2024 Symless Ltd
# SPDX-FileCopyrightText: 2009 - 2012 Nick Bolton
# SPDX-License-Identifier: MIT

# Daemon is only needed on Windows for elevating processes to deal with UAC.
if(WIN32)
  set(target ${CMAKE_PROJECT_NAME}-daemon)

  # Generate rc file
  set(EXE_DESCRIPTION "${CMAKE_PROJECT_PROPER_NAME} Daemon for handling secure desktops (UAC prompts, login screen, etc)")
  set(EXE_ICON "IDI_DESKFLOW ICON DISCARDABLE \"${CMAKE_SOURCE_DIR}/src/apps/res/dshare-hid.ico\"")
  configure_file(${CMAKE_SOURCE_DIR}/src/apps/res/windows.rc.in ${target}.rc)

  add_executable(
    ${target} WIN32
    deskflow-daemon.cpp
    DaemonApp.cpp
    DaemonApp.h
    ${CMAKE_CURRENT_BINARY_DIR}/${target}.rc)

  set_target_properties(${target} PROPERTIES OUTPUT_NAME "dshare-hid-daemon")

  target_link_libraries(
    ${target}
    arch
    base
    io
    mt
    net
    platform
    app
    common
    ${libs}
    Qt6::Core)

  # Only install the daemon if specifically requested (not included in default dshare-hid MSI)
  option(INSTALL_DAEMON "Install the daemon executable" OFF)
  if(INSTALL_DAEMON)
    install(
      TARGETS ${target}
      RUNTIME_DEPENDENCY_SET daemonDeps
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    install(RUNTIME_DEPENDENCY_SET daemonDeps
      PRE_EXCLUDE_REGEXES ${WIN32_PRE_EXCLUDE_REGEXES}
      POST_EXCLUDE_REGEXES ${WIN32_POST_EXCLUDE_REGEXES}
      RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
  endif()
endif()
