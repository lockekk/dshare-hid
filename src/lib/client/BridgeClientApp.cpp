/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "BridgeClientApp.h"

#include "BridgeSocketFactory.h"
#include "base/Log.h"
#include "deskflow/Screen.h"
#include "net/SocketMultiplexer.h"
#include "platform/bridge/BridgePlatformScreen.h"

BridgeClientApp::BridgeClientApp(
    IEventQueue *events, const QString &processName, std::shared_ptr<deskflow::bridge::CdcTransport> transport,
    const deskflow::bridge::FirmwareConfig &config, int32_t screenWidth, int32_t screenHeight
) :
    ClientApp(events, processName),
    m_transport(transport),
    m_config(config),
    m_screenWidth(screenWidth),
    m_screenHeight(screenHeight)
{
  LOG_INFO(
      "BridgeClientApp: initialized for screen=%dx%d",
      m_screenWidth,
      m_screenHeight
  );
}

deskflow::Screen *BridgeClientApp::createScreen()
{
  LOG_INFO("BridgeClientApp: creating BridgePlatformScreen");

  // Create BridgePlatformScreen instead of platform-specific screen
  auto *platformScreen =
      new deskflow::bridge::BridgePlatformScreen(getEvents(), m_transport, m_screenWidth, m_screenHeight);

  // Wrap in deskflow::Screen
  return new deskflow::Screen(platformScreen, getEvents());
}

ISocketFactory *BridgeClientApp::getSocketFactory() const
{
  // Bridge clients use BridgeSocketFactory which:
  // - Reads TLS preference provided by CLI (--secure) from Settings
  // - Uses SecurityLevel::PeerAuth (with fingerprint verification) when TLS is enabled
  return new BridgeSocketFactory(getEvents(), getSocketMultiplexer());
}
