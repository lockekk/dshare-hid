/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "base/Log.h"

#include <QTest>

class BridgeKeyboardTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  // the tables and the modifier rules (platform/bridge/BridgeKeys.h)
  void scanCodesAreKeyPositions_data();
  void scanCodesAreKeyPositions();
  void maskModifiersKeepTheHeldSide();
  void heldModifiersTurnControlAltIntoAltGr();
  void deadKeyCharacters_data();
  void deadKeyCharacters();
  void findDeadKey_data();
  void findDeadKey();
  void windowsLayoutsDeadKeys_data();
  void windowsLayoutsDeadKeys();
  void unknownLayoutIsUs();

  // every key, by its key code and by its key ID, for each kind of computer and both layouts
  void everyMacKeyCode();
  void everyLinuxKeyCode();
  void everyWindowsKeyCode();
  void modifierKeysById_data();
  void modifierKeysById();
  void keysById_data();
  void keysById();
  void keypadKeys_data();
  void keypadKeys();
  void mediaKeys_data();
  void mediaKeys();

  // the keys held, and each report's modifiers
  void stateFollowsTheKeys();
  void twoKeysHeld();
  void keyUpOfKeyNeverPressed_data();
  void keyUpOfKeyNeverPressed();
  void modifiersCombine();
  void macRightCommandIsRightGui();
  void linuxRightShiftStaysRight();
  void keyRepeat();
  void mediaKeyRepeat();
  void allKeysUpReleasesInOrder();
  void failedSends();

  // US, with Windows on US: the customer's workaround
  void usAltGrSymbols_data();
  void usAltGrSymbols();
  void usLeftAltStaysLeft();
  void usRightShiftStaysRight();
  void usKeystrokeWithoutKeys();
  void usIsoKeyIsBackslash();
  void usNumpadKeys();
  void usTypesCharacters();
  void macRightOptionIsRightAlt();

  // the same as this computer, with Windows on German
  void germanKeys_data();
  void germanKeys();
  void germanShiftedKeys();
  void germanAltGrSymbols_data();
  void germanAltGrSymbols();
  void germanAltGrTwice();
  void rightControlAndAltAreAltGr();
  void germanControlAltShortcut();
  void altShortcutWithControlAndAltHeld();
  void germanControlShortcut();
  void keystrokeWithoutKeys();
  void keypadNavigation();
  void germanDeadKeys();
  void usInternationalDeadKeys();
  void keyRepeatKeepsAltGr();
  void allKeysUpReleasesEverything();
  void mediaKeysUseConsumerControl();
  void onlyWindowsSendsKeysAsPressed();
  void layoutChangeTakesEffect();

private:
  Log m_log;
};
