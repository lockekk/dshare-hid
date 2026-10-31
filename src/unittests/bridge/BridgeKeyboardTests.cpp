/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "BridgeKeyboardTests.h"

#include "platform/bridge/BridgeKeyboard.h"

#include <utility>
#include <vector>

using namespace deskflow::bridge;
using KeyCodes = BridgeKeyboard::KeyCodes;

namespace {

// the Windows scan codes of the modifier keys the tests press
constexpr KeyButton kLeftShift = 0x2A;
constexpr KeyButton kRightShift = 0x36;
constexpr KeyButton kLeftControl = 0x1D;
constexpr KeyButton kLeftAlt = 0x38;
constexpr KeyButton kRightAlt = 0x138;

// a report as "press 40 14": a press with the modifier byte 0x40 (Right Alt) and the key 0x14 (q)
QString report(const char *type, unsigned modifiers, unsigned key)
{
  return QStringLiteral("%1 %2 %3")
      .arg(QLatin1String(type))
      .arg(modifiers, 2, 16, QLatin1Char('0'))
      .arg(key, 2, 16, QLatin1Char('0'));
}

QStringList pressAndRelease(unsigned modifiers, unsigned key)
{
  return {report("press", modifiers, key), report("release", modifiers, key)};
}

// what a keyboard sends the dongle
struct Dongle
{
  QStringList reports;
  QStringList consumer;
};

BridgeKeyboard
makeKeyboard(Dongle &dongle, KeyCodes keyCodes, KeyboardLayout layout, BridgeKeyboard::DeadKeyFinder deadKeys = nullptr)
{
  BridgeKeyboard keyboard(
      keyCodes,
      [&dongle](HidEventType type, uint8_t modifiers, uint8_t key) {
        dongle.reports.append(report(type == HidEventType::KeyboardPress ? "press" : "release", modifiers, key));
        return true;
      },
      [&dongle](HidEventType type, uint16_t usage) {
        dongle.consumer.append(QStringLiteral("%1 %2")
                                   .arg(type == HidEventType::ConsumerControlPress ? "press" : "release")
                                   .arg(usage, 4, 16, QLatin1Char('0')));
        return true;
      },
      std::move(deadKeys)
  );
  keyboard.setLayout(layout);
  return keyboard;
}

// Windows' German (Germany) layout, as windowsDeadKey() reads it from Windows: ^ is a dead key,
// and so are ´ and, with Shift, ` on the key left of Backspace
bool germanDeadKey(KeyID id, uint16_t scanCode, uint16_t &deadScanCode, uint8_t &deadModifiers)
{
  struct DeadKey
  {
    char16_t accent;
    uint16_t scanCode;
    uint8_t modifiers;
  };
  static const DeadKey deadKeys[] = {{u'^', 0x29, 0x00}, {0x00B4, 0x0D, 0x00}, {u'`', 0x0D, 0x02}};

  for (const auto &dead : deadKeys) {
    // the key typed its own accent, as the hook sends a dead key pressed twice
    if (id == dead.accent && scanCode == dead.scanCode) {
      return false;
    }
  }
  for (const char16_t accent : deadKeyCharacters(id)) {
    for (const auto &dead : deadKeys) {
      if (accent == dead.accent) {
        deadScanCode = dead.scanCode;
        deadModifiers = dead.modifiers;
        return true;
      }
    }
  }
  return false;
}

// Windows on German, and the device set to German too
BridgeKeyboard germanKeyboard(Dongle &dongle)
{
  return makeKeyboard(dongle, KeyCodes::Windows, KeyboardLayout::SameAsComputer, germanDeadKey);
}

void tap(BridgeKeyboard &keyboard, KeyID id, KeyModifierMask mask, KeyButton button)
{
  keyboard.keyDown(id, mask, button);
  keyboard.keyUp(button);
}

// AltGr as Windows reports it on a layout with AltGr: Left Control, then Right Alt
void pressAltGr(BridgeKeyboard &keyboard)
{
  keyboard.keyDown(kKeyControl_L, KeyModifierControl, kLeftControl);
  keyboard.keyDown(kKeyAltGr, KeyModifierControl | KeyModifierAlt, kRightAlt);
}

void releaseAltGr(BridgeKeyboard &keyboard)
{
  keyboard.keyUp(kRightAlt);
  keyboard.keyUp(kLeftControl);
}

QString characters(const std::vector<char16_t> &list)
{
  QString text;
  for (const char16_t c : list) {
    text.append(QChar(c));
  }
  return text;
}

} // namespace

