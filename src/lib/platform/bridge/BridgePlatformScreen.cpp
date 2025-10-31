/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "BridgePlatformScreen.h"

#include "HidFrame.h"
#include "base/Event.h"
#include "base/Log.h"

#include <algorithm>
#include <iomanip>
#include <cstring>
#include <sstream>
#include <utility>

namespace deskflow::bridge {

namespace {
constexpr int kMinMouseDelta = -127;
constexpr int kMaxMouseDelta = 127;
constexpr int kDebugScreenWidth = 1920;
constexpr int kDebugScreenHeight = 1080;
constexpr double kMouseFlushIntervalSeconds = 1.0 / 100.0;
constexpr int kMaxAccumulatedDelta = kMaxMouseDelta * 100; // Roughly 1 second of buffered motion

std::string hexDump(const uint8_t *data, size_t length, size_t maxBytes = 32)
{
  if (data == nullptr || length == 0) {
    return {};
  }

  const size_t limit = std::min(length, maxBytes);

  std::ostringstream oss;
  oss << std::hex << std::uppercase << std::setfill('0');
  for (size_t i = 0; i < limit; ++i) {
    if (i > 0)
      oss << ' ';
    oss << std::setw(2) << static_cast<unsigned>(data[i]);
  }

  if (length > maxBytes) {
    oss << " ...";
  }

  return oss.str();
}
}

BridgePlatformScreen::BridgePlatformScreen(
    IEventQueue *events, std::shared_ptr<CdcTransport> transport, const PicoConfig &config
) :
    PlatformScreen(events, false), // invertScrollDirection = false by default
    m_transport(std::move(transport)),
    m_config(config),
    m_events(events)
{
  LOG_INFO(
      "BridgeScreen: initialized for arch=%s screen=%dx%d (debug override active)",
      m_config.arch.c_str(),
      kDebugScreenWidth,
      kDebugScreenHeight
  );

  if (m_events != nullptr) {
    m_events->addHandler(deskflow::EventTypes::Timer, this, [this](const Event &event) {
      handleMouseFlushTimer(event);
    });
    m_mouseFlushTimer = m_events->newTimer(kMouseFlushIntervalSeconds, this);
  }
}

BridgePlatformScreen::~BridgePlatformScreen()
{
  if (m_events != nullptr) {
    stopMouseFlushTimer();
    m_events->removeHandler(deskflow::EventTypes::Timer, this);
  }
}

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
  width = kDebugScreenWidth;
  height = kDebugScreenHeight;
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
  x = kDebugScreenWidth / 2;
  y = kDebugScreenHeight / 2;
}

void BridgePlatformScreen::fakeMouseButton(ButtonID id, bool press)
{
  LOG_DEBUG("BridgeScreen: mouse button %d %s", id, press ? "press" : "release");

  const uint8_t hidButton = convertButtonID(id);
  if (hidButton == 0) {
    return;
  }

  if (press) {
    m_mouseButtons |= hidButton;
    if (!sendMouseButtonEvent(HidEventType::MouseButtonPress, hidButton)) {
      LOG_ERR("BridgeScreen: failed to send mouse button press");
    }
  } else {
    m_mouseButtons &= static_cast<uint8_t>(~hidButton);
    if (!sendMouseButtonEvent(HidEventType::MouseButtonRelease, hidButton)) {
      LOG_ERR("BridgeScreen: failed to send mouse button release");
    }
  }
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

  if (dx == 0 && dy == 0) {
    return;
  }

  m_pendingDx = std::clamp(
      m_pendingDx + static_cast<int64_t>(dx),
      static_cast<int64_t>(-kMaxAccumulatedDelta),
      static_cast<int64_t>(kMaxAccumulatedDelta)
  );
  m_pendingDy = std::clamp(
      m_pendingDy + static_cast<int64_t>(dy),
      static_cast<int64_t>(-kMaxAccumulatedDelta),
      static_cast<int64_t>(kMaxAccumulatedDelta)
  );
}

void BridgePlatformScreen::fakeMouseWheel(int32_t, int32_t yDelta) const
{
  LOG_DEBUG("BridgeScreen: mouse wheel y=%d", yDelta);

  if (yDelta == 0) {
    return;
  }

  yDelta = std::clamp(yDelta, -128, 127);
  if (!sendMouseScrollEvent(static_cast<int8_t>(yDelta))) {
    LOG_ERR("BridgeScreen: failed to send scroll event");
  }
}

