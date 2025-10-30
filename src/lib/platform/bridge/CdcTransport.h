/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "HidFrame.h"

#include <QString>
#include <memory>
#include <string>

namespace deskflow::bridge {

/**
 * @brief Pico configuration structure
 */
struct PicoConfig {
  std::string arch;           // e.g., "bridge-ios", "bridge-android"
  int32_t screenWidth = 0;
  int32_t screenHeight = 0;
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
   * @brief Query Pico for configuration (arch + screen info)
   * @return PicoConfig if successful
   */
  PicoConfig queryConfig();

  /**
   * @brief Send HID frame to Pico
   * @return true if successful
   */
  bool sendHidFrame(const HidFrame &frame);

  /**
   * @brief Get last error message
   */
  std::string lastError() const
  {
    return m_lastError;
  }

private:
  /**
   * @brief Send a command and wait for response
   */
  bool sendCommand(const std::string &command, std::string &response);

  /**
   * @brief Read response from device with timeout
   */
  bool readResponse(std::string &response, int timeoutMs = 1000);

  QString m_devicePath;
  int m_fd = -1; // File descriptor (Unix) or HANDLE (Windows)
  std::string m_lastError;
};

} // namespace deskflow::bridge
