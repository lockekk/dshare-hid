/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "BridgeClientApp.h"

#include "base/Log.h"
#include "deskflow/Screen.h"
#include "platform/bridge/BridgePlatformScreen.h"

BridgeClientApp::BridgeClientApp(
    IEventQueue *events, const QString &processName, std::shared_ptr<deskflow::bridge::CdcTransport> transport,
    const deskflow::bridge::PicoConfig &config
) :
    ClientApp(events, processName),
    m_transport(transport),
    m_config(config)
{
  LOG_INFO(
      "BridgeClientApp: initialized for arch=%s screen=%dx%d", m_config.arch.c_str(), m_config.screenWidth,
      m_config.screenHeight
  );
}

deskflow::Screen *BridgeClientApp::createScreen()
{
  LOG_INFO("BridgeClientApp: creating BridgePlatformScreen");

  // Create BridgePlatformScreen instead of platform-specific screen
  auto *platformScreen = new deskflow::bridge::BridgePlatformScreen(getEvents(), m_transport, m_config);

  // Wrap in deskflow::Screen
  return new deskflow::Screen(platformScreen, getEvents());
}
