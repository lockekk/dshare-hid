/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "BridgeKeyboard.h"

#include "base/Log.h"

#include <utility>

namespace deskflow::bridge {

namespace {

// the modifier keys' scan codes, 0x100 for an extended key
uint8_t windowsModifierBit(KeyButton button)
{
  switch (button) {
  case 0x2A: // Left Shift
    return 0x02;
  case 0x36: // Right Shift
    return 0x20;
  case 0x1D: // Left Control
    return 0x01;
  case 0x11D: // Right Control
    return 0x10;
  case 0x38: // Left Alt
    return 0x04;
  case 0x138: // Right Alt
    return 0x40;
  case 0x15B: // Left Windows
    return 0x08;
  case 0x15C: // Right Windows
    return 0x80;
  default:
    return 0;
  }
}

uint8_t modifierBitForKey(KeyID id)
{
  switch (id) {
  case ::kKeyShift_L:
  case 0xFFE1:
    return 0x02;
  case ::kKeyShift_R:
  case 0xFFE2:
    return 0x20;
  case ::kKeyControl_L:
  case 0xFFE3:
    return 0x01;
  case ::kKeyControl_R:
  case 0xFFE4:
    return 0x10;
  case ::kKeyAlt_L:
  case 0xFFE9:
    return 0x04;
  case ::kKeyAlt_R:
  case 0xFFEA:
  case ::kKeyAltGr:
    return 0x40;
  case ::kKeyMeta_L:
  case ::kKeySuper_L:
  case 0xFFE7:
  case 0xFFEB:
    return 0x08;
  case ::kKeyMeta_R:
  case ::kKeySuper_R:
  case 0xFFE8:
  case 0xFFEC:
    return 0x80;
  default:
    return 0;
  }
}

uint8_t macModifierBit(KeyButton button)
{
  switch (button) {
  case 57: // macOS Shift_L (56 + 1)
    return 0x02;
  case 61: // macOS Shift_R (60 + 1)
    return 0x20;
  case 60: // macOS Control_L (59 + 1)
    return 0x01;
  case 63: // macOS Control_R (62 + 1)
    return 0x10;
  case 59: // macOS Alt_L (58 + 1)
    return 0x04;
  case 62: // macOS Alt_R (61 + 1)
    return 0x40;
  case 56: // macOS Command (55 + 1)
    return 0x08;
  default:
    return 0;
  }
}

uint8_t linuxModifierBit(KeyButton button)
{
  switch (button) {
  case 50: // Linux Shift_L
    return 0x02;
  case 62: // Linux Shift_R
    return 0x20;
  case 37: // Linux Control_L
    return 0x01;
  case 105: // Linux Control_R
    return 0x10;
  case 64: // Linux Alt_L
    return 0x04;
  case 108: // Linux Alt_R
    return 0x40;
  case 133: // Linux Super_L
    return 0x08;
  case 134: // Linux Super_R
    return 0x80;
  default:
    return 0;
  }
}

uint16_t convertMediaKeyToConsumerControl(KeyID id)
{
  switch (id) {
  case 0xE0AD: // kKeyAudioMute
    return 0x00E2;
  case 0xE0AE: // kKeyAudioDown
    return 0x00EA;
  case 0xE0AF: // kKeyAudioUp
    return 0x00E9;
  case 0xE0B0: // kKeyAudioNext
    return 0x00B5;
  case 0xE0B1: // kKeyAudioPrev
    return 0x00B6;
  case 0xE0B2: // kKeyAudioStop
    return 0x00B7;
  case 0xE0B3: // kKeyAudioPlay
    return 0x00CD;
  case 0xE0B8: // kKeyBrightnessDown
    return 0x0070;
  case 0xE0B9: // kKeyBrightnessUp
    return 0x006F;
  case 0xE0A6: // kKeyWWWBack
    return 0x0224;
  case 0xE0A7: // kKeyWWWForward
    return 0x0225;
  case 0xE0A8: // kKeyWWWRefresh
    return 0x0227;
  case 0xE0A9: // kKeyWWWStop
    return 0x0226;
  case 0xE0AA: // kKeyWWWSearch
    return 0x0221;
  case 0xE0AB: // kKeyWWWFavorites
    return 0x022A;
  case 0xE0AC: // kKeyWWWHome
    return 0x0223;
  case 0xE0B4: // kKeyAppMail
    return 0x018A;
  case 0xE0B5: // kKeyAppMedia
    return 0x0183;
  default:
    return 0;
  }
}

uint8_t convertKeyID(KeyID id)
{
  if (id >= 'a' && id <= 'z')
    return static_cast<uint8_t>(0x04 + (id - 'a'));
  if (id >= 'A' && id <= 'Z')
    return static_cast<uint8_t>(0x04 + (id - 'A'));
  if (id >= '1' && id <= '9')
    return static_cast<uint8_t>(0x1E + (id - '1'));
  if (id == '0')
    return 0x27;

  switch (id) {
  case '-':
  case '_':
    return 0x2D;
  case '=':
  case '+':
    return 0x2E;
  case '[':
  case '{':
    return 0x2F;
  case ']':
  case '}':
    return 0x30;
  case '\\':
  case '|':
    return 0x31;
  case ';':
  case ':':
    return 0x33;
  case '\'':
  case '"':
    return 0x34;
  case '`':
  case '~':
    return 0x35;
  case ',':
  case '<':
    return 0x36;
  case '.':
  case '>':
    return 0x37;
  case '/':
  case '?':
    return 0x38;
  case '!':
    return 0x1E;
  case '@':
    return 0x1F;
  case '#':
    return 0x20;
  case '$':
    return 0x21;
  case '%':
    return 0x22;
  case '^':
    return 0x23;
  case '&':
    return 0x24;
  case '*':
    return 0x25;
  case '(':
    return 0x26;
  case ')':
    return 0x27;
  default:
    break;
  }

  switch (id) {
  case 0xFFE5:
  case 0xEFE5:
    return 0x39;
  case 0xFFBE:
  case 0xEFBE:
    return 0x3A;
  case 0xFFBF:
  case 0xEFBF:
    return 0x3B;
  case 0xFFC0:
  case 0xEFC0:
    return 0x3C;
  case 0xFFC1:
  case 0xEFC1:
    return 0x3D;
  case 0xFFC2:
  case 0xEFC2:
    return 0x3E;
  case 0xFFC3:
  case 0xEFC3:
    return 0x3F;
  case 0xFFC4:
  case 0xEFC4:
    return 0x40;
  case 0xFFC5:
  case 0xEFC5:
    return 0x41;
  case 0xFFC6:
  case 0xEFC6:
    return 0x42;
  case 0xFFC7:
  case 0xEFC7:
    return 0x43;
  case 0xFFC8:
  case 0xEFC8:
    return 0x44;
  case 0xFFC9:
  case 0xEFC9:
    return 0x45;
  case 0xFF61:
  case 0xEF61:
    return 0x46;
  case 0xFF14:
  case 0xEF14:
    return 0x47;
  case 0xFF13:
  case 0xEF13:
    return 0x48;
  case 0xFF63:
  case 0xEF63:
    return 0x49;
  case 0xFF0D:
  case 0xEF0D:
    return 0x28;
  case 0xFF1B:
  case 0xEF1B:
    return 0x29;
  case 0xFF08:
  case 0xEF08:
    return 0x2A;
  case 0xFF09:
  case 0xEF09:
    return 0x2B;
  case 0x0020:
    return 0x2C;
  case 0xFF50:
  case 0xEF50:
    return 0x4A;
  case 0xFF57:
  case 0xEF57:
    return 0x4D;
  case 0xFF55:
  case 0xEF55:
    return 0x4B;
  case 0xFF56:
  case 0xEF56:
    return 0x4E;
  case 0xFFFF:
  case 0xEFFF:
    return 0x4C;
  case 0xFF7F:
  case 0xEF7F:
    return 0x53;
  case 0xFF67:
  case 0xEF67:
    return 0x65;
  case 0xFF51:
  case 0xEF51:
    return 0x50;
  case 0xFF52:
  case 0xEF52:
    return 0x52;
  case 0xFF53:
  case 0xEF53:
    return 0x4F;
  case 0xFF54:
  case 0xEF54:
    return 0x51;
  default:
    LOG_VERBOSE("BridgeScreen: unmapped KeyID 0x%04x", id);
    return 0;
  }
}

uint8_t macButtonToHid(KeyButton button)
{
  switch (button) {
  case 19: // macOS '1'
    return convertKeyID('1');
  case 20: // macOS '2'
    return convertKeyID('2');
  case 21: // macOS '3'
    return convertKeyID('3');
  case 22: // macOS '4'
    return convertKeyID('4');
  case 24: // macOS '5'
    return convertKeyID('5');
  case 23: // macOS '6'
    return convertKeyID('6');
  case 27: // macOS '7'
    return convertKeyID('7');
  case 29: // macOS '8'
    return convertKeyID('8');
  case 26: // macOS '9'
    return convertKeyID('9');
  case 30: // macOS '0'
    return convertKeyID('0');
  case 28: // macOS '-'
    return 0x2d;
  case 25: // macOS '='
    return 0x2e;
  case 52: // macOS Backspace
    return 0x2a;
  case 49: // macOS Tab
    return 0x2b;
  case 13: // macOS 'q'
    return convertKeyID('q');
  case 14: // macOS 'w'
    return convertKeyID('w');
  case 15: // macOS 'e'
    return convertKeyID('e');
  case 16: // macOS 'r'
    return convertKeyID('r');
  case 18: // macOS 't'
    return convertKeyID('t');
  case 17: // macOS 'y'
    return convertKeyID('y');
  case 33: // macOS 'u'
    return convertKeyID('u');
  case 35: // macOS 'i'
    return convertKeyID('i');
  case 32: // macOS 'o', Linux 'o' (overlap)
    return convertKeyID('o');
  case 36: // macOS 'p'
    return convertKeyID('p');
  case 34: // macOS '[', Linux '[' (overlap)
    return 0x2f;
  case 31: // macOS ']'
    return 0x30;
  case 37: // macOS Enter
    return 0x28;
  case 1: // macOS 'a'
    return convertKeyID('a');
  case 2: // macOS 's'
    return convertKeyID('s');
  case 3: // macOS 'd'
    return convertKeyID('d');
  case 4: // macOS 'f'
    return convertKeyID('f');
  case 6: // macOS 'g'
    return convertKeyID('g');
  case 5: // macOS 'h'
    return convertKeyID('h');
  case 39: // macOS 'j'
    return convertKeyID('j');
  case 41: // macOS 'k'
    return convertKeyID('k');
  case 38: // macOS 'l'
    return convertKeyID('l');
  case 42: // macOS ';'
    return 0x33;
  case 40: // macOS '\''
    return 0x34;
  case 51: // macOS '`'
    return 0x35;
  case 43: // macOS '\'
    return 0x31;
  case 7: // macOS 'z'
    return convertKeyID('z');
  case 8: // macOS 'x'
    return convertKeyID('x');
  case 9: // macOS 'c'
    return convertKeyID('c');
  case 10: // macOS 'v'
    return convertKeyID('v');
  case 12: // macOS 'b'
    return convertKeyID('b');
  case 46: // macOS 'n'
    return convertKeyID('n');
  case 47: // macOS 'm'
    return convertKeyID('m');
  case 44: // macOS ','
    return 0x36;
  case 48: // macOS '.'
    return 0x37;
  case 45: // macOS '/'
    return 0x38;
  case 50: // macOS Space
    return 0x2c;
  case 58: // macOS CapsLock
    return 0x39;
  case 123: // macOS F1
    return 0x3A;
  case 121: // macOS F2
    return 0x3B;
  case 100: // macOS F3
    return 0x3C;
  case 119: // macOS F4
    return 0x3D;
  case 97: // macOS F5
    return 0x3E;
  case 98: // macOS F6
    return 0x3F;
  case 99: // macOS F7
    return 0x40;
  case 101: // macOS F8
    return 0x41;
  case 102: // macOS F9
    return 0x42;
  case 110: // macOS F10
    return 0x43;
  case 72: // macOS Numlock (Clear)
    return 0x53;
  case 127: // macOS Up
    return 0x52;
  case 124: // macOS Left
    return 0x50;
  case 125: // macOS Right
    return 0x4F;
  case 126: // macOS Down
    return 0x51;
  case 115: // macOS Help
    return 0x49;
  case 118: // macOS Delete
    return 0x4C;
  case 104: // macOS F11
    return 0x44;
  case 112: // macOS F12
    return 0x45;
  case 111: // macOS F13 (Help/Insert on some, or just F13)
    return 0x65;
  default:
    LOG_VERBOSE("BridgeScreen: unmapped button code %u", button);
    return 0;
  }
}

uint8_t linuxButtonToHid(KeyButton button)
{
  switch (button) {
  case 10: // Linux '1'
  case 90: // Linux '1' (alt?)
    return convertKeyID('1');
  case 11: // Linux '2'
    return convertKeyID('2');
  case 12: // Linux '3'
    return convertKeyID('3');
  case 13: // Linux '4'
    return convertKeyID('4');
  case 14: // Linux '5'
    return convertKeyID('5');
  case 15: // Linux '6'
    return convertKeyID('6');
  case 16: // Linux '7'
    return convertKeyID('7');
  case 17: // Linux '8'
    return convertKeyID('8');
  case 18: // Linux '9'
    return convertKeyID('9');
  case 19: // Linux '0'
    return convertKeyID('0');
  case 20: // Linux '-'
    return 0x2d;
  case 21: // Linux '='
    return 0x2e;
  case 22: // Linux Backspace
    return 0x2a;
  case 23: // Linux Tab
    return 0x2b;
  case 24: // Linux 'q'
    return convertKeyID('q');
  case 25: // Linux 'w'
    return convertKeyID('w');
  case 26: // Linux 'e'
    return convertKeyID('e');
  case 27: // Linux 'r'
    return convertKeyID('r');
  case 28: // Linux 't'
    return convertKeyID('t');
  case 29: // Linux 'y'
    return convertKeyID('y');
  case 30: // Linux 'u'
    return convertKeyID('u');
  case 31: // Linux 'i'
    return convertKeyID('i');
  case 32: // Linux 'o'
    return convertKeyID('o');
  case 33: // Linux 'p'
    return convertKeyID('p');
  case 34: // Linux '['
    return 0x2f;
  case 35: // Linux ']'
    return 0x30;
  case 36: // Linux Enter
    return 0x28;
  case 38: // Linux 'a'
    return convertKeyID('a');
  case 39: // Linux 's'
    return convertKeyID('s');
  case 40: // Linux 'd'
    return convertKeyID('d');
  case 41: // Linux 'f'
    return convertKeyID('f');
  case 42: // Linux 'g'
    return convertKeyID('g');
  case 43: // Linux 'h'
    return convertKeyID('h');
  case 44: // Linux 'j'
    return convertKeyID('j');
  case 45: // Linux 'k'
    return convertKeyID('k');
  case 46: // Linux 'l'
    return convertKeyID('l');
  case 47: // Linux ';'
    return 0x33;
  case 48: // Linux '\''
    return 0x34;
  case 49: // Linux '`'
    return 0x35;
  case 51: // Linux '\'
    return 0x31;
  case 52: // Linux 'z'
    return convertKeyID('z');
  case 53: // Linux 'x'
    return convertKeyID('x');
  case 54: // Linux 'c'
    return convertKeyID('c');
  case 55: // Linux 'v'
    return convertKeyID('v');
  case 56: // Linux 'b'
    return convertKeyID('b');
  case 57: // Linux 'n'
    return convertKeyID('n');
  case 58: // Linux 'm'
    return convertKeyID('m');
  case 59: // Linux ','
    return 0x36;
  case 60: // Linux '.'
    return 0x37;
  case 61: // Linux '/'
    return 0x38;
  case 65: // Linux Space
    return 0x2c;
  case 66: // Linux CapsLock
    return 0x39;
  case 67: // Linux F1
    return 0x3A;
  case 68: // Linux F2
    return 0x3B;
  case 69: // Linux F3
    return 0x3C;
  case 70: // Linux F4
    return 0x3D;
  case 71: // Linux F5
    return 0x3E;
  case 72: // Linux F6
    return 0x3F;
  case 73: // Linux F7
    return 0x40;
  case 74: // Linux F8
    return 0x41;
  case 75: // Linux F9
    return 0x42;
  case 76: // Linux F10
    return 0x43;
  case 77: // Linux NumLock
    return 0x53;
  case 78: // Linux ScrollLock
    return 0x47;
  case 107: // Linux PrintScreen
    return 0x46;
  case 111: // Linux Up
    return 0x52;
  case 113: // Linux Left
    return 0x50;
  case 114: // Linux Right
    return 0x4F;
  case 116: // Linux Down
    return 0x51;
  case 118: // Linux Insert
    return 0x49;
  case 119: // Linux Delete
    return 0x4C;
  case 127: // Linux Pause
    return 0x48;
  case 95: // Linux F11
    return 0x44;
  case 96: // Linux F12
    return 0x45;
  case 135: // Linux Menu
    return 0x65;
  default:
    LOG_VERBOSE("BridgeScreen: unmapped button code %u", button);
    return 0;
  }
}
} // namespace

BridgeKeyboard::BridgeKeyboard(KeyCodes keyCodes, KeySink keys, ConsumerSink consumer, DeadKeyFinder deadKeys)
    : m_keyCodes(keyCodes),
      m_keys(std::move(keys)),
      m_consumer(std::move(consumer)),
      m_deadKeys(std::move(deadKeys))
{
}

void BridgeKeyboard::setLayout(KeyboardLayout layout)
{
  m_layout = layout;
}

bool BridgeKeyboard::sendsKeysAsPressed() const
{
  return m_layout == KeyboardLayout::SameAsComputer && m_keyCodes == KeyCodes::Windows;
}

void BridgeKeyboard::keyDown(KeyID id, KeyModifierMask mask, KeyButton button)
{
  LOG_DEBUG("BridgeScreen: key down id=0x%04x button=%d", id, button);

  const uint16_t consumerCode = convertMediaKeyToConsumerControl(id);
  if (consumerCode != 0) {
    LOG_DEBUG("BridgeScreen: media key press, consumer code=0x%04x", consumerCode);
    m_pressedButtons.insert(button);
    m_pressedConsumerControls[button] = consumerCode;
    if (!m_consumer(HidEventType::ConsumerControlPress, consumerCode)) {
      LOG_ERR("BridgeScreen: failed to send consumer control press");
    }
    return;
  }

  const uint8_t bit = modifierBit(id, button);
  const uint8_t key = (bit != 0) ? 0 : hidKey(id, button);
  const uint8_t modifiers = keyModifiers(mask, heldModifierBits() | bit) | bit;

  m_activeModifiers = mask;
  m_currentHidModifiers = modifiers;
  m_pressedButtons.insert(button);
  m_activeKeys[button] = ActiveKey{key, bit};

  if (key == 0 && modifiers == 0) {
    return;
  }

  if (bit == 0 && sendsKeysAsPressed()) {
    sendDeadKey(id, button);
  }
  if (!m_keys(HidEventType::KeyboardPress, modifiers, key)) {
    LOG_ERR("BridgeScreen: failed to send key press");
  }
}

bool BridgeKeyboard::keyRepeat(KeyID id, KeyModifierMask mask, int32_t count, KeyButton button)
{
  LOG_DEBUG("BridgeScreen: key repeat id=0x%04x button=%d count=%d", id, button, count);

  if (count <= 0) {
    return true;
  }

  const uint16_t consumerCode = convertMediaKeyToConsumerControl(id);
  if (consumerCode != 0) {
    for (int32_t i = 0; i < count; ++i) {
      if (!m_consumer(HidEventType::ConsumerControlPress, consumerCode) ||
          !m_consumer(HidEventType::ConsumerControlRelease, consumerCode)) {
        LOG_ERR("BridgeScreen: failed to send consumer control repeat");
        return false;
      }
    }
    return true;
  }

  const uint8_t bit = modifierBit(id, button);
  const uint8_t key = (bit != 0) ? 0 : hidKey(id, button);
  const uint8_t modifiers = keyModifiers(mask, heldModifierBits() | bit) | bit;

  if (key == 0 && modifiers == 0) {
    return true;
  }

  for (int32_t i = 0; i < count; ++i) {
    if (!m_keys(HidEventType::KeyboardPress, modifiers, key) ||
        !m_keys(HidEventType::KeyboardRelease, modifiers, key)) {
      LOG_ERR("BridgeScreen: failed to send keyboard repeat");
      return false;
    }
  }

  return true;
}

void BridgeKeyboard::keyUp(KeyButton button)
{
  LOG_DEBUG("BridgeScreen: key up button=%d", button);

  if (auto it = m_pressedConsumerControls.find(button); it != m_pressedConsumerControls.end()) {
    const uint16_t consumerCode = it->second;
    LOG_DEBUG("BridgeScreen: media key release, consumer code=0x%04x", consumerCode);
    m_pressedButtons.erase(button);
    m_pressedConsumerControls.erase(it);
    if (!m_consumer(HidEventType::ConsumerControlRelease, consumerCode)) {
      LOG_ERR("BridgeScreen: failed to send consumer control release");
    }
    return;
  }

  uint8_t key = 0;
  uint8_t bit = 0;
  if (auto it = m_activeKeys.find(button); it != m_activeKeys.end()) {
    key = it->second.hidKey;
    bit = it->second.modifierBit;
    m_activeKeys.erase(it);
  } else if (modifierBit(0, button) == 0) {
    key = buttonToHid(button);
  }

  m_pressedButtons.erase(button);
  if (bit != 0) {
    m_currentHidModifiers &= ~bit;
  } else if (sendsKeysAsPressed()) {
    // still AltGr, if the last key typed with Control and Alt as AltGr
    m_currentHidModifiers = heldHidModifiers(m_activeModifiers, heldModifierBits());
  } else {
    m_currentHidModifiers = heldModifierBits();
  }

  if (!m_keys(HidEventType::KeyboardRelease, m_currentHidModifiers, key)) {
    LOG_ERR("BridgeScreen: failed to send key release");
  }
}

void BridgeKeyboard::allKeysUp()
{
  LOG_DEBUG("BridgeScreen: all keys up");

  for (const auto &[button, consumerCode] : m_pressedConsumerControls) {
    if (!m_consumer(HidEventType::ConsumerControlRelease, consumerCode)) {
      LOG_ERR("BridgeScreen: failed to send consumer control release for 0x%04x", consumerCode);
    }
  }

  for (const auto &[button, state] : m_activeKeys) {
    if (state.modifierBit != 0) {
      m_currentHidModifiers &= ~state.modifierBit;
    }
    if (!m_keys(HidEventType::KeyboardRelease, m_currentHidModifiers, state.hidKey)) {
      LOG_ERR("BridgeScreen: failed to send key release for %u", state.hidKey);
    }
  }

  m_pressedConsumerControls.clear();
  m_pressedButtons.clear();
  m_activeKeys.clear();
  m_currentHidModifiers = 0;
  m_activeModifiers = 0;
}

bool BridgeKeyboard::isKeyDown(KeyButton button) const
{
  return m_pressedButtons.find(button) != m_pressedButtons.end();
}

KeyModifierMask BridgeKeyboard::activeModifiers() const
{
  return m_activeModifiers;
}

const std::set<KeyButton> &BridgeKeyboard::pressedButtons() const
{
  return m_pressedButtons;
}

uint8_t BridgeKeyboard::modifierBit(KeyID id, KeyButton button) const
{
  if (const uint8_t bit = modifierBitForKey(id); bit != 0) {
    return bit;
  }
  switch (m_keyCodes) {
  case KeyCodes::MacOS:
    return macModifierBit(button);
  case KeyCodes::Windows:
    return windowsModifierBit(button);
  case KeyCodes::Linux:
    return linuxModifierBit(button);
  }
  return 0;
}

uint8_t BridgeKeyboard::keyModifiers(KeyModifierMask mask, uint8_t held) const
{
  if (sendsKeysAsPressed()) {
    // and a modifier no key holds, as in a keystroke the server makes
    return heldHidModifiers(mask, held) | maskHidModifiers(mask, held);
  }
  return maskHidModifiers(mask, held);
}

uint8_t BridgeKeyboard::hidKey(KeyID id, KeyButton button) const
{
  if (sendsKeysAsPressed()) {
    if (const uint8_t key = buttonToHid(button); key != 0) {
      return key;
    }
    // a key without a key code, as in a keystroke the server makes, goes by its character
  }
  if (const uint8_t key = convertKeyID(id); key != 0) {
    return key;
  }
  return buttonToHid(button);
}

uint8_t BridgeKeyboard::buttonToHid(KeyButton button) const
{
  switch (m_keyCodes) {
  case KeyCodes::MacOS:
    return macButtonToHid(button);
  case KeyCodes::Windows:
    if (const uint8_t key = windowsScanCodeToHid(button); key != 0) {
      return key;
    }
    LOG_VERBOSE("BridgeScreen: unmapped button code %u", button);
    return 0;
  case KeyCodes::Linux:
    return linuxButtonToHid(button);
  }
  return 0;
}

uint8_t BridgeKeyboard::heldModifierBits() const
{
  uint8_t bits = 0;
  for (const auto &entry : m_activeKeys) {
    bits |= entry.second.modifierBit;
  }
  return bits;
}

void BridgeKeyboard::sendDeadKey(KeyID id, KeyButton button) const
{
  uint16_t deadScanCode = 0;
  uint8_t deadModifiers = 0;
  if (m_keyCodes != KeyCodes::Windows || !m_deadKeys || !m_deadKeys(id, button, deadScanCode, deadModifiers)) {
    return;
  }
  const uint8_t deadKey = windowsScanCodeToHid(deadScanCode);
  if (deadKey == 0) {
    return;
  }
  LOG_DEBUG("BridgeScreen: dead key 0x%02x before id=0x%04x", deadKey, id);
  if (!m_keys(HidEventType::KeyboardPress, deadModifiers, deadKey) ||
      !m_keys(HidEventType::KeyboardRelease, deadModifiers, deadKey)) {
    LOG_ERR("BridgeScreen: failed to send dead key");
  }
}

} // namespace deskflow::bridge
