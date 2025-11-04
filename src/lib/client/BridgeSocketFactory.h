/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "net/ISocketFactory.h"

class IEventQueue;
class SocketMultiplexer;

/**
 * @brief Socket factory for bridge clients
 *
 * Bridge clients need special TLS handling:
 * - Read TLS preference from CLI `--secure` flag (persisted in Settings)
 * - Always use SecurityLevel::PeerAuth when TLS is enabled
 * - This keeps bridge clients aligned with upstream security expectations
 */
class BridgeSocketFactory : public ISocketFactory
{
public:
  BridgeSocketFactory(IEventQueue *events, SocketMultiplexer *socketMultiplexer);
  ~BridgeSocketFactory() override = default;

  // ISocketFactory overrides
  IDataSocket *create(
      IArchNetwork::AddressFamily family = IArchNetwork::AddressFamily::INet,
      SecurityLevel securityLevel = SecurityLevel::PlainText
  ) const override;

  IListenSocket *createListen(
      IArchNetwork::AddressFamily family = IArchNetwork::AddressFamily::INet,
      SecurityLevel securityLevel = SecurityLevel::PlainText
  ) const override;

private:
  /**
   * @brief Read TLS preference from settings (populated by CLI `--secure`)
   * @return SecurityLevel::PeerAuth if TLS is enabled, SecurityLevel::PlainText otherwise
   */
  SecurityLevel getServerSecurityLevel() const;

  IEventQueue *m_events;
  SocketMultiplexer *m_socketMultiplexer;
};