void BridgePlatformScreen::handleMouseFlushTimer(const Event &event) const
{
  if (event.getTarget() != this) {
    return;
  }

  const auto *timerEvent = static_cast<IEventQueue::TimerEvent *>(event.getData());
  if (timerEvent == nullptr || timerEvent->m_timer != m_mouseFlushTimer) {
    return;
  }

  flushPendingMouse(std::max<uint32_t>(1, timerEvent->m_count));
}

void BridgePlatformScreen::flushPendingMouse(uint32_t repeatCount) const
{
  constexpr uint32_t kMaxFramesPerTick = 16;
  uint32_t budget = std::max<uint32_t>(1, repeatCount) * kMaxFramesPerTick;

  while (budget-- > 0) {
    if (m_pendingDx == 0 && m_pendingDy == 0) {
      break;
    }

    const int64_t stepDx = std::clamp(
        m_pendingDx, static_cast<int64_t>(kMinMouseDelta), static_cast<int64_t>(kMaxMouseDelta)
    );
    const int64_t stepDy = std::clamp(
        m_pendingDy, static_cast<int64_t>(kMinMouseDelta), static_cast<int64_t>(kMaxMouseDelta)
    );

    if (stepDx == 0 && stepDy == 0) {
      break;
    }

    if (!sendMouseMoveEvent(static_cast<int16_t>(stepDx), static_cast<int16_t>(stepDy))) {
      LOG_ERR("BridgeScreen: failed to send mouse move");
      break;
    }

    m_pendingDx -= stepDx;
    m_pendingDy -= stepDy;
  }
}

void BridgePlatformScreen::stopMouseFlushTimer() const
{
  if (m_events != nullptr && m_mouseFlushTimer != nullptr) {
    m_events->deleteTimer(m_mouseFlushTimer);
    m_mouseFlushTimer = nullptr;
  }
}

void BridgePlatformScreen::resetMouseAccumulator() const
{
  m_pendingDx = 0;
  m_pendingDy = 0;
}

void BridgePlatformScreen::fakeKeyDown(KeyID id, KeyModifierMask mask, KeyButton button, const std::string &)
{
  LOG_DEBUG("BridgeScreen: key down id=0x%04x button=%d", id, button);

  const uint8_t hidKey = convertKey(id, button);
  uint8_t hidModifiers = convertModifiers(mask);

  if (uint8_t extra = modifierBitForKey(id); extra != 0) {
    hidModifiers |= extra;
  } else if (uint8_t extraBtn = modifierBitForButton(button); extraBtn != 0) {
    hidModifiers |= extraBtn;
  }

  m_activeModifiers = mask;
  m_pressedButtons.insert(button);
  if (hidKey != 0) {
    m_pressedKeycodes.insert(hidKey);
  }

  if (hidKey == 0 && hidModifiers == 0) {
    return;
  }

  if (!sendKeyboardEvent(HidEventType::KeyboardPress, hidModifiers, hidKey)) {
    LOG_ERR("BridgeScreen: failed to send key press");
  }
}

bool BridgePlatformScreen::fakeKeyRepeat(
    KeyID id, KeyModifierMask mask, int32_t count, KeyButton button, const std::string &
)
{
  for (int32_t i = 0; i < count; ++i) {
    fakeKeyDown(id, mask, button, {});
  }
  return true;
}

bool BridgePlatformScreen::fakeKeyUp(KeyButton button)
{
  LOG_DEBUG("BridgeScreen: key up button=%d", button);

  const uint8_t hidKey = convertKey(0, button);
  const uint8_t hidModifiers = convertModifiers(m_activeModifiers);

  m_pressedKeycodes.erase(hidKey);
  m_pressedButtons.erase(button);
  if (m_pressedButtons.empty()) {
    m_activeModifiers = 0;
  }

  if (hidKey == 0 && hidModifiers == 0) {
    return true;
  }

  if (!sendKeyboardEvent(HidEventType::KeyboardRelease, hidModifiers, hidKey)) {
    LOG_ERR("BridgeScreen: failed to send key release");
  }
  return true;
}

