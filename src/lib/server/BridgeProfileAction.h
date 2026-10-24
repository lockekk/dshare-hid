/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#pragma once

#include "server/InputFilter.h"

#include <string>

/**
 * @brief Hotkey action that switches a bridge client's device to a profile:
 * switchBridgeProfile(<position>), the position counting the device's profiles from 1
 *
 * The bridge clients' devices belong to the GUI (through their bridge cores), not to the
 * server, so the action only passes the position on to the GUI over the core IPC, which picks
 * the bridge client.  The server checks its hotkeys whichever computer the cursor is on, so
 * the switch works from any of them.  The GUI writes these hotkeys into the server
 * configuration itself (gui/core/BridgeProfileShortcuts).
 */
class BridgeProfileAction : public InputFilter::Action
{
public:
  static constexpr auto kName = "switchBridgeProfile";
  static constexpr int kMinPosition = 1;
  static constexpr int kMaxPosition = 6; // the most profiles a device has

  explicit BridgeProfileAction(int position);

  /// a position in range, as the action's argument
  static bool parsePosition(const std::string &argument, int &position);

  int getPosition() const;

  // Action overrides
  Action *clone() const override;
  std::string format() const override;
  void perform(const Event &) override;

private:
  int m_position;
};
