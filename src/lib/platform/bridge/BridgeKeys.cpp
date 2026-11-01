/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "BridgeKeys.h"

#include <QString>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace deskflow::bridge {

namespace {

// the left modifier bits of the HID modifier byte; the right ones are 4 bits higher
constexpr uint8_t kLeftControl = 0x01;
constexpr uint8_t kLeftShift = 0x02;
constexpr uint8_t kLeftAlt = 0x04;
constexpr uint8_t kLeftGui = 0x08;
constexpr uint8_t kRightAlt = 0x40;

constexpr uint8_t bothSides(uint8_t left)
{
  return static_cast<uint8_t>(left | (left << 4));
}

// set 1 scan codes 0x00-0x5F without the extended bit; 0 for the modifiers
constexpr uint8_t kScanCodeKeys[0x60] = {
    // -, Esc, 1-9, 0, -, =, Backspace, Tab
    0x00,
    0x29,
    0x1E,
    0x1F,
    0x20,
    0x21,
    0x22,
    0x23,
    0x24,
    0x25,
    0x26,
    0x27,
    0x2D,
    0x2E,
    0x2A,
    0x2B,
    // q w e r t y u i o p [ ] Enter, Left Control, a s
    0x14,
    0x1A,
    0x08,
    0x15,
    0x17,
    0x1C,
    0x18,
    0x0C,
    0x12,
    0x13,
    0x2F,
    0x30,
    0x28,
    0x00,
    0x04,
    0x16,
    // d f g h j k l ; ' `, Left Shift, \ (# on ISO keyboards), z x c v
    0x07,
    0x09,
    0x0A,
    0x0B,
    0x0D,
    0x0E,
    0x0F,
    0x33,
    0x34,
    0x35,
    0x00,
    0x31,
    0x1D,
    0x1B,
    0x06,
    0x19,
    // b n m , . /, Right Shift, keypad *, Left Alt, Space, Caps Lock, F1-F5
    0x05,
    0x11,
    0x10,
    0x36,
    0x37,
    0x38,
    0x00,
    0x55,
    0x00,
    0x2C,
    0x39,
    0x3A,
    0x3B,
    0x3C,
    0x3D,
    0x3E,
    // F6-F10, Pause, Scroll Lock, keypad 7 8 9 - 4 5 6 + 1
    0x3F,
    0x40,
    0x41,
    0x42,
    0x43,
    0x48,
    0x47,
    0x5F,
    0x60,
    0x61,
    0x56,
    0x5C,
    0x5D,
    0x5E,
    0x57,
    0x59,
    // keypad 2 3 0 ., SysRq, -, the ISO key beside Left Shift (<> on German keyboards), F11, F12,
    // keypad =
    0x5A,
    0x5B,
    0x62,
    0x63,
    0x46,
    0x00,
    0x64,
    0x44,
    0x45,
    0x67,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
};

// a combining accent and the characters a dead key for it may type
struct Accent
{
  char16_t mark;
  char16_t characters[2];
};

constexpr Accent kAccents[] = {
    {0x0300, {u'`', 0}},        // grave
    {0x0301, {0x00B4, u'\''}},  // acute: ´, or ' on US International
    {0x0302, {u'^', 0x02C6}},   // circumflex
    {0x0303, {u'~', 0x02DC}},   // tilde
    {0x0304, {0x00AF, 0}},      // macron
    {0x0306, {0x02D8, 0}},      // breve
    {0x0307, {0x02D9, 0}},      // dot above
    {0x0308, {0x00A8, u'"'}},   // diaeresis: ¨, or " on US International
    {0x030A, {0x00B0, 0x02DA}}, // ring above
    {0x030B, {0x02DD, 0}},      // double acute
    {0x030C, {0x02C7, 0}},      // caron
    {0x0327, {0x00B8, 0}},      // cedilla
    {0x0328, {0x02DB, 0}},      // ogonek
};

} // namespace

KeyboardLayout keyboardLayout(uint8_t value)
{
  return (value == static_cast<uint8_t>(KeyboardLayout::SameAsComputer)) ? KeyboardLayout::SameAsComputer
                                                                         : KeyboardLayout::Us;
}

bool canSendKeysAsPressed()
{
#if defined(_WIN32)
  return true;
#else
  return false;
#endif
}

uint8_t maskHidModifiers(KeyModifierMask mask, uint8_t held)
{
  uint8_t result = 0;
  const auto add = [&result, held](uint8_t left) {
    const uint8_t sides = held & bothSides(left);
    result |= (sides != 0) ? sides : left;
  };
  if (mask & KeyModifierShift) {
    add(kLeftShift);
  }
  if (mask & KeyModifierControl) {
    add(kLeftControl);
  }
  if (mask & KeyModifierAlt) {
    add(kLeftAlt);
  }
  if (mask & (KeyModifierSuper | KeyModifierMeta)) {
    add(kLeftGui);
  }
  if (mask & KeyModifierAltGr) {
    result |= kRightAlt;
  }
  return result;
}

