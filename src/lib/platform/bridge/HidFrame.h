/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstdint>
#include <vector>

namespace deskflow::bridge {

enum class HidEventType : uint8_t {
  KeyboardPress = 0x01,
  KeyboardRelease = 0x02,
  MouseMove = 0x03,
  MouseButtonPress = 0x04,
  MouseButtonRelease = 0x05,
  MouseScroll = 0x06,
};

/**
 * @brief HID protocol packet wrapper
 *
 * Format (little endian):
 *   header (0xAA55) | type (1 byte) | length (1 byte) | payload (N bytes)
 */
struct HidEventPacket {
  static constexpr uint16_t MAGIC = 0xAA55;

  HidEventType type;
  std::vector<uint8_t> payload;

  std::vector<uint8_t> serialize() const;
};

} // namespace deskflow::bridge