void BridgeKeyboardTests::scanCodesAreKeyPositions_data()
{
  QTest::addColumn<int>("scanCode");
  QTest::addColumn<int>("usage");

  // the letters, row by row
  const char *rows[] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
  const int firstScanCodes[] = {0x10, 0x1E, 0x2C};
  for (int row = 0; row < 3; ++row) {
    for (int i = 0; rows[row][i] != 0; ++i) {
      const char letter = rows[row][i];
      QTest::addRow("%c", letter) << firstScanCodes[row] + i << 0x04 + (letter - 'a');
    }
  }

  // on a German keyboard in brackets
  QTest::newRow("Esc") << 0x01 << 0x29;
  QTest::newRow("1") << 0x02 << 0x1E;
  QTest::newRow("9") << 0x0A << 0x26;
  QTest::newRow("0") << 0x0B << 0x27;
  QTest::newRow("- (ß)") << 0x0C << 0x2D;
  QTest::newRow("= (´)") << 0x0D << 0x2E;
  QTest::newRow("Backspace") << 0x0E << 0x2A;
  QTest::newRow("Tab") << 0x0F << 0x2B;
  QTest::newRow("[ (ü)") << 0x1A << 0x2F;
  QTest::newRow("] (+)") << 0x1B << 0x30;
  QTest::newRow("Enter") << 0x1C << 0x28;
  QTest::newRow("; (ö)") << 0x27 << 0x33;
  QTest::newRow("' (ä)") << 0x28 << 0x34;
  QTest::newRow("` (^)") << 0x29 << 0x35;
  QTest::newRow("\\ (#)") << 0x2B << 0x31;
  QTest::newRow(", (,)") << 0x33 << 0x36;
  QTest::newRow(". (.)") << 0x34 << 0x37;
  QTest::newRow("/ (-)") << 0x35 << 0x38;
  QTest::newRow("ISO key (<)") << 0x56 << 0x64;
  QTest::newRow("Space") << 0x39 << 0x2C;
  QTest::newRow("Caps Lock") << 0x3A << 0x39;
  QTest::newRow("F1") << 0x3B << 0x3A;
  QTest::newRow("F10") << 0x44 << 0x43;
  QTest::newRow("F11") << 0x57 << 0x44;
  QTest::newRow("F12") << 0x58 << 0x45;
  QTest::newRow("F13") << 0x64 << 0x68;
  QTest::newRow("F23") << 0x6E << 0x72;
  QTest::newRow("F24") << 0x76 << 0x73;
  QTest::newRow("keypad 7") << 0x47 << 0x5F;
  QTest::newRow("keypad 5") << 0x4C << 0x5D;
  QTest::newRow("keypad 1") << 0x4F << 0x59;
  QTest::newRow("keypad 0") << 0x52 << 0x62;
  QTest::newRow("keypad .") << 0x53 << 0x63;
  QTest::newRow("keypad *") << 0x37 << 0x55;
  QTest::newRow("keypad -") << 0x4A << 0x56;
  QTest::newRow("keypad +") << 0x4E << 0x57;
  QTest::newRow("keypad /") << 0x135 << 0x54;
  QTest::newRow("keypad Enter") << 0x11C << 0x58;
  QTest::newRow("Num Lock") << 0x145 << 0x53;
  QTest::newRow("Pause") << 0x45 << 0x48;
  QTest::newRow("Break") << 0x146 << 0x48;
  QTest::newRow("Scroll Lock") << 0x46 << 0x47;
  QTest::newRow("Print Screen") << 0x137 << 0x46;
  QTest::newRow("SysRq") << 0x54 << 0x46;
  QTest::newRow("Insert") << 0x152 << 0x49;
  QTest::newRow("Delete") << 0x153 << 0x4C;
  QTest::newRow("Home") << 0x147 << 0x4A;
  QTest::newRow("End") << 0x14F << 0x4D;
  QTest::newRow("Page Up") << 0x149 << 0x4B;
  QTest::newRow("Page Down") << 0x151 << 0x4E;
  QTest::newRow("Up") << 0x148 << 0x52;
  QTest::newRow("Down") << 0x150 << 0x51;
  QTest::newRow("Left") << 0x14B << 0x50;
  QTest::newRow("Right") << 0x14D << 0x4F;
  QTest::newRow("Menu") << 0x15D << 0x65;
  QTest::newRow("Ro") << 0x73 << 0x87;
  QTest::newRow("Yen") << 0x7D << 0x89;

  // the modifiers go in the modifier byte
  QTest::newRow("Left Shift") << 0x2A << 0;
  QTest::newRow("Right Shift") << 0x36 << 0;
  QTest::newRow("Left Control") << 0x1D << 0;
  QTest::newRow("Left Alt") << 0x38 << 0;
  QTest::newRow("Right Alt") << 0x138 << 0;
  QTest::newRow("Left Windows") << 0x15B << 0;
  QTest::newRow("unknown") << 0x1FF << 0;
}

