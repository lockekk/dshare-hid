/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 - 2016, 2025 - 2026 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2002 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CoreArgParser.h"

#include "arch/Arch.h"
#include "base/EventQueue.h"
#include "base/Log.h"
#include "client/BridgeClientApp.h"
#include "common/Constants.h"
#include "common/ExitCodes.h"
#include "common/Settings.h"
#include "deskflow/ClientApp.h"
#include "deskflow/ServerApp.h"
#include "deskflow/ipc/CoreIpcServer.h"
#include "platform/OpenSSLCompat.h"
#include "platform/bridge/CdcTransport.h"

#ifndef _WIN32
#include <signal.h>
#include <unistd.h>
#endif

#if defined(Q_OS_WIN)
#include "arch/win32/ArchMiscWindows.h"
#endif

#include <QApplication>
#include <QFileInfo>
#include <QSharedMemory>
#include <QTextStream>
#include <QThread>
#include <QtGlobal>

void qtMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
  const auto utf8 = message.toUtf8();
  switch (type) {
  case QtDebugMsg:
    CLOG->print(context.file, context.line, CLOG_TAG_DEBUG "%s", utf8.constData());
    break;
  case QtInfoMsg:
    CLOG->print(context.file, context.line, CLOG_TAG_INFO "%s", utf8.constData());
    break;
  case QtWarningMsg:
    CLOG->print(context.file, context.line, CLOG_TAG_WARN "%s", utf8.constData());
    break;
  case QtCriticalMsg:
    CLOG->print(context.file, context.line, CLOG_TAG_ERR "%s", utf8.constData());
    break;
  case QtFatalMsg:
    CLOG->print(context.file, context.line, CLOG_TAG_CRIT "%s", utf8.constData());
    break;
  }
  if (type == QtFatalMsg) {
    abort();
  }
}

void showHelp(const CoreArgParser &parser)
{
  QTextStream(stdout) << parser.helpText();
}

