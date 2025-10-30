/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "CdcTransport.h"
#include "deskflow/PlatformScreen.h"

#include <set>
#include <memory>

namespace deskflow::bridge {

/**
 * @brief Bridge platform screen implementation
 *
 * This screen implementation converts Deskflow input events (mouse/keyboard)
 * into HID reports and sends them over USB CDC to the Pico 2 W bridge device.
 *
 * Key differences from standard platform screens:
 * - No local event injection on the PC
 * - Screen dimensions come from Pico configuration (mobile device screen)
 * - Clipboard operations are discarded
 * - Input events are converted to HID reports and sent via CDC
 */
class BridgePlatformScreen : public PlatformScreen
{
public:
  BridgePlatformScreen(IEventQueue *events, std::shared_ptr<CdcTransport> transport, const PicoConfig &config);
  ~BridgePlatformScreen() override;

  // IScreen overrides
  void *getEventTarget() const override;
  bool getClipboard(ClipboardID id, IClipboard *clipboard) const override;
  void getShape(int32_t &x, int32_t &y, int32_t &width, int32_t &height) const override;
  void getCursorPos(int32_t &x, int32_t &y) const override;

  // IPrimaryScreen overrides
  void reconfigure(uint32_t activeSides) override;
  uint32_t activeSides() override;
  void warpCursor(int32_t x, int32_t y) override;
  uint32_t registerHotKey(KeyID key, KeyModifierMask mask) override;
  void unregisterHotKey(uint32_t id) override;
  void fakeInputBegin() override;
  void fakeInputEnd() override;
  int32_t getJumpZoneSize() const override;
  bool isAnyMouseButtonDown(uint32_t &buttonID) const override;
  void getCursorCenter(int32_t &x, int32_t &y) const override;

  // ISecondaryScreen overrides
  void fakeMouseButton(ButtonID id, bool press) override;
  void fakeMouseMove(int32_t x, int32_t y) override;
  void fakeMouseRelativeMove(int32_t dx, int32_t dy) const override;
  void fakeMouseWheel(int32_t xDelta, int32_t yDelta) const override;

  // IKeyState overrides
  void fakeKeyDown(KeyID id, KeyModifierMask mask, KeyButton button, const std::string &lang) override;
  bool fakeKeyRepeat(
      KeyID id, KeyModifierMask mask, int32_t count, KeyButton button, const std::string &lang
  ) override;
  bool fakeKeyUp(KeyButton button) override;
  void fakeAllKeysUp() override;
  void updateKeyMap() override;
  void updateKeyState() override;
  void setHalfDuplexMask(KeyModifierMask) override;
  bool fakeCtrlAltDel() override;
  bool isKeyDown(KeyButton button) const override;
  KeyModifierMask getActiveModifiers() const override;
  KeyModifierMask pollActiveModifiers() const override;
  int32_t pollActiveGroup() const override;
  void pollPressedKeys(IKeyState::KeyButtonSet &pressedKeys) const override;

  // IPlatformScreen overrides
  void enable() override;
  void disable() override;
  void enter() override;
  bool canLeave() override;
  void leave() override;
  bool setClipboard(ClipboardID id, const IClipboard *clipboard) override;
  void checkClipboards() override;
  void openScreensaver(bool notify) override;
  void closeScreensaver() override;
  void screensaver(bool activate) override;
  void resetOptions() override;
  void setOptions(const OptionsList &options) override;
  void setSequenceNumber(uint32_t seqNum) override;
  bool isPrimary() const override;
  std::string getSecureInputApp() const override;

protected:
  void updateButtons() override;
  IKeyState *getKeyState() const override;
  void handleSystemEvent(const Event &event) override;

private:
  /**
   * @brief Send current keyboard state as HID report
   */
  void sendKeyboardReport();

  /**
   * @brief Send mouse button/position update as HID report
   */
  void sendMouseReport();

  /**
   * @brief Map Deskflow KeyID to HID keycode
   */
  uint8_t mapKeyToHid(KeyID key, KeyButton button) const;

  /**
   * @brief Map Deskflow modifier mask to HID modifier bitmap
   */
  uint8_t mapModifiersToHid(KeyModifierMask mask) const;

  std::shared_ptr<CdcTransport> m_transport;
  PicoConfig m_config;

  // Screen state
  int32_t m_cursorX = 0;
  int32_t m_cursorY = 0;
  uint8_t m_mouseButtons = 0; // HID button bitmap
  uint8_t m_keyModifiers = 0; // HID modifier bitmap
  uint8_t m_pressedKeys[6] = {0}; // Up to 6 simultaneous keys (HID boot protocol)
  size_t m_numPressedKeys = 0;

  std::set<KeyButton> m_pressedButtons;
  KeyModifierMask m_activeModifiers = 0;

  bool m_enabled = false;
  uint32_t m_sequenceNumber = 0;
};

} // namespace deskflow::bridge
