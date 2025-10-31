/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "HidFrame.h"

#include <QString>
#include <cstdint>
#include <string>
#include <vector>

namespace deskflow::bridge {

/**
 * @brief Pico configuration structure
 */
struct PicoConfig {
  std::string arch;           // e.g., "bridge-ios", "bridge-android"
  int32_t screenWidth = 1920;
  int32_t screenHeight = 1080;
  int32_t screenRotation = 0; // 0, 90, 180, 270
  float screenPhysicalWidth = 0.0f;  // in inches
  float screenPhysicalHeight = 0.0f; // in inches
  float screenScaleFactor = 1.0f;

  bool isValid() const
  {
    return !arch.empty() && screenWidth > 0 && screenHeight > 0;
  }
};

/**
 * @brief USB CDC transport helper for Pico 2 W communication
 *
 * Handles:
 * - Opening/closing CDC device
 * - Configuration exchange with Pico
 * - Sending HID frames
 * - Receiving responses (if needed)
 */
class CdcTransport
{
public:
  explicit CdcTransport(const QString &devicePath);
  ~CdcTransport();

  /**
   * @brief Open the CDC device
   * @return true if successful
   */
  bool open();

  /**
   * @brief Close the CDC device
   */
  void close();

  /**
   * @brief Check if device is open
   */
  bool isOpen() const;

  /**
   * @brief Send HID event packet to Pico
   * @return true if successful
   */
  bool sendHidEvent(const HidEventPacket &packet);

  /**
   * @brief Get last error message
   */
  std::string lastError() const
  {
    return m_lastError;
  }

private:
  bool performHandshake();
  bool sendUsbFrame(uint8_t type, uint8_t flags, const uint8_t *payload, uint16_t length);
  bool sendUsbFrame(uint8_t type, uint8_t flags, const std::vector<uint8_t> &payload);
  bool writeAll(const uint8_t *data, size_t length);
  bool readFrame(uint8_t &type, uint8_t &flags, std::vector<uint8_t> &payload, int timeoutMs);
  void resetState();
  bool ensureOpen();

  QString m_devicePath;
  int m_fd = -1; // File descriptor (Unix) or HANDLE (Windows)
  bool m_handshakeComplete = false;
  uint32_t m_lastNonce = 0;
  std::vector<uint8_t> m_rxBuffer;
  std::string m_lastError;
};

} // namespace deskflow::bridge
