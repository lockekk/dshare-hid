/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "HidFrame.h"

#include <cstring>

namespace deskflow::bridge {

std::vector<uint8_t> HidFrame::serialize() const
{
  if (payload.size() > MAX_PAYLOAD_SIZE) {
    return {};
  }

  std::vector<uint8_t> data;
  data.reserve(4 + payload.size());

  data.push_back(MAGIC_BYTE);
  data.push_back(static_cast<uint8_t>(type));
  data.push_back(static_cast<uint8_t>(payload.size()));

  for (uint8_t byte : payload) {
    data.push_back(byte);
  }

  uint8_t checksum = calculateChecksum(data.data(), data.size());
  data.push_back(checksum);

  return data;
}

bool HidFrame::deserialize(const std::vector<uint8_t> &data, HidFrame &frame)
{
  if (data.size() < 4) {
    return false; // Minimum frame size: magic + type + length + checksum
  }

  if (data[0] != MAGIC_BYTE) {
    return false;
  }

  size_t payloadLength = data[2];
  size_t expectedSize = 4 + payloadLength; // magic + type + length + payload + checksum

  if (data.size() != expectedSize) {
    return false;
  }

  // Verify checksum (last byte)
  uint8_t expectedChecksum = calculateChecksum(data.data(), data.size() - 1);
  if (data.back() != expectedChecksum) {
    return false;
  }

  frame.type = static_cast<HidReportType>(data[1]);
  frame.payload.clear();
  frame.payload.insert(frame.payload.end(), data.begin() + 3, data.begin() + 3 + payloadLength);

  return true;
}

uint8_t HidFrame::calculateChecksum(const uint8_t *data, size_t length)
{
  uint8_t checksum = 0;
  for (size_t i = 0; i < length; i++) {
    checksum ^= data[i];
  }
  return checksum;
}

// KeyboardReport implementation
std::vector<uint8_t> KeyboardReport::toPayload() const
{
  std::vector<uint8_t> payload(8);
  payload[0] = modifiers;
  payload[1] = reserved;
  std::memcpy(&payload[2], keycodes, 6);
  return payload;
}

KeyboardReport KeyboardReport::fromPayload(const std::vector<uint8_t> &payload)
{
  KeyboardReport report = {};
  if (payload.size() >= 8) {
    report.modifiers = payload[0];
    report.reserved = payload[1];
    std::memcpy(report.keycodes, &payload[2], 6);
  }
  return report;
}

// MouseReport implementation
std::vector<uint8_t> MouseReport::toPayload() const
{
  return {buttons, static_cast<uint8_t>(x), static_cast<uint8_t>(y)};
}

MouseReport MouseReport::fromPayload(const std::vector<uint8_t> &payload)
{
  MouseReport report = {};
  if (payload.size() >= 3) {
    report.buttons = payload[0];
    report.x = static_cast<int8_t>(payload[1]);
    report.y = static_cast<int8_t>(payload[2]);
  }
  return report;
}

// MouseWheelReport implementation
std::vector<uint8_t> MouseWheelReport::toPayload() const
{
  return {static_cast<uint8_t>(wheel), static_cast<uint8_t>(hwheel)};
}

MouseWheelReport MouseWheelReport::fromPayload(const std::vector<uint8_t> &payload)
{
  MouseWheelReport report = {};
  if (payload.size() >= 2) {
    report.wheel = static_cast<int8_t>(payload[0]);
    report.hwheel = static_cast<int8_t>(payload[1]);
  }
  return report;
}

} // namespace deskflow::bridge
