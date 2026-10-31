/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#pragma once

#include "BridgeKeys.h"
#include "HidFrame.h"

#include <cstdint>
#include <functional>
#include <map>
#include <set>

namespace deskflow::bridge {

/**
 * @brief A bridge client's keyboard: the server's key events as the HID reports the dongle sends
 *
 * It keeps the keys that are down, to send each report with the modifier keys held. The
 * reports go to the functions it is given, which send them to the dongle, or keep them for a
 * test to check.
 */
class BridgeKeyboard
{
public:
  /// whose key codes the server's key buttons are: the computer it runs on
  enum class KeyCodes
  {
    MacOS,   // the virtual key code + 1
    Windows, // the scan code, 0x100 for an extended key
    Linux    // the X key code
  };

#if defined(__APPLE__)
  static constexpr KeyCodes kThisComputer = KeyCodes::MacOS;
#elif defined(_WIN32)
  static constexpr KeyCodes kThisComputer = KeyCodes::Windows;
#else
  static constexpr KeyCodes kThisComputer = KeyCodes::Linux;
#endif

  using KeySink = std::function<bool(HidEventType type, uint8_t modifiers, uint8_t key)>;
  using ConsumerSink = std::function<bool(HidEventType type, uint16_t usage)>;
  /// the dead key the server's Windows hook held back before a character (windowsDeadKey)
  using DeadKeyFinder =
      std::function<bool(KeyID id, uint16_t scanCode, uint16_t &deadScanCode, uint8_t &deadModifiers)>;

  BridgeKeyboard(KeyCodes keyCodes, KeySink keys, ConsumerSink consumer, DeadKeyFinder deadKeys = nullptr);

  /// the layout the device is set to, from its profile
  void setLayout(KeyboardLayout layout);
  /// the device is set to this computer's layout, and the key codes have a table (Windows')
  bool sendsKeysAsPressed() const;

  void keyDown(KeyID id, KeyModifierMask mask, KeyButton button);
  bool keyRepeat(KeyID id, KeyModifierMask mask, int32_t count, KeyButton button);
  void keyUp(KeyButton button);
  void allKeysUp();

  bool isKeyDown(KeyButton button) const;
  KeyModifierMask activeModifiers() const;
  const std::set<KeyButton> &pressedButtons() const;

private:
  struct ActiveKey
  {
    uint8_t hidKey = 0;
    uint8_t modifierBit = 0;
  };

  // the modifier bit of a modifier key, by its key ID or else its button; 0 for another key
  uint8_t modifierBit(KeyID id, KeyButton button) const;
  // the HID modifier byte for a key, given the modifier bits of the modifier keys held
  uint8_t keyModifiers(KeyModifierMask mask, uint8_t held) const;
  // the HID usage of a key: by its character, or as it is pressed
  uint8_t hidKey(KeyID id, KeyButton button) const;
  uint8_t buttonToHid(KeyButton button) const;
  uint8_t heldModifierBits() const;
  // before a character the Windows hook composed: the dead key the device needs to compose it
  void sendDeadKey(KeyID id, KeyButton button) const;

  KeyCodes m_keyCodes;
  KeySink m_keys;
  ConsumerSink m_consumer;
  DeadKeyFinder m_deadKeys;
  KeyboardLayout m_layout = KeyboardLayout::Us;

  std::set<KeyButton> m_pressedButtons;
  std::map<KeyButton, uint16_t> m_pressedConsumerControls; // button -> consumer code
  std::map<KeyButton, ActiveKey> m_activeKeys;
  uint8_t m_currentHidModifiers = 0;
  KeyModifierMask m_activeModifiers = 0;
};

} // namespace deskflow::bridge
