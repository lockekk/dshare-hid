/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "HidFrame.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <QString>

namespace deskflow::bridge {

enum class ActivationState : uint8_t {
  FreeTrial = 0,
  Inactive = 1,
  Bricked = 2,
  Activated = 3,
  Unknown = 0xFF
};

inline const char *activationStateToString(ActivationState state)
{
  switch (state) {
  case ActivationState::Activated:
    return "activated";
  case ActivationState::FreeTrial:
    return "free trial";
  case ActivationState::Inactive:
    return "unlicensed";
  default:
    return "unknown";
  }
}

/**
 * @brief Firmware-reported configuration structure
 */
struct FirmwareConfig {
  uint8_t protocolVersion = 0;
  ActivationState activationState = ActivationState::Unknown;
  uint8_t firmwareVersionBcd = 0;
  uint8_t hardwareVersionBcd = 0;
  std::string deviceName;

  const char *activationStateString() const
  {
    return activationStateToString(activationState);
  }
};

/**
 * @brief USB CDC transport helper for bridge firmware communication
 *
 * Handles:
 * - Opening/closing CDC device
 * - Configuration exchange with firmware
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
   * @brief Send HID event packet to firmware device
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

  bool fetchDeviceName(std::string &outName);
  bool setDeviceName(const std::string &name);


  /**
   * @brief Read serial number from firmware via CDC command
   * @param outSerial Output string to store serial number
   * @return true if successful, false if failed
   */
  bool fetchSerialNumber(std::string &outSerial);

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
  const FirmwareConfig &deviceConfig() const
  {
    return m_deviceConfig;
  }

  /**
   * @brief Get the device path
   */
  QString devicePath() const
  {
    return m_devicePath;
  }

  /**
   * @brief Override the authentication key using a 64-character hex string.
   * @param hex Hexadecimal key or empty string to use the default debug key.
   * @return true if the key was applied successfully.
   */
  bool setAuthKeyHex(const QString &hex);

  /**
   * @brief Validate whether a string is a valid auth key (64 hex characters or empty).
   */
  static bool isValidAuthKeyHex(const QString &hex);

private:
  static constexpr size_t kAuthKeySize = 32;
  static constexpr size_t kAuthNonceSize = 8;

  bool performHandshake();
  bool sendUsbFrame(uint8_t type, uint8_t flags, const uint8_t *payload, uint16_t length);
  bool sendUsbFrame(uint8_t type, uint8_t flags, const std::vector<uint8_t> &payload);
  bool waitForConfigResponse(uint8_t &msgType, uint8_t &status, std::vector<uint8_t> &payload, int timeoutMs);
  bool waitForControlMessage(uint8_t controlId, std::vector<uint8_t> &payload, int timeoutMs);
  bool writeAll(const uint8_t *data, size_t length);
  bool readFrame(uint8_t &type, uint8_t &flags, std::vector<uint8_t> &payload, int timeoutMs);
  void resetState();
  bool ensureOpen();

  QString m_devicePath;
  intptr_t m_fd = -1; // File descriptor (Unix) or HANDLE (Windows)
  bool m_handshakeComplete = false;
  std::array<uint8_t, kAuthNonceSize> m_hostNonce{};
  bool m_hasHostNonce = false;
  std::vector<uint8_t> m_rxBuffer;
  std::string m_lastError;
  bool m_hasDeviceConfig = false;
  FirmwareConfig m_deviceConfig;
  std::array<uint8_t, kAuthKeySize> m_authKey{};
};

} // namespace deskflow::bridge
