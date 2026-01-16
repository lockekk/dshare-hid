/*
 * Deskflow-hid -- created by locke.huang@gmail.com
 */

#pragma once

#include "HidFrame.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <QString>

namespace deskflow::bridge {

enum class ActivationState : uint8_t
{
  FreeTrial = 0,
  Inactive = 1,
  Bricked = 2,
  Activated = 3,
  Unknown = 0xFF
};

enum class FirmwareMode : uint8_t
{
  Factory = 0,
  App = 1,
  Unknown = 0xFF
};

inline const char *activationStateToString(ActivationState state)
{
  switch (state) {
  case ActivationState::Activated:
    return "Activated";
  case ActivationState::FreeTrial:
    return "Free Trial";
  case ActivationState::Inactive:
    return "Unlicensed";
  default:
    return "Unknown";
  }
}

/**
 * @brief Device Profile structure (matches firmware layout)
 */
#pragma pack(push, 1)
struct DeviceProfile
{
  char hostname[32];     // Null-terminated string
  uint8_t slot;          // Slot Index (0-14)
  uint8_t hidMode;       // 0: Combo, 1: Mouse
  uint16_t screenWidth;  // Pixels (default 1080)
  uint16_t screenHeight; // Pixels (default 2424)
  uint8_t rotation;      // 0: Portrait, 1: Landscape
  uint8_t invert;        // scroll direction 0: no invert, 1: invert
  uint8_t speed;         // scroll speed  0: 120; default is 120
  uint8_t reserved[11];  // reserved for future use, total size is 52 bytes
};
#pragma pack(pop)

/**
 * @brief Firmware-reported configuration structure
 */
struct FirmwareConfig
{
  uint8_t protocolVersion = 0;
  ActivationState activationState = ActivationState::Unknown;
  uint8_t firmwareVersionBcd = 0;
  uint8_t hardwareVersionBcd = 0;
  FirmwareMode firmwareMode = FirmwareMode::Unknown;
  uint8_t activeProfile = 0;
  uint8_t totalProfiles = 0;
  bool isBleConnected = false;
  std::string deviceName;
  bool hasOtaPartition = false;

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
   * @param allowInsecure If true, perform an insecure handshake (no HMAC).
   * @return true if successful
   */
  bool open(bool allowInsecure = false);

  /**
   * @brief Close the CDC device
   */
  void close();

  /**
   * @brief Check if device is open
   */
  bool isOpen() const;

  /**
   * @brief Check if connection is authenticated/secure
   */
  bool isSecure() const
  {
    return m_isSecure;
  }

  /**
   * @brief Send HID event packet to firmware device
   * @return true if successful
   */
  bool sendHidEvent(const HidEventPacket &packet);

  bool fetchDeviceName(std::string &outName);
  bool setDeviceName(const std::string &name);

  /**
   * @brief Read serial number from firmware via CDC command
   * @param outSerial Output string to store serial number
   * @return true if successful, false if failed
   */
  bool fetchSerialNumber(std::string &outSerial);

  /**
   * @brief Activate the device with a license code
   * @param licenseCode Base64 license string
   * @return true if successful
   */
  bool activateDevice(const std::string &licenseCode);

  /**
   * @brief Access the device to switch to factory partition
   * @return true if successful
   */
  bool gotoFactory();

  /**
   * @brief Unpair all bonded devices
   * @return true if successful
   */
  bool unpairAll();

  /**
   * @brief Get a profile from the device
   * @param index Profile index
   * @param outProfile Output profile data
   * @return true if successful
   */
  bool getProfile(uint8_t index, DeviceProfile &outProfile);

  /**
   * @brief Set a profile on the device
   * @param index Profile index
   * @param profile Profile data to set
   * @return true if successful
   */
  bool setProfile(uint8_t index, const DeviceProfile &profile);

  /**
   * @brief Switch to a specific profile
   * @param index Profile index
   * @return true if successful
   */
  bool switchProfile(uint8_t index);

  /**
   * @brief Erase a specific profile
   * @param index Profile index
   * @return true if successful
   */
  bool eraseProfile(uint8_t index);

  /**
   * @brief Erase all profiles
   * @return true if successful
   */
  bool eraseAllProfiles();

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
   * @brief Send a keep-alive CDC command to the firmware
   * @param uptimeSeconds Output parameter for firmware uptime in seconds
   * @return true if successful
   */
  bool sendKeepAlive(uint32_t &uptimeSeconds);

private:
  static constexpr size_t kPublicKeySize = 64; // X + Y (32 bytes each)
  static constexpr size_t kSignatureSize = 64; // R + S (32 bytes each)
  static constexpr size_t kAuthNonceSize = 32;

  bool performHandshake(bool allowInsecure);
  bool sendUsbFrame(uint8_t type, uint8_t flags, const uint8_t *payload, uint16_t length);
  bool sendUsbFrame(uint8_t type, uint8_t flags, const std::vector<uint8_t> &payload);
  bool waitForConfigResponse(uint8_t &msgType, uint8_t &status, std::vector<uint8_t> &payload, int timeoutMs);
  bool waitForControlMessage(uint8_t controlId, std::vector<uint8_t> &payload, int timeoutMs);
  bool writeAll(const uint8_t *data, size_t length);
  bool readFrame(uint8_t &type, uint8_t &flags, std::vector<uint8_t> &payload, int timeoutMs);
  void resetState();
  bool ensureOpen(bool allowInsecure = false);

  QString m_devicePath;
  intptr_t m_fd = -1; // File descriptor (Unix) or HANDLE (Windows)
  bool m_handshakeComplete = false;
  bool m_isSecure = false;
  std::array<uint8_t, kAuthNonceSize> m_hostNonce{};
  bool m_hasHostNonce = false;
  std::vector<uint8_t> m_rxBuffer;
  std::string m_lastError;
  bool m_hasDeviceConfig = false;
  FirmwareConfig m_deviceConfig;
};

} // namespace deskflow::bridge
