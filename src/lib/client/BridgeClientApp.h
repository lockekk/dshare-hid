/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/ClientApp.h"
#include "platform/bridge/CdcTransport.h"

#include <memory>

/**
 * @brief Bridge client application
 *
 * Derived from ClientApp to create a specialized client that:
 * - Connects to a modified Deskflow server
 * - Converts server events to HID reports
 * - Sends HID reports over USB CDC to Pico 2 W
 * - Rejects connections to upstream (unmodified) servers
 *
 * The bridge client owns the CDC transport Link and creates a
 * BridgePlatformScreen instead of a regular platform screen.
 */
class BridgeClientApp : public ClientApp
{
public:
  /**
   * @brief Construct bridge client with CDC transport and Pico config
   * @param events Event queue
   * @param processName Process name
   * @param transport CDC transport to Pico 2 W
   * @param config Pico configuration (arch + screen info)
   */
  BridgeClientApp(
      IEventQueue *events, const QString &processName, std::shared_ptr<deskflow::bridge::CdcTransport> transport,
      const deskflow::bridge::PicoConfig &config
  );

  ~BridgeClientApp() override = default;

  // Override createScreen to return BridgePlatformScreen
  deskflow::Screen *createScreen() override;

  /**
   * @brief Get the CDC transport
   */
  std::shared_ptr<deskflow::bridge::CdcTransport> getTransport() const
  {
    return m_transport;
  }

  /**
   * @brief Get the Pico configuration
   */
  const deskflow::bridge::PicoConfig &getConfig() const
  {
    return m_config;
  }

protected:
  /**
   * @brief Get socket factory for bridge client
   *
   * Returns BridgeSocketFactory that:
   * - Reads TLS setting from server's main config (not bridge client config)
   * - Uses SecurityLevel::Encrypted (not PeerAuth) when TLS is enabled
   */
  ISocketFactory *getSocketFactory() const override;

private:
  std::shared_ptr<deskflow::bridge::CdcTransport> m_transport;
  deskflow::bridge::PicoConfig m_config;
};