void BridgeKeyboardTests::scanCodesAreKeyPositions()
{
  QFETCH(int, scanCode);
  QFETCH(int, usage);
  QCOMPARE(int(windowsScanCodeToHid(static_cast<uint16_t>(scanCode))), usage);
}

void BridgeKeyboardTests::maskModifiersKeepTheHeldSide()
{
  QCOMPARE(int(maskHidModifiers(KeyModifierAlt, 0x40)), 0x40);
  QCOMPARE(int(maskHidModifiers(KeyModifierAlt, 0x04)), 0x04);
  QCOMPARE(int(maskHidModifiers(KeyModifierAlt, 0x00)), 0x04);
  QCOMPARE(int(maskHidModifiers(KeyModifierShift, 0x20)), 0x20);
  QCOMPARE(int(maskHidModifiers(KeyModifierShift | KeyModifierControl, 0x21)), 0x21);
  QCOMPARE(int(maskHidModifiers(KeyModifierSuper, 0x80)), 0x80);
  QCOMPARE(int(maskHidModifiers(KeyModifierAltGr, 0x00)), 0x40);
  // held, but the mask leaves it out
  QCOMPARE(int(maskHidModifiers(0, 0x41)), 0x00);
}

void BridgeKeyboardTests::heldModifiersTurnControlAltIntoAltGr()
{
  // Windows' AltGr, and Control+Alt typing as AltGr
  QCOMPARE(int(heldHidModifiers(0, 0x41)), 0x40);
  QCOMPARE(int(heldHidModifiers(0, 0x05)), 0x40);
  QCOMPARE(int(heldHidModifiers(KeyModifierShift, 0x43)), 0x42);
  // a shortcut: the mask keeps Control and Alt
  QCOMPARE(int(heldHidModifiers(KeyModifierControl | KeyModifierAlt, 0x41)), 0x41);
  // not both
  QCOMPARE(int(heldHidModifiers(KeyModifierAlt, 0x40)), 0x40);
  QCOMPARE(int(heldHidModifiers(0, 0x01)), 0x01);
}

void BridgeKeyboardTests::deadKeyCharacters_data()
{
  QTest::addColumn<int>("id");
  QTest::addColumn<QString>("characters");

  QTest::newRow("ê") << 0xEA << QStringLiteral("^ˆ");
  QTest::newRow("Ê") << 0xCA << QStringLiteral("^ˆ");
  QTest::newRow("é") << 0xE9 << QStringLiteral("´'");
  QTest::newRow("è") << 0xE8 << QStringLiteral("`");
  QTest::newRow("à") << 0xE0 << QStringLiteral("`");
  QTest::newRow("ñ") << 0xF1 << QStringLiteral("~˜");
  QTest::newRow("ü") << 0xFC << QStringLiteral("¨\"");
  QTest::newRow("ç") << 0xE7 << QStringLiteral("¸");
  QTest::newRow("å") << 0xE5 << QStringLiteral("°˚");
  QTest::newRow("ž") << 0x17E << QStringLiteral("ˇ");
  QTest::newRow("^ itself") << int('^') << QStringLiteral("^");
  QTest::newRow("´ itself") << 0xB4 << QStringLiteral("´");
  QTest::newRow("~ itself") << int('~') << QStringLiteral("~");
  QTest::newRow("e") << int('e') << QString();
  QTest::newRow("ß") << 0xDF << QString();
  QTest::newRow("€") << 0x20AC << QString();
  QTest::newRow("µ") << 0xB5 << QString();
  QTest::newRow("²") << 0xB2 << QString();
  QTest::newRow("a key without a character") << int(kKeyDelete) << QString();
  QTest::newRow("beyond 16 bits") << 0x1F600 << QString();
}

