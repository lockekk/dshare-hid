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

#include <QSettings>

BridgeSocketFactory::BridgeSocketFactory(IEventQueue *events, SocketMultiplexer *socketMultiplexer) :
    m_events(events),
    m_socketMultiplexer(socketMultiplexer)
{
}

SecurityLevel BridgeSocketFactory::getServerSecurityLevel() const
{
  // Read TLS setting from server's main config file
  // Note: This is different from the bridge client's own config file
  QString serverConfigPath = Settings::UserSettingFile;

  QSettings serverConfig(serverConfigPath, QSettings::IniFormat);
  bool tlsEnabled = serverConfig.value(Settings::Security::TlsEnabled, false).toBool();

  // Bridge clients always use Encrypted (not PeerAuth) to avoid fingerprint verification
  SecurityLevel level = tlsEnabled ? SecurityLevel::Encrypted : SecurityLevel::PlainText;

  LOG_DEBUG1(
      "bridge client TLS: %s (from server config: %s)",
      tlsEnabled ? "enabled (Encrypted)" : "disabled (PlainText)",
      serverConfigPath.toUtf8().constData()
  );

  return level;
}

IDataSocket *BridgeSocketFactory::create(
    IArchNetwork::AddressFamily family, SecurityLevel securityLevel
) const
{
  // Override security level with server's TLS setting
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
