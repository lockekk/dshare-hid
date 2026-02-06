/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#include "BridgePlatformScreen.h"

#include "HidFrame.h"
#include "base/Event.h"
#include "base/EventTypes.h"
#include "base/Log.h"
#include "common/Settings.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <utility>

namespace deskflow::bridge {

namespace {
constexpr int kMinMouseDelta = -32767;
constexpr int kMaxMouseDelta = 32767;
constexpr std::chrono::seconds kKeepAliveInterval(30);
constexpr double kKeepAliveIntervalSeconds = static_cast<double>(kKeepAliveInterval.count());

std::string hexDump(const uint8_t *data, size_t length, size_t maxBytes = 32)
{
  if (data == nullptr || length == 0) {
    return {};
  }

  const size_t limit = (std::min)(length, maxBytes);

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
} // namespace

BridgePlatformScreen::BridgePlatformScreen(
    IEventQueue *events, std::shared_ptr<CdcTransport> transport, int32_t screenWidth, int32_t screenHeight
)
    : PlatformScreen(events),
      m_transport(std::move(transport)),
      m_screenWidth(screenWidth),
      m_screenHeight(screenHeight),
      m_events(events)
{
  LOG_INFO("BridgeScreen: initialized screen=%dx%d", m_screenWidth, m_screenHeight);

  m_lastCdcCommand = std::chrono::steady_clock::now();

  m_bluetoothKeepAliveEnabled = Settings::value(Settings::Bridge::BluetoothKeepAlive).toBool();
  if (m_bluetoothKeepAliveEnabled && m_events != nullptr) {
    LOG_INFO("BridgeScreen: Bluetooth keep-alive enabled, starting timer");
    m_events->addHandler(deskflow::EventTypes::Timer, this, [this](const Event &event) {
      handleKeepAliveTimer(event);
    });
    startKeepAliveTimer();
  }
}

BridgePlatformScreen::~BridgePlatformScreen()
{
  stopKeepAliveTimer();
  if (m_events != nullptr) {
    m_events->removeHandler(deskflow::EventTypes::Timer, this);
  }
}

void *BridgePlatformScreen::getEventTarget() const
{
  return const_cast<BridgePlatformScreen *>(this);
}

bool BridgePlatformScreen::getClipboard(ClipboardID id, IClipboard *clipboard) const
{
  return false;
}

void BridgePlatformScreen::getShape(int32_t &x, int32_t &y, int32_t &width, int32_t &height) const
{
  x = 0;
  y = 0;
  width = m_screenWidth;
  height = m_screenHeight;
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
  return 0x0F;
}

void BridgePlatformScreen::warpCursor(int32_t x, int32_t y)
{
  m_cursorX = x;
  m_cursorY = y;
}

uint32_t BridgePlatformScreen::registerHotKey(KeyID key, KeyModifierMask mask)
{
  return 0;
}

void BridgePlatformScreen::unregisterHotKey(uint32_t id)
{
}

void BridgePlatformScreen::fakeInputBegin()
{
}

void BridgePlatformScreen::fakeInputEnd()
{
}

int32_t BridgePlatformScreen::getJumpZoneSize() const
{
  return 0;
}

bool BridgePlatformScreen::isAnyMouseButtonDown(uint32_t &buttonID) const
{
  if (m_mouseButtons != 0) {
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
  x = m_screenWidth / 2;
  y = m_screenHeight / 2;
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

  int16_t stepDx =
      std::clamp(static_cast<int64_t>(dx), static_cast<int64_t>(kMinMouseDelta), static_cast<int64_t>(kMaxMouseDelta));
  int16_t stepDy =
      std::clamp(static_cast<int64_t>(dy), static_cast<int64_t>(kMinMouseDelta), static_cast<int64_t>(kMaxMouseDelta));

  if (!sendMouseMoveEvent(stepDx, stepDy)) {
    LOG_ERR("BridgeScreen: failed to send mouse move");
  }
}

const ScrollDelta rawDelta = {0, yDelta};
const ScrollDelta correctedDelta = applyClientScrollModifier(rawDelta);
yDelta = correctedDelta.yDelta;

int32_t speed = (m_scrollSpeed > 0) ? m_scrollSpeed : 120;
m_wheelAccumulatorY += (yDelta * speed);

constexpr int32_t kStepThreshold = 14400;
int8_t steps = static_cast<int8_t>(m_wheelAccumulatorY / kStepThreshold);

if (steps != 0) {
  m_wheelAccumulatorY %= kStepThreshold;

  if (!sendMouseScrollEvent(steps)) {
    LOG_ERR("BridgeScreen: failed to send scroll event");
  }
}
}

void BridgePlatformScreen::resetMouseAccumulator() const
{
  if (m_events) {
    m_wheelAccumulatorX = 0;
    m_wheelAccumulatorY = 0;
  }
}

void BridgePlatformScreen::fakeKeyDown(KeyID id, KeyModifierMask mask, KeyButton button, const std::string &)
{
  LOG_DEBUG("BridgeScreen: key down id=0x%04x button=%d", id, button);

  const uint16_t consumerCode = convertMediaKeyToConsumerControl(id);
  if (consumerCode != 0) {
    LOG_DEBUG("BridgeScreen: media key press, consumer code=0x%04x", consumerCode);
    m_pressedButtons.insert(button);
    m_pressedConsumerControls[button] = consumerCode;
    if (!sendConsumerControlEvent(HidEventType::ConsumerControlPress, consumerCode)) {
      LOG_ERR("BridgeScreen: failed to send consumer control press");
    }
    return;
  }

  const uint8_t hidKey = convertKey(id, button);
  uint8_t hidModifiers = convertModifiers(mask);
  uint8_t modifierBit = 0;

  if (uint8_t extra = modifierBitForKey(id); extra != 0) {
    modifierBit = extra;
    hidModifiers |= extra;
  } else if (uint8_t extraBtn = modifierBitForButton(button); extraBtn != 0) {
    modifierBit = extraBtn;
    hidModifiers |= extraBtn;
  }

  m_activeModifiers = mask;
  m_currentHidModifiers = hidModifiers;
  m_pressedButtons.insert(button);
  m_buttonToActiveKey[button] = ActiveKeyState{hidKey, modifierBit};

  if (hidKey == 0 && hidModifiers == 0) {
    return;
  }

  if (!sendKeyboardEvent(HidEventType::KeyboardPress, hidModifiers, hidKey)) {
    LOG_ERR("BridgeScreen: failed to send key press");
  }
}

bool BridgePlatformScreen::
    fakeKeyRepeat(KeyID id, KeyModifierMask mask, int32_t count, KeyButton button, const std::string &)
{
  LOG_DEBUG("BridgeScreen: key repeat id=0x%04x button=%d count=%d", id, button, count);

  if (count <= 0) {
    return true;
  }

  const uint16_t consumerCode = convertMediaKeyToConsumerControl(id);
  if (consumerCode != 0) {
    for (int32_t i = 0; i < count; ++i) {
      if (!sendConsumerControlEvent(HidEventType::ConsumerControlPress, consumerCode) ||
          !sendConsumerControlEvent(HidEventType::ConsumerControlRelease, consumerCode)) {
        LOG_ERR("BridgeScreen: failed to send consumer control repeat");
        return false;
      }
    }
    return true;
  }

  const uint8_t hidKey = convertKey(id, button);
  uint8_t hidModifiers = convertModifiers(mask);

  if (uint8_t extra = modifierBitForKey(id); extra != 0) {
    hidModifiers |= extra;
  } else if (uint8_t extraBtn = modifierBitForButton(button); extraBtn != 0) {
    hidModifiers |= extraBtn;
  }

  if (hidKey == 0 && hidModifiers == 0) {
    return true;
  }

  for (int32_t i = 0; i < count; ++i) {
    if (!sendKeyboardEvent(HidEventType::KeyboardPress, hidModifiers, hidKey) ||
        !sendKeyboardEvent(HidEventType::KeyboardRelease, hidModifiers, hidKey)) {
      LOG_ERR("BridgeScreen: failed to send keyboard repeat");
      return false;
    }
  }

  return true;
}

bool BridgePlatformScreen::fakeKeyUp(KeyButton button)
{
  LOG_DEBUG("BridgeScreen: key up button=%d", button);

  auto it = m_pressedConsumerControls.find(button);
  if (it != m_pressedConsumerControls.end()) {
    const uint16_t consumerCode = it->second;
    LOG_DEBUG("BridgeScreen: media key release, consumer code=0x%04x", consumerCode);
    m_pressedButtons.erase(button);
    m_pressedConsumerControls.erase(it);
    if (!sendConsumerControlEvent(HidEventType::ConsumerControlRelease, consumerCode)) {
      LOG_ERR("BridgeScreen: failed to send consumer control release");
    }
    return true;
  }

  uint8_t hidKey = 0;
  uint8_t modifierBit = 0;
  if (auto infoIt = m_buttonToActiveKey.find(button); infoIt != m_buttonToActiveKey.end()) {
    hidKey = infoIt->second.hidKey;
    modifierBit = infoIt->second.modifierBit;
    m_buttonToActiveKey.erase(infoIt);
  } else {
    hidKey = convertKey(0, button);
  }

  m_pressedButtons.erase(button);
  if (modifierBit != 0) {
    m_currentHidModifiers &= ~modifierBit;
  } else {
    m_currentHidModifiers = activeModifierBitmap();
  }

  if (!sendKeyboardEvent(HidEventType::KeyboardRelease, m_currentHidModifiers, hidKey)) {
    LOG_ERR("BridgeScreen: failed to send key release");
  }
  return true;
}

void BridgePlatformScreen::fakeAllKeysUp()
{
  LOG_DEBUG("BridgeScreen: all keys up");

  for (const auto &[button, consumerCode] : m_pressedConsumerControls) {
    if (!sendConsumerControlEvent(HidEventType::ConsumerControlRelease, consumerCode)) {
      LOG_ERR("BridgeScreen: failed to send consumer control release for 0x%04x", consumerCode);
    }
  }

  for (const auto &[button, state] : m_buttonToActiveKey) {
    if (state.modifierBit != 0) {
      m_currentHidModifiers &= ~state.modifierBit;
    }
    if (!sendKeyboardEvent(HidEventType::KeyboardRelease, m_currentHidModifiers, state.hidKey)) {
      LOG_ERR("BridgeScreen: failed to send key release for %u", state.hidKey);
    }
  }

  m_pressedConsumerControls.clear();
  m_pressedButtons.clear();
  m_buttonToActiveKey.clear();
  m_currentHidModifiers = 0;
  m_activeModifiers = 0;
}

void BridgePlatformScreen::updateKeyMap()
{
}

void BridgePlatformScreen::updateKeyState()
{
}

void BridgePlatformScreen::setHalfDuplexMask(KeyModifierMask)
{
}

bool BridgePlatformScreen::fakeCtrlAltDel()
{
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
}

void BridgePlatformScreen::disable()
{
  LOG_INFO("BridgeScreen: disabled");
  m_enabled = false;
  resetMouseAccumulator();
}

void BridgePlatformScreen::enter()
{
  LOG_DEBUG("BridgeScreen: enter");
  resetMouseAccumulator();
  if (m_transport != nullptr && !m_transport->open()) {
    LOG_WARN("BridgeScreen: failed to open transport on enter (%s)", m_transport->lastError().c_str());
  }

  if (m_transport != nullptr && m_transport->isOpen()) {
    if (m_transport->hasDeviceConfig()) {
      const auto &config = m_transport->deviceConfig();
      DeviceProfile profile;
      if (m_transport->getProfile(config.activeProfile, profile)) {
        m_scrollSpeed = profile.speed;
        m_scrollScaleProfile = profile.yScrollScale;
        m_scrollInvertProfile = (profile.invert != 0);
        LOG_INFO(
            "BridgeScreen: loaded profile %d: speed=%d, scale=%d, invert=%d", config.activeProfile, m_scrollSpeed,
            m_scrollScaleProfile, m_scrollInvertProfile
        );
      } else {
        LOG_WARN("BridgeScreen: failed to load profile %d", config.activeProfile);
      }
    }
  }
}

bool BridgePlatformScreen::canLeave()
{
  return true;
}

void BridgePlatformScreen::leave()
{
  LOG_DEBUG("BridgeScreen: leave");
  fakeAllKeysUp();
}

bool BridgePlatformScreen::setClipboard(ClipboardID id, const IClipboard *clipboard)
{
  LOG_DEBUG("BridgeScreen: clipboard discarded");
  return false;
}

void BridgePlatformScreen::checkClipboards()
{
}

void BridgePlatformScreen::openScreensaver(bool notify)
{
}

void BridgePlatformScreen::closeScreensaver()
{
}

void BridgePlatformScreen::screensaver(bool activate)
{
}

PlatformScreen::ScrollDelta BridgePlatformScreen::applyClientScrollModifier(const ScrollDelta rawDelta) const
{
  ScrollDelta correctedDelta = rawDelta;

  // Decoding scale: 0 -> 0.1, otherwise value / 10.0
  double scale = (m_scrollScaleProfile == 0) ? 0.1 : static_cast<double>(m_scrollScaleProfile) / 10.0;

  correctedDelta.yDelta *= m_scrollInvertProfile ? -scale : scale;

  return correctedDelta;
}

void BridgePlatformScreen::resetOptions()
{
}

void BridgePlatformScreen::setOptions(const OptionsList &options)
{
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
  return "";
}

void BridgePlatformScreen::updateButtons()
{
}

IKeyState *BridgePlatformScreen::getKeyState() const
{
  return nullptr;
}

void BridgePlatformScreen::handleSystemEvent(const Event &event)
{
}

bool BridgePlatformScreen::sendEvent(HidEventType type, const std::vector<uint8_t> &payload) const
{
  if (CLOG->getFilter() >= LogLevel::Debug) {
    std::string payloadHex = hexDump(payload.data(), payload.size(), 48);
    if (!payloadHex.empty()) {
      LOG_DEBUG(
          "BridgeScreen: TX HID type=0x%02x len=%zu payload=%s", static_cast<unsigned>(type), payload.size(),
          payloadHex.c_str()
      );
    } else {
      LOG_DEBUG("BridgeScreen: TX HID type=0x%02x len=%zu", static_cast<unsigned>(type), payload.size());
    }
  }

  HidEventPacket packet;
  packet.type = type;
  packet.payload = payload;
  if (!m_transport->sendHidEvent(packet)) {
    LOG_ERR("BridgeScreen: failed to send HID event type=%u", static_cast<unsigned>(type));
    m_events->addEvent(Event(EventTypes::ScreenError, getEventTarget()));
    return false;
  }

  return true;
}

bool BridgePlatformScreen::sendKeyboardEvent(HidEventType type, uint8_t modifiers, uint8_t keycode) const
{
  if (!sendEvent(type, {modifiers, keycode})) {
    LOG_ERR("BridgeScreen: failed to send keyboard event");
    return false;
  }
  return true;
}

bool BridgePlatformScreen::sendMouseMoveEvent(int16_t dx, int16_t dy) const
{
  uint8_t dxLo = static_cast<uint8_t>(dx & 0xFF);
  uint8_t dxHi = static_cast<uint8_t>((dx >> 8) & 0xFF);
  uint8_t dyLo = static_cast<uint8_t>(dy & 0xFF);
  uint8_t dyHi = static_cast<uint8_t>((dy >> 8) & 0xFF);

  if (!sendEvent(HidEventType::MouseMove, {dxLo, dxHi, dyLo, dyHi})) {
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

bool BridgePlatformScreen::sendConsumerControlEvent(HidEventType type, uint16_t usageCode) const
{
  const uint8_t lowByte = static_cast<uint8_t>(usageCode & 0xFF);
  const uint8_t highByte = static_cast<uint8_t>((usageCode >> 8) & 0xFF);

  if (!sendEvent(type, {lowByte, highByte})) {
    LOG_ERR("BridgeScreen: failed to send consumer control event");
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
#if defined(__APPLE__)
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
#elif defined(_WIN32)
  switch (button) {
  case 42: // Windows Shift_L (Scan Code 0x2A)
    return 0x02;
  case 54: // Windows Shift_R (Scan Code 0x36)
    return 0x20;
  case 29: // Windows Control_L (Scan Code 0x1D)
    return 0x01;
  // Note: Right Control is extended 0xE0 0x1D, often mapped to 29 or handled specially.
  // Standard Scan Code Set 1 for RCtrl is 0xE0 0x1D.
  case 56: // Windows Alt_L (Scan Code 0x38)
    return 0x04;
  // Windows Alt_R is extended 0xE0 0x38
  case 91: // Windows LWin (Scan Code 0x5B)
    return 0x08;
  case 92: // Windows RWin (Scan Code 0x5C)
    return 0x80;
  default:
    return 0;
  }
#else
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
#endif
}

uint16_t BridgePlatformScreen::convertMediaKeyToConsumerControl(KeyID id) const
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
    LOG_DEBUG2("BridgeScreen: unmapped KeyID 0x%04x", id);
    return 0;
  }
}

uint8_t BridgePlatformScreen::convertKeyButton(KeyButton button) const
{
#if defined(__APPLE__)
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
    LOG_DEBUG2("BridgeScreen: unmapped button code %u", button);
    return 0;
  }
#else
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
    LOG_DEBUG2("BridgeScreen: unmapped button code %u", button);
    return 0;
  }
#endif
}

uint8_t BridgePlatformScreen::convertKey(KeyID id, KeyButton button) const
{
  if (modifierBitForKey(id) != 0 || modifierBitForButton(button) != 0) {
    return 0;
  }

  if (uint8_t code = convertKeyID(id); code != 0) {
    return code;
  }
  return convertKeyButton(button);
}

uint8_t BridgePlatformScreen::activeModifierBitmap() const
{
  uint8_t mods = 0;
  for (const auto &entry : m_buttonToActiveKey) {
    mods |= entry.second.modifierBit;
  }
  return mods;
}

uint8_t BridgePlatformScreen::convertButtonID(ButtonID id) const
{
  return static_cast<uint8_t>(id);
}

void BridgePlatformScreen::handleKeepAliveTimer(const Event &event) const
{
  if (event.getTarget() != this) {
    return;
  }

  const auto *timerEvent = static_cast<IEventQueue::TimerEvent *>(event.getData());
  if (timerEvent == nullptr || timerEvent->m_timer != m_keepAliveTimer) {
    return;
  }

  sendKeepAliveIfIdle();
}

void BridgePlatformScreen::startKeepAliveTimer()
{
  if (m_events == nullptr || m_keepAliveTimer != nullptr) {
    return;
  }
  m_keepAliveTimer = m_events->newTimer(kKeepAliveIntervalSeconds, this);
}

void BridgePlatformScreen::stopKeepAliveTimer()
{
  if (m_events != nullptr && m_keepAliveTimer != nullptr) {
    m_events->deleteTimer(static_cast<EventQueueTimer *>(m_keepAliveTimer));
    m_keepAliveTimer = nullptr;
  }
}

void BridgePlatformScreen::sendKeepAliveIfIdle() const
{
  if (m_transport == nullptr) {
    return;
  }

  const auto now = std::chrono::steady_clock::now();
  if (m_lastCdcCommand != std::chrono::steady_clock::time_point::min() && now - m_lastCdcCommand < kKeepAliveInterval) {
    return;
  }

  uint32_t uptimeSeconds = 0;
  if (m_transport->sendKeepAlive(uptimeSeconds)) {
    LOG_INFO("BridgeScreen: keep-alive ack uptime=%us", static_cast<unsigned>(uptimeSeconds));
    recordCdcCommand(now);
  } else {
    LOG_WARN("BridgeScreen: keep-alive failed (%s)", m_transport->lastError().c_str());
    m_events->addEvent(Event(EventTypes::ScreenError, getEventTarget()));
  }
}

void BridgePlatformScreen::recordCdcCommand(std::chrono::steady_clock::time_point now) const
{
  m_lastCdcCommand = now;
}

} // namespace deskflow::bridge