void BridgeKeyboardTests::deadKeyCharacters()
{
  QFETCH(int, id);
  QFETCH(QString, characters);
  QCOMPARE(::characters(deskflow::bridge::deadKeyCharacters(static_cast<KeyID>(id))), characters);
}

void BridgeKeyboardTests::unknownLayoutIsUs()
{
  QVERIFY(keyboardLayout(0) == KeyboardLayout::Us);
  QVERIFY(keyboardLayout(1) == KeyboardLayout::SameAsComputer);
  QVERIFY(keyboardLayout(2) == KeyboardLayout::Us);
  QVERIFY(keyboardLayout(255) == KeyboardLayout::Us);
}

void BridgeKeyboardTests::usAltGrSymbols_data()
{
  // Windows on US: the key as the US layout reads it, and the key the device needs
  QTest::addColumn<int>("id");
  QTest::addColumn<int>("scanCode");
  QTest::addColumn<int>("usage");

  QTest::newRow("@ (Q)") << int('q') << 0x10 << 0x14;
  QTest::newRow("€ (E)") << int('e') << 0x12 << 0x08;
  QTest::newRow("{ (7)") << int('7') << 0x08 << 0x24;
  QTest::newRow("[ (8)") << int('8') << 0x09 << 0x25;
  QTest::newRow("] (9)") << int('9') << 0x0A << 0x26;
  QTest::newRow("} (0)") << int('0') << 0x0B << 0x27;
  QTest::newRow("\\ (ß)") << int('-') << 0x0C << 0x2D;
  QTest::newRow("~ (+)") << int(']') << 0x1B << 0x30;
  QTest::newRow("² (2)") << int('2') << 0x03 << 0x1F;
  QTest::newRow("³ (3)") << int('3') << 0x04 << 0x20;
  QTest::newRow("µ (M)") << int('m') << 0x32 << 0x10;
}

void BridgeKeyboardTests::usAltGrSymbols()
{
  QFETCH(int, id);
  QFETCH(int, scanCode);
  QFETCH(int, usage);

  Dongle dongle;
  auto keyboard = makeKeyboard(dongle, KeyCodes::Windows, KeyboardLayout::Us);
  // the US layout has no AltGr: the key is Right Alt, as Alt
  keyboard.keyDown(kKeyAltGr, KeyModifierAlt, kRightAlt);
  tap(keyboard, static_cast<KeyID>(id), KeyModifierAlt, static_cast<KeyButton>(scanCode));
  keyboard.keyUp(kRightAlt);

  // Right Alt throughout, which the device's German layout reads as AltGr
  QCOMPARE(
      dongle.reports, QStringList(
                          {report("press", 0x40, 0), report("press", 0x40, usage), report("release", 0x40, usage),
                           report("release", 0, 0)}
                      )
  );
}

void BridgeKeyboardTests::usLeftAltStaysLeft()
{
  Dongle dongle;
  auto keyboard = makeKeyboard(dongle, KeyCodes::Windows, KeyboardLayout::Us);
  keyboard.keyDown(kKeyAlt_L, KeyModifierAlt, kLeftAlt);
  tap(keyboard, 'f', KeyModifierAlt, 0x21);
  keyboard.keyUp(kLeftAlt);

  QCOMPARE(
      dongle.reports, QStringList(
                          {report("press", 0x04, 0), report("press", 0x04, 0x09), report("release", 0x04, 0x09),
                           report("release", 0, 0)}
                      )
  );
}

