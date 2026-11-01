/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#pragma once

#include "deskflow/KeyTypes.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace deskflow::bridge {

/**
 * @brief The keyboard layout a profile's device is set to (DeviceProfile::keyboardLayout)
 */
enum class KeyboardLayout : uint8_t
{
  Us = 0,            // each character goes as a US keyboard types it (the default)
  SameAsComputer = 1 // each key goes as it is pressed, for the device to read with its own layout
};

/// a profile's layout; an unknown value is US
KeyboardLayout keyboardLayout(uint8_t value);

/// whether this computer's keys can go as they are pressed: Windows only, for now
bool canSendKeysAsPressed();

/**
 * @brief The HID modifier byte for a key that the mask says is typed with these modifiers
 *
 * Each modifier is on the side of the key held for it, or the left without one: a device's
 * layout reads Right Alt as AltGr.
 * @param held the modifier bits of the modifier keys held
 */
uint8_t maskHidModifiers(KeyModifierMask mask, uint8_t held);

/**
 * @brief The HID modifier byte of the modifier keys held, for a key that goes as it is pressed
 *
 * When Control and Alt are held but the mask has neither, the server's layout typed the
 * character with them as AltGr (Windows presses Left Control with AltGr), and the device gets
 * Right Alt, its AltGr.
 */
uint8_t heldHidModifiers(KeyModifierMask mask, uint8_t held);

/// the HID usage of the key with a Windows scan code (0x100 for an extended key); 0 for a
/// modifier or an unknown key
uint8_t windowsScanCodeToHid(uint16_t scanCode);

/// the characters a layout's dead key may type for the accent of a composed character, e.g. '^'
/// for 'ê', or the accent itself; none for any other character
std::vector<char16_t> deadKeyCharacters(KeyID id);

/**
 * @brief What findDeadKey asks of the server's keyboard layout
 */
struct DeadKeyLayout
{
  /// whether the key with this scan code types the character by itself, with the modifier keys
  /// held (HID modifier bits), and Caps Lock on or off
  std::function<bool(uint16_t scanCode, uint8_t heldModifiers, KeyID id)> typesCharacter;
  /// the dead key that types this accent, and the HID modifiers it is typed with
  std::function<bool(char16_t character, uint16_t &scanCode, uint8_t &modifiers)> deadKeyFor;
};

/**
 * @brief The dead key that the key with this scan code completed a character with
 *
 * The server's Windows hook holds a dead key back and sends only the character it composes,
 * on the next key, so a device that composes it again needs the dead key first.
 * @param heldModifiers the HID modifier bits of the modifier keys held
 * @param deadModifiers the HID modifiers that the dead key is typed with
 * @return false when the key typed the character by itself
 */
bool findDeadKey(
    KeyID id, uint16_t scanCode, uint8_t heldModifiers, const DeadKeyLayout &layout, uint16_t &deadScanCode,
    uint8_t &deadModifiers
);

#if defined(_WIN32)
/// findDeadKey, with the layout of the window typed in
bool windowsDeadKey(KeyID id, uint16_t scanCode, uint8_t heldModifiers, uint16_t &deadScanCode, uint8_t &deadModifiers);
#endif

} // namespace deskflow::bridge
