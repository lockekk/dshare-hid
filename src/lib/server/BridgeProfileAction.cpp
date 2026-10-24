/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#include "server/BridgeProfileAction.h"

#include "base/Log.h"
#include "base/String.h"
#include "deskflow/ipc/CoreIpc.h"

#include <charconv>

BridgeProfileAction::BridgeProfileAction(int position) : m_position(position)
{
  // do nothing
}

bool BridgeProfileAction::parsePosition(const std::string &argument, int &position)
{
  const char *end = argument.data() + argument.size();
  const auto [last, error] = std::from_chars(argument.data(), end, position);
  return error == std::errc() && last == end && position >= kMinPosition && position <= kMaxPosition;
}

int BridgeProfileAction::getPosition() const
{
  return m_position;
}

InputFilter::Action *BridgeProfileAction::clone() const
{
  return new BridgeProfileAction(*this);
}

std::string BridgeProfileAction::format() const
{
  return deskflow::string::sprintf("%s(%d)", kName, m_position);
}

void BridgeProfileAction::perform(const Event &)
{
  LOG_INFO("bridge profile hotkey: switching to profile %d", m_position);
  ipcSendToClient(QString::fromLatin1(kName), QString::number(m_position));
}