void BridgeKeyboardTests::usRightShiftStaysRight()
{
  Dongle dongle;
  auto keyboard = makeKeyboard(dongle, KeyCodes::Windows, KeyboardLayout::Us);
  keyboard.keyDown(kKeyShift_R, KeyModifierShift, kRightShift);
  tap(keyboard, 'A', KeyModifierShift, 0x1E);
  keyboard.keyUp(kRightShift);

  QCOMPARE(
      dongle.reports, QStringList(
                          {report("press", 0x20, 0), report("press", 0x20, 0x04), report("release", 0x20, 0x04),
                           report("release", 0, 0)}
                      )
  );
}

void BridgeKeyboardTests::usKeystrokeWithoutKeys()
{
  // a keystroke the server makes, with no modifier key held and no key code
  Dongle dongle;
  auto keyboard = makeKeyboard(dongle, KeyCodes::Windows, KeyboardLayout::Us);
  tap(keyboard, 'c', KeyModifierControl, 0);

  QCOMPARE(dongle.reports, QStringList({report("press", 0x01, 0x06), report("release", 0, 0x06)}));
}

void BridgeKeyboardTests::usIsoKeyIsBackslash()
{
  // Windows on US reads the key beside Left Shift as \, which is # on a German device
  Dongle dongle;
  auto keyboard = makeKeyboard(dongle, KeyCodes::Windows, KeyboardLayout::Us);
  tap(keyboard, '\\', 0, 0x56);

  QCOMPARE(dongle.reports, pressAndRelease(0, 0x31));
}

void BridgeKeyboardTests::usNumpadKeys()
{
  // they have no character, so they go by their scan codes
  Dongle dongle;
  auto keyboard = makeKeyboard(dongle, KeyCodes::Windows, KeyboardLayout::Us);
  tap(keyboard, kKeyKP_1, 0, 0x4F);
  tap(keyboard, kKeyKP_Enter, 0, 0x11C);

  QCOMPARE(dongle.reports, pressAndRelease(0, 0x59) + pressAndRelease(0, 0x58));
}

void BridgeKeyboardTests::usTypesCharacters()
{
  // Windows on German, the device on US: the z goes where a US keyboard has it, which is y on a
  // German device, so this customer needs the device's layout to be the same as this computer
  Dongle dongle;
  auto keyboard = makeKeyboard(dongle, KeyCodes::Windows, KeyboardLayout::Us);
  tap(keyboard, 'z', 0, 0x15);

  QCOMPARE(dongle.reports, pressAndRelease(0, 0x1D));
}

void BridgeKeyboardTests::macRightOptionIsRightAlt()
{
  Dongle dongle;
  auto keyboard = makeKeyboard(dongle, KeyCodes::MacOS, KeyboardLayout::Us);
  keyboard.keyDown(kKeyAlt_R, KeyModifierAlt, 62);
  tap(keyboard, 'e', KeyModifierAlt, 15);
  keyboard.keyUp(62);

  QCOMPARE(
      dongle.reports, QStringList(
                          {report("press", 0x40, 0), report("press", 0x40, 0x08), report("release", 0x40, 0x08),
                           report("release", 0, 0)}
                      )
  );
}

void BridgeKeyboardTests::germanKeys_data()
{
  // Windows on German: the character it reads, and the key the device needs
  QTest::addColumn<int>("id");
  QTest::addColumn<int>("scanCode");
  QTest::addColumn<int>("usage");

  QTest::newRow("z") << int('z') << 0x15 << 0x1C;
  QTest::newRow("y") << int('y') << 0x2C << 0x1D;
  QTest::newRow("ü") << 0xFC << 0x1A << 0x2F;
  QTest::newRow("+") << int('+') << 0x1B << 0x30;
  QTest::newRow("ö") << 0xF6 << 0x27 << 0x33;
  QTest::newRow("ä") << 0xE4 << 0x28 << 0x34;
  QTest::newRow("#") << int('#') << 0x2B << 0x31;
  QTest::newRow("ß") << 0xDF << 0x0C << 0x2D;
  QTest::newRow("<") << int('<') << 0x56 << 0x64;
  QTest::newRow("-") << int('-') << 0x35 << 0x38;
  QTest::newRow(",") << int(',') << 0x33 << 0x36;
  QTest::newRow(".") << int('.') << 0x34 << 0x37;
  QTest::newRow("1") << int('1') << 0x02 << 0x1E;
  QTest::newRow("Enter") << int(kKeyReturn) << 0x1C << 0x28;
  QTest::newRow("Delete") << int(kKeyDelete) << 0x153 << 0x4C;
  QTest::newRow("keypad 1") << int(kKeyKP_1) << 0x4F << 0x59;
}

