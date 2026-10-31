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
  void unknownLayoutIsUs();

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
  void germanControlAltShortcut();
  void germanControlShortcut();
  void germanDeadKeys();
  void keyRepeatKeepsAltGr();
  void allKeysUpReleasesEverything();
  void mediaKeysUseConsumerControl();
  void onlyWindowsSendsKeysAsPressed();
  void layoutChangeTakesEffect();

private:
  Log m_log;
};
