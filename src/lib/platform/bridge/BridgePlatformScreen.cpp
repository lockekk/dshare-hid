/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "BridgePlatformScreen.h"

#include "HidFrame.h"
#include "base/Event.h"
#include "base/EventTypes.h"
#include "base/Log.h"
#include "common/LogLevel.h"
#include "common/Settings.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
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
    IEventQueue *events, std::shared_ptr<CdcTransport> transport, std::shared_ptr<requests::Queue> requests,
    int32_t screenWidth, int32_t screenHeight
)
    : PlatformComputer(events),
      m_transport(std::move(transport)),
      m_requests(std::move(requests)),
      m_screenWidth(screenWidth),
      m_screenHeight(screenHeight),
      m_events(events),
      m_keyboard(
          BridgeKeyboard::kThisComputer,
          [this](HidEventType type, uint8_t modifiers, uint8_t key) { return sendKeyboardEvent(type, modifiers, key); },
          [this](HidEventType type, uint16_t usage) { return sendConsumerControlEvent(type, usage); },
#if defined(_WIN32)
          windowsDeadKey
#else
          nullptr
#endif
      )
{
  LOG_INFO("BridgeScreen: initialized screen=%dx%d", m_screenWidth, m_screenHeight);
  // the size given is what the GUI read before starting the core, or its default when that
  // read failed: the device's active profile decides.  nothing is connected yet to be told
  if (m_transport != nullptr) {
    loadActiveProfile();
  }

  m_lastCdcCommand = std::chrono::steady_clock::time_point::min();

  if (m_events != nullptr) {
    m_events->addHandler(deskflow::EventTypes::StreamInputReady, m_requests.get(), [this](const Event &) {
      handleRequests();
    });
  }

  m_bluetoothKeepAliveEnabled = Settings::value(Settings::Bridge::BluetoothKeepAlive).toBool();
  if (m_bluetoothKeepAliveEnabled && m_events != nullptr) {
    LOG_INFO("BridgeScreen: Bluetooth keep-alive enabled, starting timer");
    m_events->addHandler(deskflow::EventTypes::Timer, this, [this](const Event &event) {
      handleKeepAliveTimer(event);
    });
    startKeepAliveTimer();
    // the firmware drops Bluetooth once no CDC frame has arrived for 90 s, but only after
    // the first keep-alive has armed that check.  arm it now; afterwards any frame (HID
    // events included) keeps it fed, so keep-alives are only sent when idle.
    sendKeepAliveIfIdle();
  }
}