void BridgeKeyboardTests::germanKeys()
{
  QFETCH(int, id);
  QFETCH(int, scanCode);
  QFETCH(int, usage);

  Dongle dongle;
  auto keyboard = germanKeyboard(dongle);
  tap(keyboard, static_cast<KeyID>(id), 0, static_cast<KeyButton>(scanCode));

  QCOMPARE(dongle.reports, pressAndRelease(0, usage));
}

void BridgeKeyboardTests::germanShiftedKeys()
{
  Dongle dongle;
  auto keyboard = germanKeyboard(dongle);
  keyboard.keyDown(kKeyShift_L, KeyModifierShift, kLeftShift);
  tap(keyboard, '>', KeyModifierShift, 0x56);
  tap(keyboard, '/', KeyModifierShift, 0x08);
  keyboard.keyUp(kLeftShift);

  QCOMPARE(
      dongle.reports, QStringList({report("press", 0x02, 0)}) + pressAndRelease(0x02, 0x64) +
                          pressAndRelease(0x02, 0x24) + QStringList({report("release", 0, 0)})
  );
}

void BridgeKeyboardTests::germanAltGrSymbols_data()
{
  // Windows on German: AltGr and the key, and the key the device needs
  QTest::addColumn<int>("id");
  QTest::addColumn<int>("scanCode");
  QTest::addColumn<int>("usage");

  QTest::newRow("@") << int('@') << 0x10 << 0x14;
  QTest::newRow("€") << 0x20AC << 0x12 << 0x08;
  QTest::newRow("{") << int('{') << 0x08 << 0x24;
  QTest::newRow("[") << int('[') << 0x09 << 0x25;
  QTest::newRow("]") << int(']') << 0x0A << 0x26;
  QTest::newRow("}") << int('}') << 0x0B << 0x27;
  QTest::newRow("\\") << int('\\') << 0x0C << 0x2D;
  QTest::newRow("~") << int('~') << 0x1B << 0x30;
  QTest::newRow("|") << int('|') << 0x56 << 0x64;
  QTest::newRow("²") << 0xB2 << 0x03 << 0x1F;
  QTest::newRow("³") << 0xB3 << 0x04 << 0x20;
  QTest::newRow("µ") << 0xB5 << 0x32 << 0x10;
}

void BridgeKeyboardTests::germanAltGrSymbols()
{
  QFETCH(int, id);
  QFETCH(int, scanCode);
  QFETCH(int, usage);

  Dongle dongle;
  auto keyboard = germanKeyboard(dongle);
  pressAltGr(keyboard);
  // the hook clears Control and Alt from the mask when they typed the character as AltGr
  tap(keyboard, static_cast<KeyID>(id), 0, static_cast<KeyButton>(scanCode));
  releaseAltGr(keyboard);

  // the key goes with Right Alt only, the device's AltGr: not with Windows' Left Control
  QCOMPARE(
      dongle.reports, QStringList({report("press", 0x01, 0), report("press", 0x41, 0)}) + pressAndRelease(0x40, usage) +
                          QStringList({report("release", 0, 0), report("release", 0, 0)})
  );
}

void BridgeKeyboardTests::germanAltGrTwice()
{
  Dongle dongle;
  auto keyboard = germanKeyboard(dongle);
  pressAltGr(keyboard);
  tap(keyboard, '{', 0, 0x08);
  tap(keyboard, '}', 0, 0x0B);
  releaseAltGr(keyboard);

  QCOMPARE(
      dongle.reports, QStringList({report("press", 0x01, 0), report("press", 0x41, 0)}) + pressAndRelease(0x40, 0x24) +
                          pressAndRelease(0x40, 0x27) + QStringList({report("release", 0, 0), report("release", 0, 0)})
  );
}

