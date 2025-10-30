/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "BridgePlatformScreen.h"

#include "HidFrame.h"
#include "base/Log.h"

#include <cstring>

namespace deskflow::bridge {

BridgePlatformScreen::BridgePlatformScreen(
    IEventQueue *events, std::shared_ptr<CdcTransport> transport, const PicoConfig &config
) :
    PlatformScreen(events, false), // invertScrollDirection = false by default
    m_transport(transport),
    m_config(config)
{
  LOG_INFO("BridgeScreen: initialized for arch=%s screen=%dx%d", m_config.arch.c_str(), m_config.screenWidth, m_config.screenHeight);
}

BridgePlatformScreen::~BridgePlatformScreen() = default;

void *BridgePlatformScreen::getEventTarget() const
{
  return const_cast<BridgePlatformScreen *>(this);
}

bool BridgePlatformScreen::getClipboard(ClipboardID id, IClipboard *clipboard) const
{
  // Bridge doesn't support clipboard retrieval
  return false;
}

void BridgePlatformScreen::getShape(int32_t &x, int32_t &y, int32_t &width, int32_t &height) const
{
  x = 0;
  y = 0;
  width = m_config.screenWidth;
  height = m_config.screenHeight;
}

void BridgePlatformScreen::getCursorPos(int32_t &x, int32_t &y) const
{
  x = m_cursorX;
  y = m_cursorY;
}

void BridgePlatformScreen::reconfigure(uint32_t activeSides)
{
  LOG_DEBUG("BridgeScreen: reconfigure sides=%s", sidesMaskToString(activeSides).c_str());
}

uint32_t BridgePlatformScreen::activeSides()
{
  // Bridge screen is always active on all sides
  return 0x0F; // All sides
}

void BridgePlatformScreen::warpCursor(int32_t x, int32_t y)
{
  m_cursorX = x;
  m_cursorY = y;
  // Note: Absolute cursor positioning not sent to Pico
  // Mobile devices typically use relative movement
}

uint32_t BridgePlatformScreen::registerHotKey(KeyID key, KeyModifierMask mask)
{
  // Bridge doesn't support hotkeys
  return 0;
}

void BridgePlatformScreen::unregisterHotKey(uint32_t id)
{
  // Bridge doesn't support hotkeys
}

void BridgePlatformScreen::fakeInputBegin()
{
  // No special handling needed
}

void BridgePlatformScreen::fakeInputEnd()
{
  // No special handling needed
}

int32_t BridgePlatformScreen::getJumpZoneSize() const
{
  return 0; // No jump zones for bridge
}

bool BridgePlatformScreen::isAnyMouseButtonDown(uint32_t &buttonID) const
{
  if (m_mouseButtons != 0) {
    // Find first pressed button
    for (uint32_t i = 0; i < 8; i++) {
      if (m_mouseButtons & (1 << i)) {
        buttonID = i + 1;
        return true;
      }
    }
  }
  return false;
}

void BridgePlatformScreen::getCursorCenter(int32_t &x, int32_t &y) const
{
  x = m_config.screenWidth / 2;
  y = m_config.screenHeight / 2;
}

void BridgePlatformScreen::fakeMouseButton(ButtonID id, bool press)
{
  LOG_DEBUG("BridgeScreen: mouse button %d %s", id, press ? "press" : "release");

  if (id == 0 || id > 8) {
    return;
  }

  uint8_t buttonBit = 1 << (id - 1);

  if (press) {
    m_mouseButtons |= buttonBit;
  } else {
    m_mouseButtons &= ~buttonBit;
  }

  sendMouseReport();
}

void BridgePlatformScreen::fakeMouseMove(int32_t x, int32_t y)
{
  LOG_DEBUG2("BridgeScreen: mouse move to %d,%d", x, y);

  // Calculate delta from current position
  int32_t dx = x - m_cursorX;
  int32_t dy = y - m_cursorY;

  m_cursorX = x;
  m_cursorY = y;

  fakeMouseRelativeMove(dx, dy);
}

void BridgePlatformScreen::fakeMouseRelativeMove(int32_t dx, int32_t dy) const
{
  LOG_DEBUG2("BridgeScreen: mouse relative move %d,%d", dx, dy);

  // HID mouse reports use int8_t, so we need to send multiple reports for large movements
  while (dx != 0 || dy != 0) {
    int8_t reportDx = (dx > 127) ? 127 : (dx < -127) ? -127 : static_cast<int8_t>(dx);
    int8_t reportDy = (dy > 127) ? 127 : (dy < -127) ? -127 : static_cast<int8_t>(dy);

    MouseReport report;
    report.buttons = m_mouseButtons;
    report.x = reportDx;
    report.y = reportDy;

    HidFrame frame;
    frame.type = HidReportType::Mouse;
    frame.payload = report.toPayload();

    if (!const_cast<BridgePlatformScreen *>(this)->m_transport->sendHidFrame(frame)) {
      LOG_ERR("BridgeScreen: failed to send mouse report");
      break;
    }

    dx -= reportDx;
    dy -= reportDy;
  }
}

void BridgePlatformScreen::fakeMouseWheel(int32_t xDelta, int32_t yDelta) const
{
  LOG_DEBUG("BridgeScreen: mouse wheel x=%d y=%d", xDelta, yDelta);

  // Convert to HID wheel deltas (typically scaled down)
  int8_t wheelY = static_cast<int8_t>(yDelta / 120); // Standard wheel delta is 120
  int8_t wheelX = static_cast<int8_t>(xDelta / 120);

  MouseWheelReport report;
  report.wheel = wheelY;
  report.hwheel = wheelX;

  HidFrame frame;
  frame.type = HidReportType::MouseWheel;
  frame.payload = report.toPayload();

  if (!const_cast<BridgePlatformScreen *>(this)->m_transport->sendHidFrame(frame)) {
    LOG_ERR("BridgeScreen: failed to send wheel report");
  }
}

void BridgePlatformScreen::fakeKeyDown(KeyID id, KeyModifierMask mask, KeyButton button, const std::string &lang)
{
  LOG_DEBUG("BridgeScreen: key down id=0x%04x button=%d", id, button);

  uint8_t hidKey = mapKeyToHid(id, button);
  m_keyModifiers = mapModifiersToHid(mask);
  m_activeModifiers = mask;
  m_pressedButtons.insert(button);

  // Add key to pressed keys array (if not already present)
  if (m_numPressedKeys < 6) {
    bool alreadyPressed = false;
    for (size_t i = 0; i < m_numPressedKeys; i++) {
      if (m_pressedKeys[i] == hidKey) {
        alreadyPressed = true;
        break;
      }
    }

    if (!alreadyPressed && hidKey != 0) {
      m_pressedKeys[m_numPressedKeys++] = hidKey;
    }
  }

  sendKeyboardReport();
}

bool BridgePlatformScreen::fakeKeyRepeat(
    KeyID id, KeyModifierMask mask, int32_t count, KeyButton button, const std::string &lang
)
{
  // Key repeat is handled by sending the same key down event
  for (int32_t i = 0; i < count; i++) {
    fakeKeyDown(id, mask, button, lang);
  }
  return true;
}

bool BridgePlatformScreen::fakeKeyUp(KeyButton button)
{
  LOG_DEBUG("BridgeScreen: key up button=%d", button);

  // We need to map the button back to HID code to remove it
  // For now, we'll use a simplified approach
  uint8_t hidKey = mapKeyToHid(0, button);

  // Remove key from pressed keys array
  for (size_t i = 0; i < m_numPressedKeys; i++) {
    if (m_pressedKeys[i] == hidKey) {
      // Shift remaining keys
      for (size_t j = i; j < m_numPressedKeys - 1; j++) {
        m_pressedKeys[j] = m_pressedKeys[j + 1];
      }
      m_pressedKeys[--m_numPressedKeys] = 0;
      break;
    }
  }

  m_pressedButtons.erase(button);
  if (m_pressedButtons.empty()) {
    m_activeModifiers = 0;
    m_keyModifiers = 0;
  }

  sendKeyboardReport();
  return true;
}

void BridgePlatformScreen::fakeAllKeysUp()
{
  LOG_DEBUG("BridgeScreen: all keys up");

  m_keyModifiers = 0;
  m_numPressedKeys = 0;
  std::memset(m_pressedKeys, 0, sizeof(m_pressedKeys));
  m_pressedButtons.clear();
  m_activeModifiers = 0;

  sendKeyboardReport();
}

void BridgePlatformScreen::updateKeyMap()
{
  // No native key map available for bridge mode
}

void BridgePlatformScreen::updateKeyState()
{
  // Bridge relies on internal state tracking only
}

void BridgePlatformScreen::setHalfDuplexMask(KeyModifierMask)
{
  // Half-duplex toggles not applicable
}

bool BridgePlatformScreen::fakeCtrlAltDel()
{
  // Allow normal processing; bridge does not synthesize CAD
  return false;
}

bool BridgePlatformScreen::isKeyDown(KeyButton button) const
{
  return m_pressedButtons.find(button) != m_pressedButtons.end();
}

KeyModifierMask BridgePlatformScreen::getActiveModifiers() const
{
  return m_activeModifiers;
}

KeyModifierMask BridgePlatformScreen::pollActiveModifiers() const
{
  return m_activeModifiers;
}

int32_t BridgePlatformScreen::pollActiveGroup() const
{
  return 0;
}

void BridgePlatformScreen::pollPressedKeys(IKeyState::KeyButtonSet &pressedKeys) const
{
  pressedKeys.insert(m_pressedButtons.begin(), m_pressedButtons.end());
}

void BridgePlatformScreen::enable()
{
  LOG_INFO("BridgeScreen: enabled");
  m_enabled = true;
}

void BridgePlatformScreen::disable()
{
  LOG_INFO("BridgeScreen: disabled");
  m_enabled = false;
}

void BridgePlatformScreen::enter()
{
  LOG_DEBUG("BridgeScreen: enter");
}

bool BridgePlatformScreen::canLeave()
{
  return true;
}

void BridgePlatformScreen::leave()
{
  LOG_DEBUG("BridgeScreen: leave");
  fakeAllKeysUp(); // Release all keys when leaving
}

bool BridgePlatformScreen::setClipboard(ClipboardID id, const IClipboard *clipboard)
{
  // Clipboard is discarded per plan
  LOG_DEBUG("BridgeScreen: clipboard discarded");
  return false;
}

void BridgePlatformScreen::checkClipboards()
{
  // No clipboard support
}

void BridgePlatformScreen::openScreensaver(bool notify)
{
  // No screensaver support
}

void BridgePlatformScreen::closeScreensaver()
{
  // No screensaver support
}

void BridgePlatformScreen::screensaver(bool activate)
{
  // No screensaver support
}

void BridgePlatformScreen::resetOptions()
{
  // No options to reset
}

void BridgePlatformScreen::setOptions(const OptionsList &options)
{
  // No options supported
}

void BridgePlatformScreen::setSequenceNumber(uint32_t seqNum)
{
  m_sequenceNumber = seqNum;
}

bool BridgePlatformScreen::isPrimary() const
{
  return false; // Bridge screen is always a secondary screen
}

std::string BridgePlatformScreen::getSecureInputApp() const
{
  // Bridge doesn't support secure input notification
  return "";
}

void BridgePlatformScreen::updateButtons()
{
  // No button mapping needed for bridge
}

IKeyState *BridgePlatformScreen::getKeyState() const
{
  // Bridge doesn't maintain key state in the traditional sense
  return nullptr;
}

void BridgePlatformScreen::handleSystemEvent(const Event &event)
{
  // Bridge doesn't handle system events
}

void BridgePlatformScreen::sendKeyboardReport()
{
  KeyboardReport report;
  report.modifiers = m_keyModifiers;
  report.reserved = 0;
  std::memcpy(report.keycodes, m_pressedKeys, 6);

  HidFrame frame;
  frame.type = HidReportType::Keyboard;
  frame.payload = report.toPayload();

  if (!m_transport->sendHidFrame(frame)) {
    LOG_ERR("BridgeScreen: failed to send keyboard report");
  }
}

void BridgePlatformScreen::sendMouseReport()
{
  MouseReport report;
  report.buttons = m_mouseButtons;
  report.x = 0; // Position updates sent via fakeMouseRelativeMove
  report.y = 0;

  HidFrame frame;
  frame.type = HidReportType::Mouse;
  frame.payload = report.toPayload();

  if (!m_transport->sendHidFrame(frame)) {
    LOG_ERR("BridgeScreen: failed to send mouse report");
  }
}

uint8_t BridgePlatformScreen::mapKeyToHid(KeyID key, KeyButton button) const
{
  // TODO: Implement proper KeyID -> HID keycode mapping
  // For now, return button as-is (this is a placeholder)
  // Full implementation would require a comprehensive mapping table
  // based on the target arch (iOS vs Android)

  // Simplified mapping for common keys (placeholder)
  if (button >= 4 && button <= 231) {
    return static_cast<uint8_t>(button - 4); // Approximate HID mapping
  }

  return 0; // No key
}

uint8_t BridgePlatformScreen::mapModifiersToHid(KeyModifierMask mask) const
{
  uint8_t hidModifiers = 0;

  // Deskflow modifier mask to HID modifier bitmap mapping
  // HID modifiers: Ctrl=0x01, Shift=0x02, Alt=0x04, GUI=0x08 (left)
  //                Ctrl=0x10, Shift=0x20, Alt=0x40, GUI=0x80 (right)

  // This is a placeholder - full implementation would need proper mask values
  // from Deskflow's KeyModifierMask definitions

  return hidModifiers;
}

} // namespace deskflow::bridge