int main(int argc, char **argv)
{
#if defined(Q_OS_UNIX)
  signal(SIGPIPE, SIG_IGN);
#endif

#if defined(Q_OS_WIN)
  {
    // HACK to make sure settings gets the correct qApp path
    QCoreApplication m(argc, argv);
  }

  ArchMiscWindows::setInstanceWin32(GetModuleHandle(nullptr));
#endif

  Arch arch;
  arch.init();

  Log log;
  qInstallMessageHandler(qtMessageHandler);

  // Initialize OpenSSL 3.x providers/environment (macOS and Linux)
  deskflow::platform::initializeOpenSSL();

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
    initialSettingsFile = QStringLiteral("%1/bridge-clients/%2.conf").arg(Settings::settingsPath(), instanceName);
  } else if (!configOverride.isEmpty()) {
    initialSettingsFile = configOverride;
  }

  if (!initialSettingsFile.isEmpty()) {
    Settings::setSettingsFile(initialSettingsFile);
  }

  CoreArgParser parser(args);

  // Print any parser errors
  if (!parser.errorText().isEmpty()) {
    QTextStream(stdout) << parser.errorText() << "\n";
  }

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
    sharedMemKey = QString("dshare-hid-core-client-%1").arg(instanceName);
  } else if (isServer) {
    sharedMemKey = "dshare-hid-core-server";
  } else {
    // Default to old behavior if mode not detected yet
    sharedMemKey = "dshare-hid-core";
  }

  // Step 3: Create shared memory segment with the constructed key
  // This is to prevent a new instance from running if one is already running
  QSharedMemory sharedMemory(sharedMemKey);

  // Attempt to attach first and detach in order to clean up stale shm chunks
  // This can happen if the previous instance was killed or crashed
  if (sharedMemory.attach())
    sharedMemory.detach();

  if (!sharedMemory.create(1) && parser.singleInstanceOnly()) {
    LOG_WARN("an instance of dshare-hid core is already running");
    return s_exitDuplicate;
  }

  // Step 4: Full argument parsing with CoreArgParser
  parser.parse();

  EventQueue events;
  const auto processName = QFileInfo(argv[0]).fileName();

  // Step 5: Bridge client transport setup (must happen before constructing the App)
  std::shared_ptr<deskflow::bridge::CdcTransport> bridgeTransport;
  deskflow::bridge::FirmwareConfig bridgeConfig;
  int bridgeScreenWidth = 0;
  int bridgeScreenHeight = 0;
  if (isBridgeClient) {
    LOG_INFO("initializing bridge client with link device: %s", linkDevice.toUtf8().constData());

    bridgeTransport = std::make_shared<deskflow::bridge::CdcTransport>(linkDevice);
    if (!bridgeTransport->open()) {
      LOG_ERR(
          "failed to open CDC transport %s: %s", linkDevice.toUtf8().constData(), bridgeTransport->lastError().c_str()
      );
      return s_exitFailed;
    }

    if (!bridgeTransport->hasDeviceConfig()) {
      LOG_ERR("CDC handshake did not provide metadata");
      bridgeTransport->close();
      return s_exitFailed;
    }

    bridgeConfig = bridgeTransport->deviceConfig();

    LOG_INFO(
        "Firmware handshake: proto=%u activation_state=%s(%u) fw_bcd=%u hw_bcd=%u", bridgeConfig.protocolVersion,
        bridgeConfig.activationStateString(), static_cast<unsigned>(bridgeConfig.activationState),
        static_cast<unsigned>(bridgeConfig.firmwareVersionBcd), static_cast<unsigned>(bridgeConfig.hardwareVersionBcd)
    );

    bridgeScreenWidth = parser.screenWidth();
    bridgeScreenHeight = parser.screenHeight();

    if (bridgeScreenWidth <= 0 || bridgeScreenHeight <= 0) {
      LOG_ERR(
          "Invalid screen dimensions from CLI arguments: %dx%d (use --screen-width and --screen-height)",
          bridgeScreenWidth, bridgeScreenHeight
      );
      bridgeTransport->close();
      return s_exitFailed;
    }

    LOG_INFO("Screen config from CLI: %dx%d", bridgeScreenWidth, bridgeScreenHeight);
  }

  // Step 6: Construct the appropriate App (server / client / bridge client)
  App *coreApp = nullptr;
  if (parser.serverMode()) {
    coreApp = new ServerApp(&events, processName);
  } else if (parser.clientMode()) {
    if (isBridgeClient) {
      coreApp = new BridgeClientApp(
          &events, processName, bridgeTransport, bridgeConfig, bridgeScreenWidth, bridgeScreenHeight
      );
    } else {
      coreApp = new ClientApp(&events, processName);
    }
  }

  // Step 7: Create the QApplication, IPC server and run the App on a worker thread.
  // The QApplication is required so that platform-specific event loops (e.g. Cocoa on macOS)
  // are kept alive while the App logic runs in `coreThread`.
  QApplication app(argc, argv);
  QApplication::setApplicationName(QStringLiteral("%1 Core").arg(kAppName));

  const auto ipcServer = new deskflow::core::ipc::CoreIpcServer(&app); // NOSONAR - Qt managed
  QObject::connect(
      ipcServer, &deskflow::core::ipc::IpcServer::stopProcessRequested, coreApp, &App::quit, Qt::DirectConnection
  );
  ipcServer->listen();

  QThread coreThread;
  QObject::connect(&coreThread, &QThread::finished, &app, &QApplication::quit);
  coreApp->run(coreThread);

  int exitCode = QApplication::exec();
  coreThread.wait();

  if (exitCode == s_exitSuccess) {
    exitCode = coreApp->getExitCode();
  }

  // Bridge client cleanup: close the transport before exit
  if (isBridgeClient && bridgeTransport) {
    bridgeTransport->close();
  }

  LOG_DEBUG("core exited, code: %d", exitCode);
  return exitCode;
}