void BridgeKeyboardTests::germanControlAltShortcut()
{
  // Control+Alt+T types no character on German, so the hook keeps them in the mask
  Dongle dongle;
  auto keyboard = germanKeyboard(dongle);
  keyboard.keyDown(kKeyControl_L, KeyModifierControl, kLeftControl);
  keyboard.keyDown(kKeyAlt_L, KeyModifierControl | KeyModifierAlt, kLeftAlt);
  tap(keyboard, 't', KeyModifierControl | KeyModifierAlt, 0x14);
  keyboard.keyUp(kLeftAlt);
  keyboard.keyUp(kLeftControl);

  QCOMPARE(
      dongle.reports, QStringList({report("press", 0x01, 0), report("press", 0x05, 0)}) + pressAndRelease(0x05, 0x17) +
                          QStringList({report("release", 0x01, 0), report("release", 0, 0)})
  );
}

void BridgeKeyboardTests::germanControlShortcut()
{
  Dongle dongle;
  auto keyboard = germanKeyboard(dongle);
  keyboard.keyDown(kKeyControl_L, KeyModifierControl, kLeftControl);
  tap(keyboard, 'c', KeyModifierControl, 0x2E);
  keyboard.keyUp(kLeftControl);

  QCOMPARE(
      dongle.reports,
      QStringList({report("press", 0x01, 0)}) + pressAndRelease(0x01, 0x06) + QStringList({report("release", 0, 0)})
  );
}

void BridgeKeyboardTests::germanDeadKeys()
{
  // the events Windows' hook sends: it holds a dead key back, and sends only the character it
  // composes with the next key; a dead key that composes nothing it sends after all
  struct Event
  {
    bool down;
    KeyID id;
    KeyModifierMask mask;
    KeyButton button;
  };
  const auto tapped = [](KeyID id, KeyModifierMask mask, KeyButton button) {
    return std::vector<Event>{{true, id, mask, button}, {false, 0, 0, button}};
  };
  const auto shifted = [](std::vector<Event> events) {
    events.insert(events.begin(), Event{true, kKeyShift_L, KeyModifierShift, kLeftShift});
    events.push_back(Event{false, 0, 0, kLeftShift});
    return events;
  };
  struct Case
  {
    const char *name;
    std::vector<Event> events;
    QStringList reports;
  };

  const QStringList circumflex = pressAndRelease(0, 0x35);
  const QStringList acute = pressAndRelease(0, 0x2E);
  const QStringList grave = pressAndRelease(0x02, 0x2E);
  const auto withShift = [](const QStringList &reports) {
    return QStringList({report("press", 0x02, 0)}) + reports + QStringList({report("release", 0, 0)});
  };

  const Case cases[] = {
      {"^ e", tapped(0xEA, 0, 0x12), circumflex + pressAndRelease(0, 0x08)},
      {"^ Shift+E", shifted(tapped(0xCA, KeyModifierShift, 0x12)), withShift(circumflex + pressAndRelease(0x02, 0x08))},
      {"^ Space", tapped('^', 0, 0x39), circumflex + pressAndRelease(0, 0x2C)},
      {"^ ^",
       [&] {
         auto events = tapped('^', 0, 0x29);
         const auto again = tapped('^', 0, 0x29);
         events.insert(events.end(), again.begin(), again.end());
         return events;
       }(),
       circumflex + circumflex},
      {"^ x",
       [&] {
         auto events = tapped('^', 0, 0x29);
         const auto x = tapped('x', 0, 0x2D);
         events.insert(events.end(), x.begin(), x.end());
         return events;
       }(),
       circumflex + pressAndRelease(0, 0x1B)},
      {"´ e", tapped(0xE9, 0, 0x12), acute + pressAndRelease(0, 0x08)},
      {"` a", tapped(0xE0, 0, 0x1E), grave + pressAndRelease(0, 0x04)},
      // keys of their own, not dead keys
      {"ü", tapped(0xFC, 0, 0x1A), pressAndRelease(0, 0x2F)},
      {"Shift+^ (°)", shifted(tapped(0xB0, KeyModifierShift, 0x29)), withShift(pressAndRelease(0x02, 0x35))},
  };

  for (const auto &c : cases) {
    Dongle dongle;
    auto keyboard = germanKeyboard(dongle);
    for (const auto &event : c.events) {
      if (event.down) {
        keyboard.keyDown(event.id, event.mask, event.button);
      } else {
        keyboard.keyUp(event.button);
      }
    }
    QVERIFY2(
        dongle.reports == c.reports,
        qPrintable(QStringLiteral("%1: sent %2, expected %3")
                       .arg(
                           QString::fromUtf8(c.name), dongle.reports.join(QStringLiteral(", ")),
                           c.reports.join(QStringLiteral(", "))
                       ))
    );
  }
}