void BridgePlatformScreen::fakeAllKeysUp()
{
  LOG_DEBUG("BridgeScreen: all keys up");

  for (uint8_t hidKey : m_pressedKeycodes) {
    if (!sendKeyboardEvent(HidEventType::KeyboardRelease, 0, hidKey)) {
      LOG_ERR("BridgeScreen: failed to send key release for %u", hidKey);
    }
  }

  m_pressedKeycodes.clear();
  m_pressedButtons.clear();
  m_activeModifiers = 0;
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
  resetMouseAccumulator();

  if (m_events != nullptr && m_mouseFlushTimer == nullptr) {
    m_mouseFlushTimer = m_events->newTimer(kMouseFlushIntervalSeconds, this);
  }
}

void BridgePlatformScreen::disable()
{
  LOG_INFO("BridgeScreen: disabled");
  m_enabled = false;
  stopMouseFlushTimer();
  resetMouseAccumulator();
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

bool BridgePlatformScreen::sendEvent(HidEventType type, const std::vector<uint8_t> &payload) const
{
  std::string payloadHex = hexDump(payload.data(), payload.size(), 48);
  if (!payloadHex.empty()) {
    LOG_DEBUG(
        "BridgeScreen: TX HID type=0x%02x len=%zu payload=%s", static_cast<unsigned>(type), payload.size(),
        payloadHex.c_str()
    );
  } else {
    LOG_DEBUG("BridgeScreen: TX HID type=0x%02x len=%zu", static_cast<unsigned>(type), payload.size());
  }

  HidEventPacket packet;
  packet.type = type;
  packet.payload = payload;
  if (!m_transport->sendHidEvent(packet)) {
    LOG_ERR("BridgeScreen: failed to send HID event type=%u", static_cast<unsigned>(type));
    return false;
  }
  return true;
}

bool BridgePlatformScreen::sendKeyboardEvent(HidEventType type, uint8_t modifiers, uint8_t keycode) const
{
  const bool isPress = (type == HidEventType::KeyboardPress);
  const bool isRelease = (type == HidEventType::KeyboardRelease);

  if (isPress || isRelease) {
    if (m_transport->sendKeyboardCompact(modifiers, keycode, isPress)) {
      return true;
    }
    LOG_WARN("BridgeScreen: compact keyboard send failed, falling back to HID payload");
  }

  if (!sendEvent(type, {modifiers, keycode})) {
    LOG_ERR("BridgeScreen: failed to send keyboard event");
    return false;
  }
  return true;
}

bool BridgePlatformScreen::sendMouseMoveEvent(int16_t dx, int16_t dy) const
{
  if (!m_transport->sendMouseMoveCompact(dx, dy)) {
    LOG_ERR("BridgeScreen: failed to send mouse move event");
    return false;
  }
  return true;
}

bool BridgePlatformScreen::sendMouseButtonEvent(HidEventType type, uint8_t buttonMask) const
{
  if (!sendEvent(type, {buttonMask})) {
    LOG_ERR("BridgeScreen: failed to send mouse button event");
    return false;
  }
  return true;
}

bool BridgePlatformScreen::sendMouseScrollEvent(int8_t delta) const
{
  if (!sendEvent(HidEventType::MouseScroll, {static_cast<uint8_t>(delta)})) {
    LOG_ERR("BridgeScreen: failed to send scroll event");
    return false;
  }
  return true;
}

uint8_t BridgePlatformScreen::convertModifiers(KeyModifierMask mask) const
{
  uint8_t hidMods = 0;
  if (mask & KeyModifierShift)
    hidMods |= 0x02;
  if (mask & KeyModifierControl)
    hidMods |= 0x01;
  if (mask & KeyModifierAlt)
    hidMods |= 0x04;
  if (mask & KeyModifierAltGr)
    hidMods |= 0x40;
  if (mask & (KeyModifierSuper | KeyModifierMeta))
    hidMods |= 0x08;
  return hidMods;
}

uint8_t BridgePlatformScreen::modifierBitForKey(KeyID id) const
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

uint8_t BridgePlatformScreen::modifierBitForButton(KeyButton button) const
{
  switch (button) {
  case 50:
    return 0x02;
  case 62:
    return 0x20;
  case 37:
    return 0x01;
  case 105:
    return 0x10;
  case 64:
    return 0x04;
  case 108:
    return 0x40;
  case 133:
    return 0x08;
  case 134:
    return 0x80;
  default:
    return 0;
  }
}

uint8_t BridgePlatformScreen::convertKeyID(KeyID id) const
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
    LOG_DEBUG2("BridgeScreen: unmapped KeyID 0x%04x", id);
    return 0;
  }
}

