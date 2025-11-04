/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "BridgeSocketFactory.h"

#include "base/Log.h"
#include "common/Settings.h"
#include "net/SecureListenSocket.h"
#include "net/SecureSocket.h"
#include "net/SecurityLevel.h"
#include "net/TCPListenSocket.h"
#include "net/TCPSocket.h"

BridgeSocketFactory::BridgeSocketFactory(IEventQueue *events, SocketMultiplexer *socketMultiplexer) :
    m_events(events),
    m_socketMultiplexer(socketMultiplexer)
{
}

SecurityLevel BridgeSocketFactory::getServerSecurityLevel() const
{
  // CLI `--secure` flag (handled by CoreArgParser) stores desired TLS state in settings
  bool tlsEnabled = Settings::value(Settings::Security::TlsEnabled).toBool();

  // Bridge clients follow CLI-selected TLS preference, enforcing PeerAuth when enabled
  SecurityLevel level = tlsEnabled ? SecurityLevel::PeerAuth : SecurityLevel::PlainText;

  LOG_DEBUG1(
      "bridge client TLS: %s (source=cli --secure)",
      tlsEnabled ? "enabled (PeerAuth)" : "disabled (PlainText)"
  );

  return level;
}

IDataSocket *BridgeSocketFactory::create(
    IArchNetwork::AddressFamily family, SecurityLevel securityLevel
) const
{
  // Override security level with CLI-provided TLS setting
  SecurityLevel actualLevel = getServerSecurityLevel();

  // Create socket based on security level
  if (actualLevel != SecurityLevel::PlainText) {
    auto *secureSocket = new SecureSocket(m_events, m_socketMultiplexer, family, actualLevel);
    secureSocket->initSsl(false);
    return secureSocket;
  } else {
    return new TCPSocket(m_events, m_socketMultiplexer, family);
  }
}

IListenSocket *BridgeSocketFactory::createListen(
    IArchNetwork::AddressFamily family, SecurityLevel securityLevel
) const
{
  // Bridge clients don't need listen sockets (they only connect to server)
  // But implement for completeness
  SecurityLevel actualLevel = getServerSecurityLevel();

  if (actualLevel != SecurityLevel::PlainText) {
    return new SecureListenSocket(m_events, m_socketMultiplexer, family, actualLevel);
  } else {
    return new TCPListenSocket(m_events, m_socketMultiplexer, family);
  }
}
