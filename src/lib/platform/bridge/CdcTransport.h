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
  int32_t screenWidth = 1080;
  int32_t screenHeight = 2424;
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
   * @brief Send a compact mouse movement frame
   *
   * Encodes the deltas directly in the USB link header so the on-wire
   * frame is exactly eight bytes (header only, no payload).
   */
  bool sendMouseMoveCompact(int16_t dx, int16_t dy);
  bool sendMouseButtonCompact(uint8_t buttons, bool isPress);
  bool sendMouseScrollCompact(int8_t delta);

  /**
   * @brief Send a compact keyboard event frame
   *
   * Encodes the modifier/keycode pair and press/release state directly in the
   * header so the on-wire frame is eight bytes with no payload.
   */
  bool sendKeyboardCompact(uint8_t modifiers, uint8_t keycode, bool isPress);

  /**
   * @brief Get last error message
   */
  std::string lastError() const
  {
    return m_lastError;
  }

  /**
   * @brief Check whether the device provided configuration data during handshake
   */
  bool hasDeviceConfig() const
  {
    return m_handshakeComplete && m_hasDeviceConfig;
  }

  /**
   * @brief Retrieve configuration discovered during the CDC handshake
   */
  const PicoConfig &deviceConfig() const
  {
    return m_deviceConfig;
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
  bool m_hasDeviceConfig = false;
  PicoConfig m_deviceConfig;
};

} // namespace deskflow::bridge