uint8_t heldHidModifiers(KeyModifierMask mask, uint8_t held)
{
  const bool controlAndAlt = (held & bothSides(kLeftControl)) != 0 && (held & bothSides(kLeftAlt)) != 0;
  if (!controlAndAlt || (mask & (KeyModifierControl | KeyModifierAlt)) != 0) {
    return held;
  }
  return static_cast<uint8_t>((held & ~(bothSides(kLeftControl) | bothSides(kLeftAlt))) | kRightAlt);
}

uint8_t windowsScanCodeToHid(uint16_t scanCode)
{
  if (scanCode < sizeof(kScanCodeKeys)) {
    return kScanCodeKeys[scanCode];
  }
  if (scanCode >= 0x64 && scanCode <= 0x6E) {
    return static_cast<uint8_t>(0x68 + (scanCode - 0x64)); // F13-F23
  }

  switch (scanCode) {
  case 0x70: // Katakana/Hiragana
    return 0x88;
  case 0x73: // Ro (JIS \_)
    return 0x87;
  case 0x76: // F24
    return 0x73;
  case 0x79: // Henkan
    return 0x8A;
  case 0x7B: // Muhenkan
    return 0x8B;
  case 0x7D: // Yen
    return 0x89;
  case 0x7E: // keypad . on Brazilian keyboards
    return 0x85;
  case 0x11C: // keypad Enter
    return 0x58;
  case 0x135: // keypad /
    return 0x54;
  case 0x137: // Print Screen
    return 0x46;
  case 0x145: // Num Lock
    return 0x53;
  case 0x146: // Break (Control+Pause)
    return 0x48;
  case 0x147: // Home
    return 0x4A;
  case 0x148: // Up
    return 0x52;
  case 0x149: // Page Up
    return 0x4B;
  case 0x14B: // Left
    return 0x50;
  case 0x14D: // Right
    return 0x4F;
  case 0x14F: // End
    return 0x4D;
  case 0x150: // Down
    return 0x51;
  case 0x151: // Page Down
    return 0x4E;
  case 0x152: // Insert
    return 0x49;
  case 0x153: // Delete
    return 0x4C;
  case 0x15D: // Menu
    return 0x65;
  default:
    return 0;
  }
}

std::vector<char16_t> deadKeyCharacters(KeyID id)
{
  if (id > 0xFFFF) {
    return {};
  }
  const auto character = static_cast<char16_t>(id);

  // a dead key's accent, as it types before a space
  for (const auto &accent : kAccents) {
    for (const char16_t c : accent.characters) {
      if (c != 0 && c == character) {
        return {character};
      }
    }
  }

  // a letter with an accent
  const QString parts = QString(QChar(character)).normalized(QString::NormalizationForm_D);
  if (parts.size() != 2) {
    return {};
  }
  for (const auto &accent : kAccents) {
    if (parts.at(1).unicode() == accent.mark) {
      std::vector<char16_t> characters;
      for (const char16_t c : accent.characters) {
        if (c != 0) {
          characters.push_back(c);
        }
      }
      return characters;
    }
  }
  return {};
}

bool findDeadKey(
    KeyID id, uint16_t scanCode, uint8_t heldModifiers, const DeadKeyLayout &layout, uint16_t &deadScanCode,
    uint8_t &deadModifiers
)
{
  // a character, not a key without one (deskflow's are in the private use area)
  if (id < 0x20 || id > 0xFFFF || (id >= 0xE000 && id <= 0xF8FF)) {
    return false;
  }
  // the key's own character: é on French, AltGr+E on US International, or a dead key pressed twice
  if (layout.typesCharacter(scanCode, heldModifiers, id)) {
    return false;
  }
  for (const char16_t character : deadKeyCharacters(id)) {
    if (layout.deadKeyFor(character, deadScanCode, deadModifiers)) {
      return true;
    }
  }
  return false;
}

#if defined(_WIN32)