void BridgeKeyboardTests::keyRepeatKeepsAltGr()
{
  Dongle dongle;
  auto keyboard = germanKeyboard(dongle);
  pressAltGr(keyboard);
  keyboard.keyDown('@', 0, 0x10);
  QVERIFY(keyboard.keyRepeat('@', 0, 2, 0x10));

  QCOMPARE(
      dongle.reports, QStringList({report("press", 0x01, 0), report("press", 0x41, 0), report("press", 0x40, 0x14)}) +
                          pressAndRelease(0x40, 0x14) + pressAndRelease(0x40, 0x14)
  );
}

void BridgeKeyboardTests::allKeysUpReleasesEverything()
{
  Dongle dongle;
  auto keyboard = germanKeyboard(dongle);
  pressAltGr(keyboard);
  keyboard.keyDown('@', 0, 0x10);
  keyboard.allKeysUp();

  QCOMPARE(
      dongle.reports, QStringList(
                          {report("press", 0x01, 0), report("press", 0x41, 0), report("press", 0x40, 0x14),
                           report("release", 0x40, 0x14), report("release", 0x40, 0), report("release", 0, 0)}
                      )
  );
  QVERIFY(!keyboard.isKeyDown(0x10));
  QVERIFY(!keyboard.isKeyDown(kRightAlt));
  QCOMPARE(keyboard.activeModifiers(), KeyModifierMask(0));
}

void BridgeKeyboardTests::mediaKeysUseConsumerControl()
{
  Dongle dongle;
  auto keyboard = germanKeyboard(dongle);
  tap(keyboard, kKeyAudioMute, 0, 0x120);

  QVERIFY(dongle.reports.isEmpty());
  QCOMPARE(dongle.consumer, QStringList({QStringLiteral("press 00e2"), QStringLiteral("release 00e2")}));
}

void BridgeKeyboardTests::onlyWindowsSendsKeysAsPressed()
{
  Dongle dongle;
  QVERIFY(makeKeyboard(dongle, KeyCodes::Windows, KeyboardLayout::SameAsComputer).sendsKeysAsPressed());
  QVERIFY(!makeKeyboard(dongle, KeyCodes::Windows, KeyboardLayout::Us).sendsKeysAsPressed());
  QVERIFY(!makeKeyboard(dongle, KeyCodes::MacOS, KeyboardLayout::SameAsComputer).sendsKeysAsPressed());
  QVERIFY(!makeKeyboard(dongle, KeyCodes::Linux, KeyboardLayout::SameAsComputer).sendsKeysAsPressed());

  // so a Mac goes by the character, as on US
  auto keyboard = makeKeyboard(dongle, KeyCodes::MacOS, KeyboardLayout::SameAsComputer);
  tap(keyboard, 'z', 0, 7);
  QCOMPARE(dongle.reports, pressAndRelease(0, 0x1D));
}

void BridgeKeyboardTests::layoutChangeTakesEffect()
{
  Dongle dongle;
  auto keyboard = makeKeyboard(dongle, KeyCodes::Windows, KeyboardLayout::Us);
  tap(keyboard, 'z', 0, 0x15);
  keyboard.setLayout(KeyboardLayout::SameAsComputer);
  tap(keyboard, 'z', 0, 0x15);

  QCOMPARE(dongle.reports, pressAndRelease(0, 0x1D) + pressAndRelease(0, 0x1C));
}

QTEST_MAIN(BridgeKeyboardTests)
