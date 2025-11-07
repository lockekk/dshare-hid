/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2002 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "arch/Arch.h"
#include "base/EventQueue.h"
#include "base/Log.h"
#include "client/BridgeClientApp.h"
#include "common/ExitCodes.h"
#include "common/Settings.h"
#include "deskflow/ClientApp.h"
#include "deskflow/CoreArgParser.h"
#include "deskflow/ServerApp.h"
#include "platform/bridge/CdcTransport.h"

#if SYSAPI_WIN32
#include "arch/win32/ArchMiscWindows.h"
#include <QCoreApplication>
#endif

#include <QFileInfo>
#include <QSharedMemory>
#include <QTextStream>
#include <iostream>

void showHelp(const CoreArgParser &parser)
{
  QTextStream(stdout) << parser.helpText();
}

int main(int argc, char **argv)
{
#if SYSAPI_WIN32
  // HACK to make sure settings gets the correct qApp path
  QCoreApplication m(argc, argv);
  m.deleteLater();

  ArchMiscWindows::setInstanceWin32(GetModuleHandle(nullptr));
#endif

  Arch arch;
  arch.init();

  Log log;

  QStringList args;
  for (int i = 0; i < argc; i++)
    args.append(argv[i]);

  // Step 1: Early lightweight argument pre-parse to extract --name and detect mode
  QString instanceName = "default";
  QString linkDevice;
  bool isClient = false;
  bool isServer = false;
  bool isBridgeClient = false;

  QString configOverride;

  for (int i = 1; i < argc; i++) {
    const QString arg = QString::fromUtf8(argv[i]);

    if ((arg == "--name" || arg == "-n") && (i + 1 < argc)) {
      instanceName = QString::fromUtf8(argv[++i]);
    } else if (arg == "--link" && (i + 1 < argc)) {
      linkDevice = QString::fromUtf8(argv[++i]);
      isBridgeClient = true;
      isClient = true; // Bridge client is a type of client
    } else if ((arg == "--settings" || arg == "-s") && (i + 1 < argc)) {
      configOverride = QString::fromUtf8(argv[++i]);
    } else if (arg == "client") {
      isClient = true;
    } else if (arg == "server") {
      isServer = true;
    }
  }

  Settings::setBridgeClientMode(isBridgeClient);

  // Determine initial settings file before constructing CoreArgParser so CLI overrides take effect.
  QString initialSettingsFile;
  if (isBridgeClient) {
    // Bridge clients use: ~/.config/deskflow/bridge-clients/<client-name>.conf
    initialSettingsFile = QStringLiteral("%1/bridge-clients/%2.conf")
                              .arg(Settings::settingsPath(), instanceName);
  } else if (!configOverride.isEmpty()) {
    initialSettingsFile = configOverride;
  }

  if (!initialSettingsFile.isEmpty()) {
    Settings::setSettingFile(initialSettingsFile);
  }

  CoreArgParser parser(args);

  // Comment below until we are ready use only this parser
  // if (!parser.errorText().isEmpty()) {
  //   QTextStream(stdout) << parser.errorText() << "\nUse --help for more information.";
  //   return s_exitFailed;
  // }

  if (parser.help()) {
    showHelp(parser);
    return s_exitSuccess;
  }

  if (parser.version()) {
    QTextStream(stdout) << parser.versionText();
    return s_exitSuccess;
  }

  // Step 2: Build shared memory key based on role
  QString sharedMemKey;
  if (isClient) {
    sharedMemKey = QString("deskflow-core-client-%1").arg(instanceName);
  } else if (isServer) {
    sharedMemKey = "deskflow-core-server";
  } else {
    // Default to old behavior if mode not detected yet
    sharedMemKey = "deskflow-core";
  }

  // Step 3: Create shared memory segment with the constructed key
  // This is to prevent a new instance from running if one is already running
  QSharedMemory sharedMemory(sharedMemKey);

  // Attempt to attach first and detach in order to clean up stale shm chunks
  // This can happen if the previous instance was killed or crashed
  if (sharedMemory.attach())
    sharedMemory.detach();

  // If we can create 1 byte of SHM we are the only instance
  if (!sharedMemory.create(1)) {
    LOG_WARN("an instance of deskflow core is already running");
    return s_exitDuplicate;
  }

  // Step 4: Full argument parsing with CoreArgParser
  parser.parse();

  EventQueue events;
  const auto processName = QFileInfo(argv[0]).fileName();

  if (parser.serverMode()) {
    ServerApp app(&events, processName);
    return app.run();
  } else if (parser.clientMode()) {
    // Check if this is a bridge client
    if (isBridgeClient) {
      // Step 6: Bridge client initialization
      LOG_INFO("initializing bridge client with link device: %s", linkDevice.toUtf8().constData());

      // Create CDC transport
      auto transport = std::make_shared<deskflow::bridge::CdcTransport>(linkDevice);
      if (!transport->open()) {
        LOG_ERR(
            "failed to open CDC transport %s: %s",
            linkDevice.toUtf8().constData(),
            transport->lastError().c_str()
        );
        return s_exitFailed;
      }

      if (!transport->hasDeviceConfig()) {
        LOG_ERR("CDC handshake did not provide metadata");
        return s_exitFailed;
      }

      deskflow::bridge::FirmwareConfig config = transport->deviceConfig();

      LOG_INFO(
          "Firmware handshake: proto=%u hid_connected=%d host_os=%s ble_interval=%ums activated=%d fw_bcd=%u hw_bcd=%u",
          config.protocolVersion,
          config.hidConnected ? 1 : 0,
          config.hostOsString(),
          config.bleIntervalMs,
          config.productionActivated ? 1 : 0,
          static_cast<unsigned>(config.firmwareVersionBcd),
          static_cast<unsigned>(config.hardwareVersionBcd)
      );

      // Get screen info from CLI arguments (provided by GUI)
      int screenWidth = parser.screenWidth();
      int screenHeight = parser.screenHeight();
      QString screenOrientation = parser.screenOrientation();

      if (screenWidth <= 0 || screenHeight <= 0) {
        LOG_ERR(
            "Invalid screen dimensions from CLI arguments: %dx%d (use --screen-width and --screen-height)",
            screenWidth,
            screenHeight
        );
        return s_exitFailed;
      }

      LOG_INFO(
          "Screen config from CLI: %dx%d orientation=%s",
          screenWidth,
          screenHeight,
          screenOrientation.toUtf8().constData()
      );

      // Create and run bridge client
      BridgeClientApp app(&events, processName, transport, config, screenWidth, screenHeight);
      return app.run();
    } else {
      // Standard client
      ClientApp app(&events, processName);
      return app.run();
    }
  }

  return s_exitSuccess;
}
