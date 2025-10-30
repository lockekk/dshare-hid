/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstdint>
#include <vector>

namespace deskflow::bridge {

/**
 * @brief HID report types for bridge communication
 */
enum class HidReportType : uint8_t {
  Keyboard = 0x01,
  Mouse = 0x02,
  MouseWheel = 0x03,
  ConsumerControl = 0x04
};

/**
 * @brief HID frame structure for USB CDC transport
 *
 * Frame format:
 * - Magic byte (0xAA)
 * - Report type (1 byte)
 * - Payload length (1 byte)
 * - Payload data (variable)
 * - Checksum (1 byte, simple XOR of all preceding bytes)
 */
struct HidFrame {
  static constexpr uint8_t MAGIC_BYTE = 0xAA;
  static constexpr size_t MAX_PAYLOAD_SIZE = 64;

  HidReportType type;
  std::vector<uint8_t> payload;

  /**
   * @brief Serialize frame to byte vector for transmission
   */
  std::vector<uint8_t> serialize() const;

  /**
   * @brief Deserialize frame from byte vector
   * @return true if frame is valid, false otherwise
   */
  static bool deserialize(const std::vector<uint8_t> &data, HidFrame &frame);

  /**
   * @brief Calculate checksum for frame data
   */
  static uint8_t calculateChecksum(const uint8_t *data, size_t length);
};

/**
 * @brief Keyboard HID report structure (Boot Protocol)
 */
struct KeyboardReport {
  uint8_t modifiers;  // Modifier keys bitmap
  uint8_t reserved;   // Reserved byte (usually 0)
  uint8_t keycodes[6]; // Up to 6 simultaneous key presses

  std::vector<uint8_t> toPayload() const;
  static KeyboardReport fromPayload(const std::vector<uint8_t> &payload);
};

/**
 * @brief Mouse HID report structure (Boot Protocol)
 */
struct MouseReport {
  uint8_t buttons;    // Button state bitmap
  int8_t x;           // X movement delta
  int8_t y;           // Y movement delta

  std::vector<uint8_t> toPayload() const;
  static MouseReport fromPayload(const std::vector<uint8_t> &payload);
};

/**
 * @brief Mouse wheel HID report structure
 */
struct MouseWheelReport {
  int8_t wheel;       // Vertical wheel delta
  int8_t hwheel;      // Horizontal wheel delta (optional)

  std::vector<uint8_t> toPayload() const;
  static MouseWheelReport fromPayload(const std::vector<uint8_t> &payload);
};

} // namespace deskflow::bridge