uint8_t BridgePlatformScreen::convertKeyButton(KeyButton button) const
{
  switch (button) {
  case 10:
  case 90:
    return convertKeyID('1');
  case 11:
    return convertKeyID('2');
  case 12:
    return convertKeyID('3');
  case 13:
    return convertKeyID('4');
  case 14:
    return convertKeyID('5');
  case 15:
    return convertKeyID('6');
  case 16:
    return convertKeyID('7');
  case 17:
    return convertKeyID('8');
  case 18:
    return convertKeyID('9');
  case 19:
    return convertKeyID('0');
  case 20:
    return 0x2d;
  case 21:
    return 0x2e;
  case 22:
    return 0x2a;
  case 23:
    return 0x2b;
  case 24:
    return convertKeyID('q');
  case 25:
    return convertKeyID('w');
  case 26:
    return convertKeyID('e');
  case 27:
    return convertKeyID('r');
  case 28:
    return convertKeyID('t');
  case 29:
    return convertKeyID('y');
  case 30:
    return convertKeyID('u');
  case 31:
    return convertKeyID('i');
  case 32:
    return convertKeyID('o');
  case 33:
    return convertKeyID('p');
  case 34:
    return 0x2f;
  case 35:
    return 0x30;
  case 36:
    return 0x28;
  case 38:
    return convertKeyID('a');
  case 39:
    return convertKeyID('s');
  case 40:
    return convertKeyID('d');
  case 41:
    return convertKeyID('f');
  case 42:
    return convertKeyID('g');
  case 43:
    return convertKeyID('h');
  case 44:
    return convertKeyID('j');
  case 45:
    return convertKeyID('k');
  case 46:
    return convertKeyID('l');
  case 47:
    return 0x33;
  case 48:
    return 0x34;
  case 49:
    return 0x35;
  case 51:
    return 0x31;
  case 52:
    return convertKeyID('z');
  case 53:
    return convertKeyID('x');
  case 54:
    return convertKeyID('c');
  case 55:
    return convertKeyID('v');
  case 56:
    return convertKeyID('b');
  case 57:
    return convertKeyID('n');
  case 58:
    return convertKeyID('m');
  case 59:
    return 0x36;
  case 60:
    return 0x37;
  case 61:
    return 0x38;
  case 65:
    return 0x2c;
  case 66:
    return 0x39;
  case 67:
    return 0x3A;
  case 68:
    return 0x3B;
  case 69:
    return 0x3C;
  case 70:
    return 0x3D;
  case 71:
    return 0x3E;
  case 72:
    return 0x3F;
  case 73:
    return 0x40;
  case 74:
    return 0x41;
  case 75:
    return 0x42;
  case 76:
    return 0x43;
  case 77:
    return 0x53;
  case 78:
    return 0x47;
  case 107:
    return 0x46;
  case 111:
    return 0x52;
  case 113:
    return 0x50;
  case 114:
    return 0x4F;
  case 116:
    return 0x51;
  case 118:
    return 0x49;
  case 119:
    return 0x4C;
  case 127:
    return 0x48;
  case 95:
    return 0x44;
  case 96:
    return 0x45;
  case 135:
    return 0x65;
  default:
    LOG_DEBUG2("BridgeScreen: unmapped button code %u", button);
    return 0;
  }
}

uint8_t BridgePlatformScreen::convertKey(KeyID id, KeyButton button) const
{
  if (uint8_t code = convertKeyID(id); code != 0) {
    return code;
  }
  return convertKeyButton(button);
}

uint8_t BridgePlatformScreen::convertButtonID(ButtonID id) const
{
  switch (id) {
  case 1:
    return 0x01; // Left
  case 2:
    return 0x04; // Right
  case 3:
    return 0x02; // Middle
  default:
    return 0;
  }
}

} // namespace deskflow::bridge