namespace {

// the layout of the window typed in, which is the layout the server's hook reads keys with
HKL currentLayout()
{
  if (const HWND window = GetForegroundWindow(); window != nullptr) {
    return GetKeyboardLayout(GetWindowThreadProcessId(window, nullptr));
  }
  return GetKeyboardLayout(0);
}

// a key's scan code as the hook gives it, with 0x100 for an extended key
uint16_t scanCodeOf(UINT virtualKey, HKL layout)
{
  const UINT code = MapVirtualKeyExW(virtualKey, MAPVK_VK_TO_VSC_EX, layout);
  return static_cast<uint16_t>((code & 0xFF) | (((code & 0xFF00) != 0) ? 0x100 : 0));
}

// VkKeyScanEx's shift state: 1 Shift, 2 Control, 4 Alt; Control and Alt are AltGr
uint8_t hidModifiersOf(int shiftState)
{
  uint8_t modifiers = 0;
  if (shiftState & 1) {
    modifiers |= kLeftShift;
  }
  if ((shiftState & 6) == 6) {
    modifiers |= kRightAlt;
  } else if (shiftState & 2) {
    modifiers |= kLeftControl;
  } else if (shiftState & 4) {
    modifiers |= kLeftAlt;
  }
  return modifiers;
}

// what a key types with this keyboard state: the number of characters, negative for a dead key
int typed(UINT virtualKey, const BYTE *keys, WCHAR (&text)[4], HKL layout)
{
  // 0x4 leaves the keyboard state as it is (Windows 10 1607 and later)
  constexpr UINT kKeepState = 0x4;
  const int result =
      ToUnicodeEx(virtualKey, MapVirtualKeyExW(virtualKey, MAPVK_VK_TO_VSC, layout), keys, text, 4, kKeepState, layout);
  if (result < 0) {
    // a Windows without that flag keeps the dead key: a space types it out
    const BYTE none[256] = {};
    WCHAR spare[4] = {};
    ToUnicodeEx(VK_SPACE, MapVirtualKeyExW(VK_SPACE, MAPVK_VK_TO_VSC, layout), none, spare, 4, kKeepState, layout);
  }
  return result;
}

bool isDeadKey(UINT virtualKey, int shiftState, HKL layout)
{
  BYTE keys[256] = {};
  if (shiftState & 1) {
    keys[VK_SHIFT] = 0x80;
  }
  if (shiftState & 2) {
    keys[VK_CONTROL] = 0x80;
  }
  if (shiftState & 4) {
    keys[VK_MENU] = 0x80;
  }
  WCHAR text[4] = {};
  return typed(virtualKey, keys, text, layout) < 0;
}

bool typesCharacter(uint16_t scanCode, uint8_t heldModifiers, KeyID id, HKL layout)
{
  // 0xE0 before an extended key's scan code
  const UINT virtualKey =
      MapVirtualKeyExW((scanCode & 0xFF) | (((scanCode & 0x100) != 0) ? 0xE000 : 0), MAPVK_VSC_TO_VK_EX, layout);
  if (virtualKey == 0) {
    return false;
  }

  BYTE keys[256] = {};
  const auto hold = [&keys, heldModifiers](uint8_t left, int either, int leftKey, int rightKey) {
    if ((heldModifiers & left) != 0) {
      keys[either] = keys[leftKey] = 0x80;
    }
    if ((heldModifiers & (left << 4)) != 0) {
      keys[either] = keys[rightKey] = 0x80;
    }
  };
  hold(kLeftShift, VK_SHIFT, VK_LSHIFT, VK_RSHIFT);
  hold(kLeftControl, VK_CONTROL, VK_LCONTROL, VK_RCONTROL);
  hold(kLeftAlt, VK_MENU, VK_LMENU, VK_RMENU);

  // Caps Lock's state isn't known here
  for (const BYTE capsLock : {BYTE(0), BYTE(1)}) {
    keys[VK_CAPITAL] = capsLock;
    WCHAR text[4] = {};
    const int count = typed(virtualKey, keys, text, layout);
    // a dead key types its accent, as when it's pressed twice
    if ((count == 1 || count == -1) && text[0] == id) {
      return true;
    }
  }
  return false;
}

bool deadKeyFor(char16_t character, uint16_t &scanCode, uint8_t &modifiers, HKL layout)
{
  const SHORT key = VkKeyScanExW(static_cast<WCHAR>(character), layout);
  if (key == -1) {
    return false;
  }
  const UINT virtualKey = LOBYTE(key);
  const int shiftState = HIBYTE(key);
  if (!isDeadKey(virtualKey, shiftState, layout)) {
    return false;
  }
  scanCode = scanCodeOf(virtualKey, layout);
  modifiers = hidModifiersOf(shiftState);
  return true;
}

} // namespace

bool windowsDeadKey(KeyID id, uint16_t scanCode, uint8_t heldModifiers, uint16_t &deadScanCode, uint8_t &deadModifiers)
{
  const HKL layout = currentLayout();
  const DeadKeyLayout windows{
      [layout](uint16_t key, uint8_t held, KeyID character) { return typesCharacter(key, held, character, layout); },
      [layout](char16_t character, uint16_t &key, uint8_t &modifiers) {
        return deadKeyFor(character, key, modifiers, layout);
      }
  };
  return findDeadKey(id, scanCode, heldModifiers, windows, deadScanCode, deadModifiers);
}

#endif

} // namespace deskflow::bridge
