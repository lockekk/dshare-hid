/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#include "BridgeClientApp.h"

#include "BridgeSocketFactory.h"
#include "base/Event.h"
#include "base/EventQueue.h"
#include "base/Log.h"
#include "common/ExitCodes.h"
#include "common/Settings.h"
#include "deskflow/DeskflowException.h"
#include "deskflow/Computer.h"
#include "net/SocketMultiplexer.h"
#include "platform/bridge/BridgePlatformScreen.h"

#include <string>
#include <thread>

#if defined(Q_OS_UNIX)
#include <cerrno>
#include <unistd.h>
#elif defined(Q_OS_WIN)
#include <windows.h>
#endif

namespace {

// The GUI talks to a bridge core over its stdin, one command per line:
// - "stop" shuts it down (BridgeClientProcess::stop): on Windows QProcess::terminate() can't
//   stop a process without windows, and a kill skips the clean shutdown that releases keys
//   still held on the device.
// - "req ..." asks it to talk to the device it holds (see platform/bridge/BridgeRequests.h).
//   The screen carries requests out on the event thread, which owns the transport.
// Read the raw handle, not std::cin, so the blocked read holds no CRT lock that exit() would
// wait for.
void readStdinCommands(IEventQueue *events, std::shared_ptr<deskflow::bridge::requests::Queue> requests)
{
  std::thread([events, requests] {
    std::string line;
    char c = 0;
    for (;;) {
#if defined(Q_OS_UNIX)
      const ssize_t n = ::read(STDIN_FILENO, &c, 1);
      if (n < 0 && errno == EINTR) {
        continue;
      }
      if (n != 1) {
        return; // no stdin, or the GUI closed it: nothing to wait for
      }
#elif defined(Q_OS_WIN)
      DWORD n = 0;
      if (!ReadFile(GetStdHandle(STD_INPUT_HANDLE), &c, 1, &n, nullptr) || n != 1) {
        return;
      }
#endif
      if (c != '\n') {
        line += c;
        continue;
      }
      if (!line.empty() && line.back() == '\r') {
        line.pop_back();
      }
      if (line == "stop") {
        requests->postUnlessClosed([events] {
          LOG_INFO("BridgeClientApp: stop requested, shutting down");
          events->addEvent(Event(EventTypes::Quit));
        });
        return;
      }
      if (QString::fromStdString(line).startsWith(deskflow::bridge::requests::kRequest + QLatin1Char(' '))) {
        requests->push(line);
        requests->postUnlessClosed([events, &requests] {
          events->addEvent(Event(EventTypes::StreamInputReady, requests.get()));
        });
      }
      line.clear();
    }
  }).detach();
}

} // namespace

BridgeClientApp::BridgeClientApp(
    IEventQueue *events, const QString &processName, std::shared_ptr<deskflow::bridge::CdcTransport> transport,
    const deskflow::bridge::FirmwareConfig &config, int32_t screenWidth, int32_t screenHeight
)
    : ClientApp(events, processName + "[BRIDGE]"),
      m_transport(transport),
      m_requests(std::make_shared<deskflow::bridge::requests::Queue>()),
      m_config(config),
      m_screenWidth(screenWidth),
      m_screenHeight(screenHeight)
{
  LOG_INFO("BridgeClientApp: initialized for screen=%dx%d", m_screenWidth, m_screenHeight);
}

BridgeClientApp::~BridgeClientApp()
{
  m_requests->close(); // the stdin reader thread lives on, but the event queue won't
}

void BridgeClientApp::initApp()
{
  ClientApp::initApp();
  readStdinCommands(getEvents(), m_requests);
}

deskflow::Computer *BridgeClientApp::createComputer()
{
  LOG_INFO("BridgeClientApp: creating BridgePlatformScreen");

  // Create BridgePlatformScreen instead of platform-specific screen
  auto *platformScreen =
      new deskflow::bridge::BridgePlatformScreen(getEvents(), m_transport, m_requests, m_screenWidth, m_screenHeight);

  // Wrap in deskflow::Computer
  return new deskflow::Computer(platformScreen, getEvents());
}

ISocketFactory *BridgeClientApp::getSocketFactory() const
{
  // Bridge clients use BridgeSocketFactory which:
  // - Reads TLS preference provided by CLI (--secure) from Settings
  // - Uses SecurityLevel::PeerAuth (with fingerprint verification) when TLS is enabled
  return new BridgeSocketFactory(getEvents(), getSocketMultiplexer());
}

void BridgeClientApp::handleComputerError() const
{
  LOG_CRIT("Bridge screen fatal error detected. Exiting for restart.");
  throw ExitAppException(s_exitFailed);
}

void BridgeClientApp::handleClientConnected()
{
  ClientApp::handleClientConnected();
  // the GUI cancels its connect timeout when it sees this line (s_connectedRegex);
  // ClientApp only logs it at debug level, and without TLS nothing else matches
  LOG_INFO("BridgeClientApp: connected to server");
  LOG_INFO("BridgeClientApp: Pre-connecting to bridge device...");
  if (m_transport && !m_transport->open()) {
    LOG_WARN("BridgeClientApp: Failed to pre-connect to bridge device (%s)", m_transport->lastError().c_str());
  }
}

void BridgeClientApp::handleClientFailed(const Event &e)
{
  LOG_WARN("Server connection failed. Exiting for restart.");
  throw ExitAppException(s_exitFailed);
}

void BridgeClientApp::handleClientDisconnected()
{
  LOG_INFO("Server disconnected. Exiting for restart.");
  throw ExitAppException(s_exitSuccess);
}
