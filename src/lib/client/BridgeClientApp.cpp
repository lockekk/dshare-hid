/*
 * Deskflow-hid -- created by locke.huang@gmail.com
 */

#include "BridgeClientApp.h"

#include "BridgeSocketFactory.h"
#include "base/Event.h"
#include "base/EventQueue.h"
#include "base/Log.h"
#include "common/ExitCodes.h"
#include "common/Settings.h"
#include "deskflow/DeskflowException.h"
#include "deskflow/Screen.h"
#include "net/SocketMultiplexer.h"
#include "platform/bridge/BridgePlatformScreen.h"

BridgeClientApp::BridgeClientApp(
    IEventQueue *events, const QString &processName, std::shared_ptr<deskflow::bridge::CdcTransport> transport,
    const deskflow::bridge::FirmwareConfig &config, int32_t screenWidth, int32_t screenHeight
)
    : ClientApp(events, processName),
      m_transport(transport),
      m_config(config),
      m_screenWidth(screenWidth),
      m_screenHeight(screenHeight)
{
  LOG_INFO("BridgeClientApp: initialized for screen=%dx%d", m_screenWidth, m_screenHeight);
}

void BridgeClientApp::initApp()
{
  ClientApp::initApp();
}

deskflow::Screen *BridgeClientApp::createScreen()
{
  LOG_INFO("BridgeClientApp: creating BridgePlatformScreen");

  // Create BridgePlatformScreen instead of platform-specific screen
  const bool invertScrolling = Settings::value(Settings::Client::InvertScrollDirection).toBool();
  auto *platformScreen = new deskflow::bridge::BridgePlatformScreen(
      getEvents(), m_transport, m_screenWidth, m_screenHeight, invertScrolling
  );

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

void BridgeClientApp::handleScreenError() const
{
  LOG_CRIT("Bridge screen fatal error detected. Exiting for restart.");
  throw ExitAppException(s_exitFailed);
}

void BridgeClientApp::handleClientFailed(const Event &e)
{
  LOG_WARN("Server connection failed. Exiting for restart.");
  throw ExitAppException(s_exitFailed);
}

void BridgeClientApp::handleClientDisconnected()
{
  LOG_IPC("Server disconnected. Exiting for restart.");
  throw ExitAppException(s_exitSuccess);
}
