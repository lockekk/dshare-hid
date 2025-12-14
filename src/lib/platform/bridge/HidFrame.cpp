/*
 * Deskflow-hid -- created by locke.huang@gmail.com
 */

#include "HidFrame.h"

namespace deskflow::bridge {

std::vector<uint8_t> HidEventPacket::serialize() const
{
  if (payload.size() > 0xFF) {
    return {};
  }

  std::vector<uint8_t> data;
  data.reserve(4 + payload.size());

  data.push_back(static_cast<uint8_t>(MAGIC & 0xFF));
  data.push_back(static_cast<uint8_t>((MAGIC >> 8) & 0xFF));
  data.push_back(static_cast<uint8_t>(type));
  data.push_back(static_cast<uint8_t>(payload.size()));

  data.insert(data.end(), payload.begin(), payload.end());
  return data;
}

} // namespace deskflow::bridge