BridgePlatformScreen::~BridgePlatformScreen()
{
  stopKeepAliveTimer();
  if (m_events != nullptr) {
    m_events->removeHandler(deskflow::EventTypes::Timer, this);
    m_events->removeHandler(deskflow::EventTypes::StreamInputReady, m_requests.get());
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
  LOG_VERBOSE("BridgeScreen: mouse move to %d,%d", x, y);

  int32_t dx = x - m_cursorX;
  int32_t dy = y - m_cursorY;

  m_cursorX = x;
  m_cursorY = y;

  fakeMouseRelativeMove(dx, dy);
}

void BridgePlatformScreen::fakeMouseRelativeMove(int32_t dx, int32_t dy) const
{
  LOG_VERBOSE("BridgeScreen: mouse relative move %d,%d", dx, dy);

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

void BridgePlatformScreen::fakeMouseWheel(ScrollDelta delta) const
{
  if (delta.y == 0) {
    return;
  }

  // the server sends 120 per wheel notch.  apply the profile's speed before its scale so
  // small (trackpad) deltas at low scales aren't rounded away.
  int32_t speed = (m_scrollSpeed > 0) ? m_scrollSpeed : 120;
  ScrollDelta spedDelta = delta;
  spedDelta.y = delta.y * speed;
  m_wheelAccumulatorY += applyClientScrollModifier(spedDelta).y;

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
  m_keyboard.keyDown(id, mask, button);
}

bool BridgePlatformScreen::fakeKeyRepeat(
    KeyID id, KeyModifierMask mask, int32_t count, KeyButton button, const std::string &
)
{
  return m_keyboard.keyRepeat(id, mask, count, button);
}

bool BridgePlatformScreen::fakeKeyUp(KeyButton button)
{
  m_keyboard.keyUp(button);
  return true;
}

void BridgePlatformScreen::fakeAllKeysUp()
{
  m_keyboard.allKeysUp();
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
  return m_keyboard.isKeyDown(button);
}

KeyModifierMask BridgePlatformScreen::getActiveModifiers() const
{
  return m_keyboard.activeModifiers();
}

KeyModifierMask BridgePlatformScreen::pollActiveModifiers() const
{
  return m_keyboard.activeModifiers();
}

int32_t BridgePlatformScreen::pollActiveGroup() const
{
  return 0;
}

void BridgePlatformScreen::pollPressedKeys(IKeyState::KeyButtonSet &pressedKeys) const
{
  const auto &buttons = m_keyboard.pressedButtons();
  pressedKeys.insert(buttons.begin(), buttons.end());
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
  LOG_PRINT("%s", qPrintable(requests::formatFocus(true)));
  resetMouseAccumulator();
  if (m_transport != nullptr && !m_transport->open()) {
    LOG_WARN("BridgeScreen: failed to open transport on enter (%s)", m_transport->lastError().c_str());
  }

  if (m_transport != nullptr && m_transport->isOpen()) {
    if (m_transport->hasDeviceConfig()) {
      const auto &config = m_transport->deviceConfig();
      DeviceProfile profile;
      if (m_transport->getProfile(config.activeProfile, profile)) {
        applyProfileSettings(config.activeProfile, profile);
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
  LOG_PRINT("%s", qPrintable(requests::formatFocus(false)));
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

ScrollDelta BridgePlatformScreen::applyClientScrollModifier(const ScrollDelta rawDelta) const
{
  ScrollDelta correctedDelta = rawDelta;

  // the profile stores the scale in tenths (1-100 = 0.1-10.0).  0 is the firmware's
  // default, "let the host decide", which is 1.0 like the client's own yScrollScale.
  double scale = (m_scrollScaleProfile == 0) ? 1.0 : static_cast<double>(m_scrollScaleProfile) / 10.0;

  correctedDelta.y =
      static_cast<int32_t>(std::lround(m_scrollInvertProfile ? rawDelta.y * -scale : rawDelta.y * scale));

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
  if (CLOG->getFilter() >= LogLevel::Level::Debug) {
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
    m_events->addEvent(Event(EventTypes::ComputerError, getEventTarget()));
    return false;
  }

  recordCdcCommand();
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
    m_events->addEvent(Event(EventTypes::ComputerError, getEventTarget()));
  }
}

void BridgePlatformScreen::recordCdcCommand(std::chrono::steady_clock::time_point now) const
{
  m_lastCdcCommand = now;
}

void BridgePlatformScreen::handleRequests()
{
  for (const auto &line : m_requests->takeAll()) {
    requests::Request request;
    if (!requests::parseRequest(QString::fromStdString(line), request)) {
      LOG_WARN("BridgeScreen: ignoring malformed request: %s", line.c_str());
      continue;
    }

    QStringList fields;
    QString error;
    const bool ok = requests::execute(*m_transport, request.command, request.arguments, fields, error);
    if (ok) {
      recordCdcCommand(); // the firmware counts any frame as Bluetooth keep-alive traffic
    } else {
      LOG_WARN("BridgeScreen: request '%s' failed: %s", qPrintable(request.command), qPrintable(error));
      fields = {error};
    }
    // even a failed request may have changed the device, e.g. a switch whose reply timed out, or
    // when only reading it back failed
    if (requests::changesActiveProfile(request.command)) {
      applyActiveProfile();
    }
    // print level: never filtered or prefixed, so the GUI always gets the reply as is
    LOG_PRINT("%s", qPrintable(requests::formatResponse(request.id, ok, fields)));
  }
}

void BridgePlatformScreen::applyActiveProfile()
{
  if (loadActiveProfile()) {
    // the client sends the server the new size, so it stays connected through the switch
    m_events->addEvent(Event(EventTypes::ComputerShapeChanged, getEventTarget()));
  }
}

bool BridgePlatformScreen::loadActiveProfile()
{
  // ask the device which profile is active rather than trust deviceConfig(): a switch whose
  // reply timed out may still have happened
  uint8_t index = 0;
  DeviceProfile profile;
  if (!m_transport->getActiveProfile(index, profile)) {
    LOG_WARN("BridgeScreen: failed to load the active profile");
    return false;
  }
  applyProfileSettings(index, profile);

  int32_t width = 0;
  int32_t height = 0;
  profileScreenSize(profile, width, height);
  if (width == m_screenWidth && height == m_screenHeight) {
    return false;
  }
  // the protocol carries the size as int16, and the server drops a client whose size isn't positive
  if (width <= 0 || height <= 0 || width > INT16_MAX || height > INT16_MAX) {
    LOG_WARN("BridgeScreen: ignoring profile %d's screen size %dx%d", index, width, height);
    return false;
  }

  LOG_INFO("BridgeScreen: profile %d screen=%dx%d", index, width, height);
  m_screenWidth = width;
  m_screenHeight = height;
  getCursorCenter(m_cursorX, m_cursorY);
  return true;
}

void BridgePlatformScreen::applyProfileSettings(uint8_t index, const DeviceProfile &profile)
{
  m_scrollSpeed = profile.speed;
  m_scrollScaleProfile = profile.yScrollScale;
  m_scrollInvertProfile = (profile.invert != 0);
  m_keyboard.setLayout(keyboardLayout(profile.keyboardLayout));
  LOG_INFO(
      "BridgeScreen: loaded profile %d: speed=%d, scale=%d, invert=%d, keys as pressed=%d", index, m_scrollSpeed,
      m_scrollScaleProfile, m_scrollInvertProfile, m_keyboard.sendsKeysAsPressed()
  );
}

} // namespace deskflow::bridge
