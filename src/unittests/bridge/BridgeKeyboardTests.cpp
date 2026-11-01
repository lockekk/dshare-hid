/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "BridgeKeyboardTests.h"

#include "platform/bridge/BridgeKeyboard.h"

#include <map>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

using namespace deskflow::bridge;
using KeyCodes = BridgeKeyboard::KeyCodes;

namespace {

// the Windows scan codes of the modifier keys the tests press
constexpr KeyButton kLeftShift = 0x2A;
constexpr KeyButton kRightShift = 0x36;
constexpr KeyButton kLeftControl = 0x1D;
constexpr KeyButton kRightControl = 0x11D;
constexpr KeyButton kLeftAlt = 0x38;
constexpr KeyButton kRightAlt = 0x138;

// a key code that no computer's table has
constexpr KeyButton kUnknownKey = 0x1FF;
// Volume Up's Windows scan code: a media key goes by its key ID, whatever its key code
constexpr KeyButton kVolumeUp = 0x130;

const KeyCodes kAllKeyCodes[] = {KeyCodes::MacOS, KeyCodes::Windows, KeyCodes::Linux};
const KeyboardLayout kBothLayouts[] = {KeyboardLayout::Us, KeyboardLayout::SameAsComputer};

QLatin1String nameOf(KeyCodes keyCodes)
{
  switch (keyCodes) {
  case KeyCodes::MacOS:
    return QLatin1String("Mac");
  case KeyCodes::Windows:
    return QLatin1String("Windows");
  case KeyCodes::Linux:
    return QLatin1String("Linux");
  }
  return QLatin1String("?");
}

QLatin1String nameOf(KeyboardLayout layout)
{
  return layout == KeyboardLayout::Us ? QLatin1String("US") : QLatin1String("same as this computer");
}

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

// a consumer control report, as "press 00e9" (Volume Up)
QString consumerReport(const char *type, unsigned usage)
{
  return QStringLiteral("%1 %2").arg(QLatin1String(type)).arg(usage, 4, 16, QLatin1Char('0'));
}

// the same among the keyboard reports
QString consumerEvent(const char *type, unsigned usage)
{
  return QStringLiteral("consumer ") + consumerReport(type, usage);
}

// what a keyboard sends the dongle
struct Dongle
{
  QStringList reports;  // the keyboard reports
  QStringList consumer; // the consumer control reports
  QStringList all;      // both, in the order sent
  int sends = 0;
  int failingSend = 0; // the send that fails, counted from 1; 0 for none
};

BridgeKeyboard
makeKeyboard(Dongle &dongle, KeyCodes keyCodes, KeyboardLayout layout, BridgeKeyboard::DeadKeyFinder deadKeys = nullptr)
{
  BridgeKeyboard keyboard(
      keyCodes,
      [&dongle](HidEventType type, uint8_t modifiers, uint8_t key) {
        const QString sent = report(type == HidEventType::KeyboardPress ? "press" : "release", modifiers, key);
        dongle.reports.append(sent);
        dongle.all.append(sent);
        return ++dongle.sends != dongle.failingSend;
      },
      [&dongle](HidEventType type, uint16_t usage) {
        const char *kind = type == HidEventType::ConsumerControlPress ? "press" : "release";
        dongle.consumer.append(consumerReport(kind, usage));
        dongle.all.append(consumerEvent(kind, usage));
        return ++dongle.sends != dongle.failingSend;
      },
      std::move(deadKeys)
  );
  keyboard.setLayout(layout);
  return keyboard;
}

// a key of a Windows layout, as windowsDeadKey() reads it from Windows
struct LayoutKey
{
  uint16_t scanCode;
  int shiftState; // VkKeyScanEx's: 0, 1 Shift, 6 AltGr (Control and Alt), 7 Shift and AltGr
  char16_t character;
  bool dead = false;
};
using Layout = std::vector<LayoutKey>;

// the keys the tests type on Windows' German (Germany) layout: ^ is a dead key, and so are ´
// and, with Shift, ` on the key left of Backspace
const Layout kGerman = {
    {0x29, 0, u'^', true}, {0x29, 1, 0x00B0}, {0x0D, 0, 0x00B4, true}, {0x0D, 1, u'`', true},
    {0x12, 0, u'e'},       {0x12, 1, u'E'},   {0x12, 6, 0x20AC},       {0x1E, 0, u'a'},
    {0x1A, 0, 0x00FC},     {0x2D, 0, u'x'},   {0x39, 0, u' '},         {0x10, 6, u'@'},
};

// US International: ' " ` ~ and Shift+6's ^ are dead keys, and AltGr types some accented letters
const Layout kUsInternational = {
    {0x28, 0, u'\'', true}, {0x28, 1, u'"', true}, {0x29, 0, u'`', true}, {0x29, 1, u'~', true}, {0x07, 1, u'^', true},
    {0x28, 6, 0x00B4},      {0x12, 0, u'e'},       {0x12, 1, u'E'},       {0x12, 6, 0x00E9},     {0x12, 7, 0x00C9},
    {0x16, 0, u'u'},        {0x31, 0, u'n'},       {0x31, 6, 0x00F1},     {0x39, 0, u' '},
};

// Czech: the number row types ě š č..., and Shift with the key left of Backspace is a dead ˇ
const Layout kCzech = {{0x03, 0, 0x011B}, {0x0D, 1, 0x02C7, true}, {0x12, 0, u'e'}, {0x12, 1, u'E'}};

int shiftStateOf(uint8_t heldModifiers)
{
  return ((heldModifiers & 0x22) != 0 ? 1 : 0) | ((heldModifiers & 0x11) != 0 ? 2 : 0) |
         ((heldModifiers & 0x44) != 0 ? 4 : 0);
}

DeadKeyLayout deadKeyLayout(const Layout &layout)
{
  return {
      [&layout](uint16_t scanCode, uint8_t heldModifiers, KeyID id) {
        for (const auto &key : layout) {
          if (key.scanCode == scanCode && key.shiftState == shiftStateOf(heldModifiers)) {
            // with Caps Lock on or off
            const QChar character(key.character);
            return id == character.unicode() || id == character.toUpper().unicode() ||
                   id == character.toLower().unicode();
          }
        }
        return false;
      },
      [&layout](char16_t character, uint16_t &scanCode, uint8_t &modifiers) {
        for (const auto &key : layout) {
          if (key.dead && key.character == character) {
            scanCode = key.scanCode;
            modifiers = static_cast<uint8_t>(((key.shiftState & 1) != 0 ? 0x02 : 0) | (key.shiftState >= 6 ? 0x40 : 0));
            return true;
          }
        }
        return false;
      }
  };
}

BridgeKeyboard::DeadKeyFinder deadKeysOf(const Layout &layout)
{
  return [&layout](KeyID id, uint16_t scanCode, uint8_t heldModifiers, uint16_t &deadScanCode, uint8_t &deadModifiers) {
    return deskflow::bridge::findDeadKey(
        id, scanCode, heldModifiers, deadKeyLayout(layout), deadScanCode, deadModifiers
    );
  };
}

// Windows on German, and the device set to German too
BridgeKeyboard germanKeyboard(Dongle &dongle)
{
  return makeKeyboard(dongle, KeyCodes::Windows, KeyboardLayout::SameAsComputer, deadKeysOf(kGerman));
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

QString mismatch(const QString &what, const QStringList &sent, const QStringList &expected)
{
  return QStringLiteral("%1: sent %2, expected %3")
      .arg(what, sent.join(QStringLiteral(", ")), expected.join(QStringLiteral(", ")));
}

// key code -> HID usage, or modifier bit
using KeyTable = std::map<int, int>;

// the Mac's key codes: the virtual key code + 1.  Not here, so they go by their key ID only:
// Escape, Home, End, Page Up and Page Down, Right Command, the keypad, F13 and up, and the ISO key
const KeyTable kMacKeys = {
    // a s d f h g z x c v b
    {1, 0x04},
    {2, 0x16},
    {3, 0x07},
    {4, 0x09},
    {5, 0x0B},
    {6, 0x0A},
    {7, 0x1D},
    {8, 0x1B},
    {9, 0x06},
    {10, 0x19},
    {12, 0x05},
    // q w e r y t
    {13, 0x14},
    {14, 0x1A},
    {15, 0x08},
    {16, 0x15},
    {17, 0x1C},
    {18, 0x17},
    // 1 2 3 4 6 5 = 9 7 - 8 0
    {19, 0x1E},
    {20, 0x1F},
    {21, 0x20},
    {22, 0x21},
    {23, 0x23},
    {24, 0x22},
    {25, 0x2E},
    {26, 0x26},
    {27, 0x24},
    {28, 0x2D},
    {29, 0x25},
    {30, 0x27},
    // ] o u [ i p Return l j ' k ; \ , / n m .
    {31, 0x30},
    {32, 0x12},
    {33, 0x18},
    {34, 0x2F},
    {35, 0x0C},
    {36, 0x13},
    {37, 0x28},
    {38, 0x0F},
    {39, 0x0D},
    {40, 0x34},
    {41, 0x0E},
    {42, 0x33},
    {43, 0x31},
    {44, 0x36},
    {45, 0x38},
    {46, 0x11},
    {47, 0x10},
    {48, 0x37},
    // Tab, Space, `, Backspace, Caps Lock, keypad Clear as Num Lock
    {49, 0x2B},
    {50, 0x2C},
    {51, 0x35},
    {52, 0x2A},
    {58, 0x39},
    {72, 0x53},
    // F1-F12
    {123, 0x3A},
    {121, 0x3B},
    {100, 0x3C},
    {119, 0x3D},
    {97, 0x3E},
    {98, 0x3F},
    {99, 0x40},
    {101, 0x41},
    {102, 0x42},
    {110, 0x43},
    {104, 0x44},
    {112, 0x45},
    // the context menu key, Help as Insert, forward Delete, Left Right Down Up
    {111, 0x65},
    {115, 0x49},
    {118, 0x4C},
    {124, 0x50},
    {125, 0x4F},
    {126, 0x51},
    {127, 0x52},
};
// Shift, Right Shift, Control, Right Control, Option, Right Option, Command
const KeyTable kMacModifiers = {{57, 0x02}, {61, 0x20}, {60, 0x01}, {63, 0x10}, {59, 0x04}, {62, 0x40}, {56, 0x08}};

// Linux's X key codes.  Not here, so they go by their key ID only: Escape, Home, End, Page Up
// and Page Down, the rest of the keypad, F13 and up, and the ISO key
const KeyTable kLinuxKeys = {
    // 1-0 - = Backspace Tab
    {10, 0x1E},
    {11, 0x1F},
    {12, 0x20},
    {13, 0x21},
    {14, 0x22},
    {15, 0x23},
    {16, 0x24},
    {17, 0x25},
    {18, 0x26},
    {19, 0x27},
    {20, 0x2D},
    {21, 0x2E},
    {22, 0x2A},
    {23, 0x2B},
    // q w e r t y u i o p [ ] Return
    {24, 0x14},
    {25, 0x1A},
    {26, 0x08},
    {27, 0x15},
    {28, 0x17},
    {29, 0x1C},
    {30, 0x18},
    {31, 0x0C},
    {32, 0x12},
    {33, 0x13},
    {34, 0x2F},
    {35, 0x30},
    {36, 0x28},
    // a s d f g h j k l ; ' `
    {38, 0x04},
    {39, 0x16},
    {40, 0x07},
    {41, 0x09},
    {42, 0x0A},
    {43, 0x0B},
    {44, 0x0D},
    {45, 0x0E},
    {46, 0x0F},
    {47, 0x33},
    {48, 0x34},
    {49, 0x35},
    // \ z x c v b n m , . /
    {51, 0x31},
    {52, 0x1D},
    {53, 0x1B},
    {54, 0x06},
    {55, 0x19},
    {56, 0x05},
    {57, 0x11},
    {58, 0x10},
    {59, 0x36},
    {60, 0x37},
    {61, 0x38},
    // Space, Caps Lock, F1-F10, Num Lock, Scroll Lock
    {65, 0x2C},
    {66, 0x39},
    {67, 0x3A},
    {68, 0x3B},
    {69, 0x3C},
    {70, 0x3D},
    {71, 0x3E},
    {72, 0x3F},
    {73, 0x40},
    {74, 0x41},
    {75, 0x42},
    {76, 0x43},
    {77, 0x53},
    {78, 0x47},
    // keypad 0, as 1: a mistake (see keypadKeys)
    {90, 0x1E},
    // F11 F12, Print Screen, Up Left Right Down, Insert Delete, Pause, Menu
    {95, 0x44},
    {96, 0x45},
    {107, 0x46},
    {111, 0x52},
    {113, 0x50},
    {114, 0x4F},
    {116, 0x51},
    {118, 0x49},
    {119, 0x4C},
    {127, 0x48},
    {135, 0x65},
};
// Shift_L Shift_R Control_L Control_R Alt_L Alt_R Super_L Super_R
const KeyTable kLinuxModifiers = {{50, 0x02}, {62, 0x20},  {37, 0x01},  {105, 0x10},
                                  {64, 0x04}, {108, 0x40}, {133, 0x08}, {134, 0x80}};

// Windows' scan codes, 0x100 for an extended key
const KeyTable kWindowsKeys = {
    // Esc, 1-0 - = Backspace Tab
    {0x01, 0x29},
    {0x02, 0x1E},
    {0x03, 0x1F},
    {0x04, 0x20},
    {0x05, 0x21},
    {0x06, 0x22},
    {0x07, 0x23},
    {0x08, 0x24},
    {0x09, 0x25},
    {0x0A, 0x26},
    {0x0B, 0x27},
    {0x0C, 0x2D},
    {0x0D, 0x2E},
    {0x0E, 0x2A},
    {0x0F, 0x2B},
    // q w e r t y u i o p [ ] Enter
    {0x10, 0x14},
    {0x11, 0x1A},
    {0x12, 0x08},
    {0x13, 0x15},
    {0x14, 0x17},
    {0x15, 0x1C},
    {0x16, 0x18},
    {0x17, 0x0C},
    {0x18, 0x12},
    {0x19, 0x13},
    {0x1A, 0x2F},
    {0x1B, 0x30},
    {0x1C, 0x28},
    // a s d f g h j k l ; ' ` \ (# on ISO keyboards)
    {0x1E, 0x04},
    {0x1F, 0x16},
    {0x20, 0x07},
    {0x21, 0x09},
    {0x22, 0x0A},
    {0x23, 0x0B},
    {0x24, 0x0D},
    {0x25, 0x0E},
    {0x26, 0x0F},
    {0x27, 0x33},
    {0x28, 0x34},
    {0x29, 0x35},
    {0x2B, 0x31},
    // z x c v b n m , . /, keypad *, Space, Caps Lock
    {0x2C, 0x1D},
    {0x2D, 0x1B},
    {0x2E, 0x06},
    {0x2F, 0x19},
    {0x30, 0x05},
    {0x31, 0x11},
    {0x32, 0x10},
    {0x33, 0x36},
    {0x34, 0x37},
    {0x35, 0x38},
    {0x37, 0x55},
    {0x39, 0x2C},
    {0x3A, 0x39},
    // F1-F10, Pause, Scroll Lock
    {0x3B, 0x3A},
    {0x3C, 0x3B},
    {0x3D, 0x3C},
    {0x3E, 0x3D},
    {0x3F, 0x3E},
    {0x40, 0x3F},
    {0x41, 0x40},
    {0x42, 0x41},
    {0x43, 0x42},
    {0x44, 0x43},
    {0x45, 0x48},
    {0x46, 0x47},
    // keypad 7 8 9 - 4 5 6 + 1 2 3 0 .
    {0x47, 0x5F},
    {0x48, 0x60},
    {0x49, 0x61},
    {0x4A, 0x56},
    {0x4B, 0x5C},
    {0x4C, 0x5D},
    {0x4D, 0x5E},
    {0x4E, 0x57},
    {0x4F, 0x59},
    {0x50, 0x5A},
    {0x51, 0x5B},
    {0x52, 0x62},
    {0x53, 0x63},
    // SysRq, the ISO key beside Left Shift, F11 F12, keypad =
    {0x54, 0x46},
    {0x56, 0x64},
    {0x57, 0x44},
    {0x58, 0x45},
    {0x59, 0x67},
    // F13-F23, F24
    {0x64, 0x68},
    {0x65, 0x69},
    {0x66, 0x6A},
    {0x67, 0x6B},
    {0x68, 0x6C},
    {0x69, 0x6D},
    {0x6A, 0x6E},
    {0x6B, 0x6F},
    {0x6C, 0x70},
    {0x6D, 0x71},
    {0x6E, 0x72},
    {0x76, 0x73},
    // Katakana/Hiragana, Ro, Henkan, Muhenkan, Yen, keypad . on Brazilian keyboards
    {0x70, 0x88},
    {0x73, 0x87},
    {0x79, 0x8A},
    {0x7B, 0x8B},
    {0x7D, 0x89},
    {0x7E, 0x85},
    // keypad Enter, keypad /, Print Screen, Num Lock, Break
    {0x11C, 0x58},
    {0x135, 0x54},
    {0x137, 0x46},
    {0x145, 0x53},
    {0x146, 0x48},
    // Home Up Page Up Left Right End Down Page Down Insert Delete, Menu
    {0x147, 0x4A},
    {0x148, 0x52},
    {0x149, 0x4B},
    {0x14B, 0x50},
    {0x14D, 0x4F},
    {0x14F, 0x4D},
    {0x150, 0x51},
    {0x151, 0x4E},
    {0x152, 0x49},
    {0x153, 0x4C},
    {0x15D, 0x65},
};
// Left Shift, Right Shift, Left Control, Right Control, Left Alt, Right Alt, Left Windows, Right
// Windows
const KeyTable kWindowsModifiers = {{0x2A, 0x02}, {0x36, 0x20},  {0x1D, 0x01},  {0x11D, 0x10},
                                    {0x38, 0x04}, {0x138, 0x40}, {0x15B, 0x08}, {0x15C, 0x80}};

// each key code from 0 to 0x1FF pressed and released by itself, with no key ID, in both layouts
QStringList checkEveryKeyCode(KeyCodes keyCodes, const KeyTable &keys, const KeyTable &modifiers)
{
  QStringList problems;
  for (const auto layout : kBothLayouts) {
    for (int code = 0; code <= 0x1FF; ++code) {
      const auto button = static_cast<KeyButton>(code);
      Dongle dongle;
      auto keyboard = makeKeyboard(dongle, keyCodes, layout);
      keyboard.keyDown(kKeyNone, 0, button);
      const bool wasDown = keyboard.isKeyDown(button);
      keyboard.keyUp(button);

      QStringList expected;
      if (const auto modifier = modifiers.find(code); modifier != modifiers.end()) {
        expected = {report("press", modifier->second, 0), report("release", 0, 0)};
      } else if (const auto key = keys.find(code); key != keys.end()) {
        expected = pressAndRelease(0, key->second);
      } else {
        expected = {report("release", 0, 0)}; // nothing to press
      }
      if (dongle.reports != expected || !wasDown || keyboard.isKeyDown(button)) {
        problems.append(mismatch(
            QStringLiteral("%1, %2, key code 0x%3").arg(nameOf(keyCodes), nameOf(layout)).arg(code, 0, 16),
            dongle.reports, expected
        ));
      }
    }
  }
  return problems;
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
  QCOMPARE(int(maskHidModifiers(KeyModifierControl, 0x10)), 0x10);
  QCOMPARE(int(maskHidModifiers(KeyModifierSuper, 0x80)), 0x80);
  QCOMPARE(int(maskHidModifiers(KeyModifierMeta, 0x00)), 0x08);
  QCOMPARE(int(maskHidModifiers(KeyModifierAltGr, 0x00)), 0x40);
  // both sides held
  QCOMPARE(int(maskHidModifiers(KeyModifierShift, 0x22)), 0x22);
  // held, but the mask leaves it out
  QCOMPARE(int(maskHidModifiers(0, 0x41)), 0x00);
  // the locks aren't modifier keys
  QCOMPARE(int(maskHidModifiers(KeyModifierCapsLock | KeyModifierNumLock | KeyModifierScrollLock, 0x00)), 0x00);
}

void BridgeKeyboardTests::heldModifiersTurnControlAltIntoAltGr()
{
  // Windows' AltGr, and Control+Alt typing as AltGr
  QCOMPARE(int(heldHidModifiers(0, 0x41)), 0x40);
  QCOMPARE(int(heldHidModifiers(0, 0x05)), 0x40);
  QCOMPARE(int(heldHidModifiers(0, 0x14)), 0x40); // Right Control and Left Alt
  QCOMPARE(int(heldHidModifiers(KeyModifierShift, 0x43)), 0x42);
  // a shortcut: the mask keeps Control and Alt, or either
  QCOMPARE(int(heldHidModifiers(KeyModifierControl | KeyModifierAlt, 0x41)), 0x41);
  QCOMPARE(int(heldHidModifiers(KeyModifierControl, 0x05)), 0x05);
  QCOMPARE(int(heldHidModifiers(KeyModifierAlt, 0x05)), 0x05);
  // not both
  QCOMPARE(int(heldHidModifiers(KeyModifierAlt, 0x40)), 0x40);
  QCOMPARE(int(heldHidModifiers(0, 0x01)), 0x01);
  QCOMPARE(int(heldHidModifiers(0, 0x00)), 0x00);
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

void BridgeKeyboardTests::findDeadKey_data()
{
  // the character the hook sends, the key it sends it with and the modifier keys held, and the
  // dead key, if any, that the character was composed with
  QTest::addColumn<QString>("layout");
  QTest::addColumn<int>("id");
  QTest::addColumn<int>("scanCode");
  QTest::addColumn<int>("held");
  QTest::addColumn<bool>("found");
  QTest::addColumn<int>("deadScanCode");
  QTest::addColumn<int>("deadModifiers");

  const QString german = QStringLiteral("German");
  QTest::newRow("German ^ e") << german << 0xEA << 0x12 << 0 << true << 0x29 << 0;
  QTest::newRow("German ^ Shift+E") << german << 0xCA << 0x12 << 0x02 << true << 0x29 << 0;
  QTest::newRow("German ´ e") << german << 0xE9 << 0x12 << 0 << true << 0x0D << 0;
  QTest::newRow("German ` a") << german << 0xE0 << 0x1E << 0 << true << 0x0D << 0x02;
  QTest::newRow("German ^ Space") << german << int('^') << 0x39 << 0 << true << 0x29 << 0;
  QTest::newRow("German ^ ^") << german << int('^') << 0x29 << 0 << false << 0 << 0;
  QTest::newRow("German Shift+^ (°)") << german << 0xB0 << 0x29 << 0x02 << false << 0 << 0;
  QTest::newRow("German AltGr+E (€)") << german << 0x20AC << 0x12 << 0x41 << false << 0 << 0;
  QTest::newRow("German ü") << german << 0xFC << 0x1A << 0 << false << 0 << 0;
  QTest::newRow("German ñ, with no dead ~") << german << 0xF1 << 0x31 << 0 << false << 0 << 0;
  QTest::newRow("a key without a character") << german << int(kKeyDelete) << 0x153 << 0 << false << 0 << 0;
  QTest::newRow("a control character") << german << 0x08 << 0x0E << 0 << false << 0 << 0;
  QTest::newRow("beyond 16 bits") << german << 0x1F600 << 0x12 << 0 << false << 0 << 0;

  const QString usInternational = QStringLiteral("US International");
  QTest::newRow("US International ' e") << usInternational << 0xE9 << 0x12 << 0 << true << 0x28 << 0;
  QTest::newRow("US International AltGr+E (é)") << usInternational << 0xE9 << 0x12 << 0x41 << false << 0 << 0;
  QTest::newRow("US International Shift+AltGr+E (É)") << usInternational << 0xC9 << 0x12 << 0x43 << false << 0 << 0;
  QTest::newRow("US International \" u") << usInternational << 0xFC << 0x16 << 0 << true << 0x28 << 0x02;
  QTest::newRow("US International ~ n") << usInternational << 0xF1 << 0x31 << 0 << true << 0x29 << 0x02;
  QTest::newRow("US International ^ e") << usInternational << 0xEA << 0x12 << 0 << true << 0x07 << 0x02;
  QTest::newRow("US International ' Space") << usInternational << int('\'') << 0x39 << 0 << true << 0x28 << 0;
  QTest::newRow("US International ' '") << usInternational << int('\'') << 0x28 << 0 << false << 0 << 0;

  const QString czech = QStringLiteral("Czech");
  QTest::newRow("Czech ˇ e") << czech << 0x011B << 0x12 << 0 << true << 0x0D << 0x02;
  QTest::newRow("Czech ě") << czech << 0x011B << 0x03 << 0 << false << 0 << 0;
  QTest::newRow("Czech ě with Caps Lock (Ě)") << czech << 0x011A << 0x03 << 0 << false << 0 << 0;
}

void BridgeKeyboardTests::findDeadKey()
{
  QFETCH(QString, layout);
  QFETCH(int, id);
  QFETCH(int, scanCode);
  QFETCH(int, held);
  QFETCH(bool, found);
  QFETCH(int, deadScanCode);
  QFETCH(int, deadModifiers);

  const Layout &keys = layout == QStringLiteral("German")  ? kGerman
                       : layout == QStringLiteral("Czech") ? kCzech
                                                           : kUsInternational;
  uint16_t foundScanCode = 0;
  uint8_t foundModifiers = 0;
  QCOMPARE(
      deskflow::bridge::findDeadKey(
          static_cast<KeyID>(id), static_cast<uint16_t>(scanCode), static_cast<uint8_t>(held), deadKeyLayout(keys),
          foundScanCode, foundModifiers
      ),
      found
  );
  if (found) {
    QCOMPARE(int(foundScanCode), deadScanCode);
    QCOMPARE(int(foundModifiers), deadModifiers);
  }
}

void BridgeKeyboardTests::unknownLayoutIsUs()
{
  QVERIFY(keyboardLayout(0) == KeyboardLayout::Us);
  QVERIFY(keyboardLayout(1) == KeyboardLayout::SameAsComputer);
  QVERIFY(keyboardLayout(2) == KeyboardLayout::Us);
  QVERIFY(keyboardLayout(255) == KeyboardLayout::Us);
}

void BridgeKeyboardTests::everyMacKeyCode()
{
  const QStringList problems = checkEveryKeyCode(KeyCodes::MacOS, kMacKeys, kMacModifiers);
  QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QLatin1Char('\n'))));
}

void BridgeKeyboardTests::everyLinuxKeyCode()
{
  const QStringList problems = checkEveryKeyCode(KeyCodes::Linux, kLinuxKeys, kLinuxModifiers);
  QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QLatin1Char('\n'))));
}

void BridgeKeyboardTests::everyWindowsKeyCode()
{
  const QStringList problems = checkEveryKeyCode(KeyCodes::Windows, kWindowsKeys, kWindowsModifiers);
  QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QLatin1Char('\n'))));
}

void BridgeKeyboardTests::modifierKeysById_data()
{
  QTest::addColumn<int>("id");
  QTest::addColumn<int>("bit");

  // deskflow's key IDs, and X's keysyms
  QTest::newRow("Shift_L") << int(kKeyShift_L) << 0x02;
  QTest::newRow("Shift_L (X)") << 0xFFE1 << 0x02;
  QTest::newRow("Shift_R") << int(kKeyShift_R) << 0x20;
  QTest::newRow("Shift_R (X)") << 0xFFE2 << 0x20;
  QTest::newRow("Control_L") << int(kKeyControl_L) << 0x01;
  QTest::newRow("Control_L (X)") << 0xFFE3 << 0x01;
  QTest::newRow("Control_R") << int(kKeyControl_R) << 0x10;
  QTest::newRow("Control_R (X)") << 0xFFE4 << 0x10;
  QTest::newRow("Alt_L") << int(kKeyAlt_L) << 0x04;
  QTest::newRow("Alt_L (X)") << 0xFFE9 << 0x04;
  QTest::newRow("Alt_R") << int(kKeyAlt_R) << 0x40;
  QTest::newRow("Alt_R (X)") << 0xFFEA << 0x40;
  QTest::newRow("AltGr") << int(kKeyAltGr) << 0x40;
  QTest::newRow("Meta_L") << int(kKeyMeta_L) << 0x08;
  QTest::newRow("Meta_L (X)") << 0xFFE7 << 0x08;
  QTest::newRow("Meta_R") << int(kKeyMeta_R) << 0x80;
  QTest::newRow("Meta_R (X)") << 0xFFE8 << 0x80;
  QTest::newRow("Super_L") << int(kKeySuper_L) << 0x08;
  QTest::newRow("Super_L (X)") << 0xFFEB << 0x08;
  QTest::newRow("Super_R") << int(kKeySuper_R) << 0x80;
  QTest::newRow("Super_R (X)") << 0xFFEC << 0x80;
}

void BridgeKeyboardTests::modifierKeysById()
{
  QFETCH(int, id);
  QFETCH(int, bit);

  for (const auto keyCodes : kAllKeyCodes) {
    for (const auto layout : kBothLayouts) {
      Dongle dongle;
      auto keyboard = makeKeyboard(dongle, keyCodes, layout);
      // no key code, so it goes by its key ID
      keyboard.keyDown(static_cast<KeyID>(id), 0, 0);
      QVERIFY(keyboard.isKeyDown(0));
      keyboard.keyUp(0);

      const QStringList expected = {report("press", bit, 0), report("release", 0, 0)};
      QVERIFY2(
          dongle.reports == expected,
          qPrintable(mismatch(QStringLiteral("%1, %2").arg(nameOf(keyCodes), nameOf(layout)), dongle.reports, expected))
      );
    }
  }
}

void BridgeKeyboardTests::keysById_data()
{
  QTest::addColumn<int>("id");
  QTest::addColumn<int>("usage"); // 0 for none

  // the letters, either case, and the digits: HID has them in order
  for (char c = 'a'; c <= 'z'; ++c) {
    QTest::addRow("%c", c) << int(c) << 0x04 + (c - 'a');
    QTest::addRow("%c", c - 'a' + 'A') << int(c - 'a' + 'A') << 0x04 + (c - 'a');
  }
  for (char c = '1'; c <= '9'; ++c) {
    QTest::addRow("%c", c) << int(c) << 0x1E + (c - '1');
  }
  QTest::newRow("0") << int('0') << 0x27;

  // where a US keyboard has them
  const std::pair<char, int> symbols[] = {
      {'-', 0x2D}, {'_', 0x2D},  {'=', 0x2E}, {'+', 0x2E}, {'[', 0x2F}, {'{', 0x2F},  {']', 0x30},
      {'}', 0x30}, {'\\', 0x31}, {'|', 0x31}, {';', 0x33}, {':', 0x33}, {'\'', 0x34}, {'"', 0x34},
      {'`', 0x35}, {'~', 0x35},  {',', 0x36}, {'<', 0x36}, {'.', 0x37}, {'>', 0x37},  {'/', 0x38},
      {'?', 0x38}, {'!', 0x1E},  {'@', 0x1F}, {'#', 0x20}, {'$', 0x21}, {'%', 0x22},  {'^', 0x23},
      {'&', 0x24}, {'*', 0x25},  {'(', 0x26}, {')', 0x27}, {' ', 0x2C},
  };
  for (const auto &[symbol, usage] : symbols) {
    QTest::addRow("'%c'", symbol) << int(symbol) << usage;
  }

  struct Key
  {
    const char *name;
    KeyID id;
    int usage;
  };
  const Key keys[] = {
      {"Return", kKeyReturn, 0x28},
      {"Escape", kKeyEscape, 0x29},
      {"BackSpace", kKeyBackSpace, 0x2A},
      {"Tab", kKeyTab, 0x2B},
      {"Caps Lock", kKeyCapsLock, 0x39},
      {"Print", kKeyPrint, 0x46},
      {"Scroll Lock", kKeyScrollLock, 0x47},
      {"Pause", kKeyPause, 0x48},
      {"Insert", kKeyInsert, 0x49},
      {"Home", kKeyHome, 0x4A},
      {"Page Up", kKeyPageUp, 0x4B},
      {"Delete", kKeyDelete, 0x4C},
      {"End", kKeyEnd, 0x4D},
      {"Page Down", kKeyPageDown, 0x4E},
      {"Right", kKeyRight, 0x4F},
      {"Left", kKeyLeft, 0x50},
      {"Down", kKeyDown, 0x51},
      {"Up", kKeyUp, 0x52},
      {"Num Lock", kKeyNumLock, 0x53},
      {"Menu", kKeyMenu, 0x65},
  };
  for (const auto &key : keys) {
    QTest::addRow("%s", key.name) << int(key.id) << key.usage;
    // X's keysym, 0x1000 above deskflow's key ID
    QTest::addRow("%s (X)", key.name) << int(key.id + 0x1000) << key.usage;
  }
  for (int i = 0; i < 12; ++i) {
    QTest::addRow("F%d", i + 1) << int(kKeyF1 + i) << 0x3A + i;
    QTest::addRow("F%d (X)", i + 1) << int(kKeyF1 + i + 0x1000) << 0x3A + i;
  }

  // nothing to press without a key code
  QTest::newRow("F13") << int(kKeyF13) << 0;
  QTest::newRow("F25") << int(kKeyF25) << 0;
  QTest::newRow("keypad 1") << int(kKeyKP_1) << 0;
  QTest::newRow("keypad Enter") << int(kKeyKP_Enter) << 0;
  QTest::newRow("é") << 0xE9 << 0;
  QTest::newRow("€") << 0x20AC << 0;
  QTest::newRow("none") << int(kKeyNone) << 0;
}

void BridgeKeyboardTests::keysById()
{
  QFETCH(int, id);
  QFETCH(int, usage);

  for (const auto keyCodes : kAllKeyCodes) {
    for (const auto layout : kBothLayouts) {
      Dongle dongle;
      auto keyboard = makeKeyboard(dongle, keyCodes, layout);
      // no key code, so it goes by its key ID
      tap(keyboard, static_cast<KeyID>(id), 0, 0);

      const QStringList expected = usage != 0 ? pressAndRelease(0, usage) : QStringList{report("release", 0, 0)};
      QVERIFY2(
          dongle.reports == expected,
          qPrintable(mismatch(QStringLiteral("%1, %2").arg(nameOf(keyCodes), nameOf(layout)), dongle.reports, expected))
      );
    }
  }
}

void BridgeKeyboardTests::keypadKeys_data()
{
  // the keypad's keys have no character, so they go by their key codes
  QTest::addColumn<int>("keyCodes");
  QTest::addColumn<int>("id");
  QTest::addColumn<int>("button");
  QTest::addColumn<int>("usage");
  QTest::addColumn<QByteArray>("mistake"); // known, and not fixed yet

  const auto windowsCodes = int(KeyCodes::Windows);
  QTest::newRow("Windows keypad 1") << windowsCodes << int(kKeyKP_1) << 0x4F << 0x59 << QByteArray();
  QTest::newRow("Windows keypad 0") << windowsCodes << int(kKeyKP_0) << 0x52 << 0x62 << QByteArray();
  QTest::newRow("Windows keypad 7 as Home") << windowsCodes << int(kKeyKP_Home) << 0x47 << 0x5F << QByteArray();
  QTest::newRow("Windows keypad Enter") << windowsCodes << int(kKeyKP_Enter) << 0x11C << 0x58 << QByteArray();

  // kVK_ANSI_Keypad1 is 0x53, kVK_ANSI_KeypadEnter 0x4C
  const auto macCodes = int(KeyCodes::MacOS);
  const QByteArray macTable("the Mac table has no keypad keys, so they type nothing");
  QTest::newRow("Mac keypad 1") << macCodes << int(kKeyKP_1) << 84 << 0x59 << macTable;
  QTest::newRow("Mac keypad Enter") << macCodes << int(kKeyKP_Enter) << 77 << 0x58 << macTable;

  // X's KP_1 is 87, KP_0 90
  const auto linuxCodes = int(KeyCodes::Linux);
  QTest::newRow("Linux keypad 1") << linuxCodes << int(kKeyKP_1) << 87 << 0x59
                                  << QByteArray("the Linux table has no keypad 1, so it types nothing");
  QTest::newRow("Linux keypad 0") << linuxCodes << int(kKeyKP_0) << 90 << 0x62
                                  << QByteArray("the Linux table has keypad 0 as 1");
}

void BridgeKeyboardTests::keypadKeys()
{
  QFETCH(int, keyCodes);
  QFETCH(int, id);
  QFETCH(int, button);
  QFETCH(int, usage);
  QFETCH(QByteArray, mistake);

  Dongle dongle;
  auto keyboard = makeKeyboard(dongle, static_cast<KeyCodes>(keyCodes), KeyboardLayout::Us);
  tap(keyboard, static_cast<KeyID>(id), 0, static_cast<KeyButton>(button));

  if (!mistake.isEmpty()) {
    QEXPECT_FAIL("", mistake.constData(), Continue);
  }
  QCOMPARE(dongle.reports, pressAndRelease(0, usage));
}

void BridgeKeyboardTests::mediaKeys_data()
{
  QTest::addColumn<int>("id");
  QTest::addColumn<int>("usage"); // the consumer control usage; 0 for none

  QTest::newRow("Mute") << int(kKeyAudioMute) << 0xE2;
  QTest::newRow("Volume Down") << int(kKeyAudioDown) << 0xEA;
  QTest::newRow("Volume Up") << int(kKeyAudioUp) << 0xE9;
  QTest::newRow("Next") << int(kKeyAudioNext) << 0xB5;
  QTest::newRow("Previous") << int(kKeyAudioPrev) << 0xB6;
  QTest::newRow("Stop") << int(kKeyAudioStop) << 0xB7;
  QTest::newRow("Play") << int(kKeyAudioPlay) << 0xCD;
  QTest::newRow("Brightness Down") << int(kKeyBrightnessDown) << 0x70;
  QTest::newRow("Brightness Up") << int(kKeyBrightnessUp) << 0x6F;
  QTest::newRow("Back") << int(kKeyWWWBack) << 0x224;
  QTest::newRow("Forward") << int(kKeyWWWForward) << 0x225;
  QTest::newRow("Refresh") << int(kKeyWWWRefresh) << 0x227;
  QTest::newRow("Stop loading") << int(kKeyWWWStop) << 0x226;
  QTest::newRow("Search") << int(kKeyWWWSearch) << 0x221;
  QTest::newRow("Favorites") << int(kKeyWWWFavorites) << 0x22A;
  QTest::newRow("Home page") << int(kKeyWWWHome) << 0x223;
  QTest::newRow("Mail") << int(kKeyAppMail) << 0x18A;
  QTest::newRow("Media") << int(kKeyAppMedia) << 0x183;
  // not sent
  QTest::newRow("Eject") << int(kKeyEject) << 0;
  QTest::newRow("Sleep") << int(kKeySleep) << 0;
}

void BridgeKeyboardTests::mediaKeys()
{
  QFETCH(int, id);
  QFETCH(int, usage);

  for (const auto keyCodes : kAllKeyCodes) {
    for (const auto layout : kBothLayouts) {
      Dongle dongle;
      auto keyboard = makeKeyboard(dongle, keyCodes, layout);
      keyboard.keyDown(static_cast<KeyID>(id), 0, kVolumeUp);
      QVERIFY(keyboard.isKeyDown(kVolumeUp));
      keyboard.keyUp(kVolumeUp);
      QVERIFY(!keyboard.isKeyDown(kVolumeUp));

      const QStringList expected = usage != 0
                                       ? QStringList{consumerEvent("press", usage), consumerEvent("release", usage)}
                                       : QStringList{report("release", 0, 0)};
      QVERIFY2(
          dongle.all == expected,
          qPrintable(mismatch(QStringLiteral("%1, %2").arg(nameOf(keyCodes), nameOf(layout)), dongle.all, expected))
      );
    }
  }
}

void BridgeKeyboardTests::stateFollowsTheKeys()
{
  // Left Shift and a, by each computer's key codes
  struct Keys
  {
    KeyCodes keyCodes;
    KeyButton shift;
    KeyButton a;
  };
  const Keys computers[] = {{KeyCodes::MacOS, 57, 1}, {KeyCodes::Windows, kLeftShift, 0x1E}, {KeyCodes::Linux, 50, 38}};

  for (const auto &computer : computers) {
    Dongle dongle;
    auto keyboard = makeKeyboard(dongle, computer.keyCodes, KeyboardLayout::Us);
    keyboard.keyDown(kKeyShift_L, KeyModifierShift, computer.shift);
    QVERIFY(keyboard.isKeyDown(computer.shift));
    QCOMPARE(keyboard.activeModifiers(), KeyModifierMask(KeyModifierShift));

    keyboard.keyDown('A', KeyModifierShift, computer.a);
    keyboard.keyDown(kKeyAudioUp, KeyModifierShift, kVolumeUp);
    QVERIFY(keyboard.pressedButtons() == (std::set<KeyButton>{computer.shift, computer.a, kVolumeUp}));

    keyboard.keyUp(computer.a);
    QVERIFY(!keyboard.isKeyDown(computer.a));
    keyboard.keyUp(kVolumeUp);
    QVERIFY(!keyboard.isKeyDown(kVolumeUp));
    QVERIFY(keyboard.pressedButtons() == std::set<KeyButton>{computer.shift});
    keyboard.keyUp(computer.shift);
    QVERIFY(keyboard.pressedButtons().empty());

    // each released once
    keyboard.allKeysUp();
    QCOMPARE(
        dongle.all, (QStringList{
                        report("press", 0x02, 0), report("press", 0x02, 0x04), consumerEvent("press", 0xE9),
                        report("release", 0x02, 0x04), consumerEvent("release", 0xE9), report("release", 0, 0)
                    })
    );
    QCOMPARE(keyboard.activeModifiers(), KeyModifierMask(0));
  }
}

void BridgeKeyboardTests::twoKeysHeld()
{
  // a and s, by each computer's key codes
  const std::tuple<KeyCodes, KeyButton, KeyButton> computers[] = {
      {KeyCodes::MacOS, 1, 2}, {KeyCodes::Windows, 0x1E, 0x1F}, {KeyCodes::Linux, 38, 39}
  };
  for (const auto &[keyCodes, a, s] : computers) {
    for (const auto layout : kBothLayouts) {
      Dongle dongle;
      auto keyboard = makeKeyboard(dongle, keyCodes, layout);
      keyboard.keyDown('a', 0, a);
      keyboard.keyDown('s', 0, s);
      keyboard.keyUp(a);
      keyboard.keyUp(s);

      const QStringList expected = {
          report("press", 0, 0x04), report("press", 0, 0x16), report("release", 0, 0x04), report("release", 0, 0x16)
      };
      QVERIFY2(
          dongle.reports == expected,
          qPrintable(mismatch(QStringLiteral("%1, %2").arg(nameOf(keyCodes), nameOf(layout)), dongle.reports, expected))
      );
    }
  }
}

void BridgeKeyboardTests::keyUpOfKeyNeverPressed_data()
{
  // as when the key went down before the server switched to this screen
  QTest::addColumn<int>("keyCodes");
  QTest::addColumn<int>("button");
  QTest::addColumn<int>("usage");

  QTest::newRow("Mac a") << int(KeyCodes::MacOS) << 1 << 0x04;
  QTest::newRow("Mac Shift") << int(KeyCodes::MacOS) << 57 << 0;
  QTest::newRow("Mac unknown") << int(KeyCodes::MacOS) << int(kUnknownKey) << 0;
  QTest::newRow("Windows a") << int(KeyCodes::Windows) << 0x1E << 0x04;
  QTest::newRow("Windows Left Shift") << int(KeyCodes::Windows) << int(kLeftShift) << 0;
  QTest::newRow("Windows unknown") << int(KeyCodes::Windows) << int(kUnknownKey) << 0;
  QTest::newRow("Linux a") << int(KeyCodes::Linux) << 38 << 0x04;
  QTest::newRow("Linux Shift_L") << int(KeyCodes::Linux) << 50 << 0;
  QTest::newRow("Linux unknown") << int(KeyCodes::Linux) << int(kUnknownKey) << 0;
}

void BridgeKeyboardTests::keyUpOfKeyNeverPressed()
{
  QFETCH(int, keyCodes);
  QFETCH(int, button);
  QFETCH(int, usage);

  for (const auto layout : kBothLayouts) {
    Dongle dongle;
    auto keyboard = makeKeyboard(dongle, static_cast<KeyCodes>(keyCodes), layout);
    keyboard.keyUp(static_cast<KeyButton>(button));
    QCOMPARE(dongle.reports, QStringList{report("release", 0, usage)});
  }
}

void BridgeKeyboardTests::modifiersCombine()
{
  Dongle dongle;
  auto keyboard = makeKeyboard(dongle, KeyCodes::Windows, KeyboardLayout::Us);
  keyboard.keyDown(kKeyShift_L, KeyModifierShift, kLeftShift);
  keyboard.keyDown(kKeyControl_R, KeyModifierShift | KeyModifierControl, kRightControl);
  tap(keyboard, 'a', KeyModifierShift | KeyModifierControl, 0x1E);
  keyboard.keyDown(kKeySuper_L, KeyModifierShift | KeyModifierControl | KeyModifierSuper, 0x15B);
  keyboard.keyUp(0x15B);
  keyboard.keyUp(kLeftShift);
  keyboard.keyDown(kKeySuper_R, KeyModifierControl | KeyModifierSuper, 0x15C);
  tap(keyboard, 'l', KeyModifierControl | KeyModifierSuper, 0x26);
  keyboard.keyUp(0x15C);
  keyboard.keyUp(kRightControl);

  QCOMPARE(
      dongle.reports, QStringList({report("press", 0x02, 0), report("press", 0x12, 0)}) + pressAndRelease(0x12, 0x04) +
                          QStringList(
                              {report("press", 0x1A, 0), report("release", 0x12, 0), report("release", 0x10, 0),
                               report("press", 0x90, 0)}
                          ) +
                          pressAndRelease(0x90, 0x0F) +
                          QStringList({report("release", 0x10, 0), report("release", 0, 0)})
  );
}

void BridgeKeyboardTests::macRightCommandIsRightGui()
{
  // kVK_RightCommand is 0x36
  Dongle dongle;
  auto keyboard = makeKeyboard(dongle, KeyCodes::MacOS, KeyboardLayout::Us);
  keyboard.keyDown(kKeySuper_R, KeyModifierSuper, 55);
  tap(keyboard, 'c', KeyModifierSuper, 9);
  keyboard.keyUp(55);

  QCOMPARE(
      dongle.reports,
      QStringList({report("press", 0x80, 0)}) + pressAndRelease(0x80, 0x06) + QStringList({report("release", 0, 0)})
  );
}

void BridgeKeyboardTests::linuxRightShiftStaysRight()
{
  Dongle dongle;
  auto keyboard = makeKeyboard(dongle, KeyCodes::Linux, KeyboardLayout::Us);
  keyboard.keyDown(kKeyShift_R, KeyModifierShift, 62);
  tap(keyboard, 'A', KeyModifierShift, 38);
  keyboard.keyUp(62);

  QCOMPARE(
      dongle.reports,
      QStringList({report("press", 0x20, 0)}) + pressAndRelease(0x20, 0x04) + QStringList({report("release", 0, 0)})
  );
}

void BridgeKeyboardTests::keyRepeat()
{
  Dongle dongle;
  auto keyboard = makeKeyboard(dongle, KeyCodes::Windows, KeyboardLayout::Us);
  keyboard.keyDown(kKeyShift_R, KeyModifierShift, kRightShift);
  keyboard.keyDown('A', KeyModifierShift, 0x1E);
  QVERIFY(keyboard.keyRepeat('A', KeyModifierShift, 3, 0x1E));
  // none
  QVERIFY(keyboard.keyRepeat('A', KeyModifierShift, 0, 0x1E));
  QVERIFY(keyboard.keyRepeat('A', KeyModifierShift, -1, 0x1E));
  keyboard.keyUp(0x1E);
  // a modifier key, and a key with nothing to press
  QVERIFY(keyboard.keyRepeat(kKeyShift_R, KeyModifierShift, 2, kRightShift));
  QVERIFY(keyboard.keyRepeat(kKeyF25, 0, 2, kUnknownKey));
  keyboard.keyUp(kRightShift);

  QCOMPARE(
      dongle.reports, QStringList({report("press", 0x20, 0), report("press", 0x20, 0x04)}) +
                          pressAndRelease(0x20, 0x04) + pressAndRelease(0x20, 0x04) + pressAndRelease(0x20, 0x04) +
                          QStringList({report("release", 0x20, 0x04)}) + pressAndRelease(0x20, 0) +
                          pressAndRelease(0x20, 0) + QStringList({report("release", 0, 0)})
  );
}

void BridgeKeyboardTests::mediaKeyRepeat()
{
  for (const auto keyCodes : kAllKeyCodes) {
    Dongle dongle;
    auto keyboard = makeKeyboard(dongle, keyCodes, KeyboardLayout::Us);
    keyboard.keyDown(kKeyAudioUp, 0, kVolumeUp);
    QVERIFY(keyboard.keyRepeat(kKeyAudioUp, 0, 2, kVolumeUp));
    QVERIFY(keyboard.keyRepeat(kKeyAudioUp, 0, 0, kVolumeUp));
    keyboard.keyUp(kVolumeUp);

    QVERIFY(dongle.reports.isEmpty());
    QCOMPARE(
        dongle.consumer,
        (QStringList{
            consumerReport("press", 0xE9), consumerReport("press", 0xE9), consumerReport("release", 0xE9),
            consumerReport("press", 0xE9), consumerReport("release", 0xE9), consumerReport("release", 0xE9)
        })
    );
  }
}

void BridgeKeyboardTests::allKeysUpReleasesInOrder()
{
  Dongle dongle;
  auto keyboard = makeKeyboard(dongle, KeyCodes::Windows, KeyboardLayout::Us);
  keyboard.keyDown(kKeyShift_R, KeyModifierShift, kRightShift);
  keyboard.keyDown('A', KeyModifierShift, 0x1E);
  keyboard.keyDown(kKeyAudioUp, KeyModifierShift, kVolumeUp);
  keyboard.allKeysUp();

  // the media keys, then the keys by their key codes, each with the modifier keys still held
  QCOMPARE(
      dongle.all, (QStringList{
                      report("press", 0x20, 0), report("press", 0x20, 0x04), consumerEvent("press", 0xE9),
                      consumerEvent("release", 0xE9), report("release", 0x20, 0x04), report("release", 0, 0)
                  })
  );
  QVERIFY(keyboard.pressedButtons().empty());
  QCOMPARE(keyboard.activeModifiers(), KeyModifierMask(0));

  // and nothing is held after
  dongle.all.clear();
  tap(keyboard, 'a', 0, 0x1E);
  QCOMPARE(dongle.all, pressAndRelease(0, 0x04));
}

void BridgeKeyboardTests::failedSends()
{
  // a repeat stops at the first report that fails
  {
    Dongle dongle;
    auto keyboard = makeKeyboard(dongle, KeyCodes::Windows, KeyboardLayout::Us);
    dongle.failingSend = 2;
    QVERIFY(!keyboard.keyRepeat('a', 0, 3, 0x1E));
    QCOMPARE(dongle.reports, pressAndRelease(0, 0x04));
  }
  {
    Dongle dongle;
    auto keyboard = makeKeyboard(dongle, KeyCodes::Windows, KeyboardLayout::Us);
    dongle.failingSend = 1;
    QVERIFY(!keyboard.keyRepeat('a', 0, 3, 0x1E));
    QCOMPARE(dongle.reports, QStringList{report("press", 0, 0x04)});
  }
  {
    Dongle dongle;
    auto keyboard = makeKeyboard(dongle, KeyCodes::Windows, KeyboardLayout::Us);
    dongle.failingSend = 3;
    QVERIFY(!keyboard.keyRepeat(kKeyAudioUp, 0, 3, kVolumeUp));
    QCOMPARE(
        dongle.consumer,
        (QStringList{consumerReport("press", 0xE9), consumerReport("release", 0xE9), consumerReport("press", 0xE9)})
    );
  }

  // a key that fails to go down is still down, so its key up releases it
  {
    Dongle dongle;
    auto keyboard = makeKeyboard(dongle, KeyCodes::Windows, KeyboardLayout::Us);
    dongle.failingSend = 1;
    keyboard.keyDown('a', 0, 0x1E);
    QVERIFY(keyboard.isKeyDown(0x1E));
    keyboard.keyUp(0x1E);
    QVERIFY(!keyboard.isKeyDown(0x1E));
    QCOMPARE(dongle.reports, pressAndRelease(0, 0x04));
  }

  // a dead key that fails doesn't hold its character back
  {
    Dongle dongle;
    auto keyboard = germanKeyboard(dongle);
    dongle.failingSend = 1;
    tap(keyboard, 0xEA, 0, 0x12); // ê
    QCOMPARE(
        dongle.reports, (QStringList{report("press", 0, 0x35), report("press", 0, 0x08), report("release", 0, 0x08)})
    );
  }
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
                          QStringList({report("release", 0x01, 0), report("release", 0, 0)})
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
                          pressAndRelease(0x40, 0x27) +
                          QStringList({report("release", 0x01, 0), report("release", 0, 0)})
  );
}

void BridgeKeyboardTests::rightControlAndAltAreAltGr()
{
  // Windows takes any Control and Alt as AltGr, here Right Control and Left Alt
  Dongle dongle;
  auto keyboard = germanKeyboard(dongle);
  keyboard.keyDown(kKeyControl_R, KeyModifierControl, kRightControl);
  keyboard.keyDown(kKeyAlt_L, KeyModifierControl | KeyModifierAlt, kLeftAlt);
  tap(keyboard, '@', 0, 0x10);
  keyboard.keyUp(kLeftAlt);
  keyboard.keyUp(kRightControl);

  // and no Right Alt is left once either is up
  QCOMPARE(
      dongle.reports, QStringList({report("press", 0x10, 0), report("press", 0x14, 0)}) + pressAndRelease(0x40, 0x14) +
                          QStringList({report("release", 0x10, 0), report("release", 0, 0)})
  );

  // nor after all keys up
  dongle.reports.clear();
  keyboard.keyDown(kKeyControl_L, KeyModifierControl, kLeftControl);
  keyboard.keyDown(kKeyAlt_L, KeyModifierControl | KeyModifierAlt, kLeftAlt);
  keyboard.keyDown('@', 0, 0x10);
  keyboard.allKeysUp();
  QCOMPARE(
      dongle.reports, QStringList(
                          {report("press", 0x01, 0), report("press", 0x05, 0), report("press", 0x40, 0x14),
                           report("release", 0x40, 0x14), report("release", 0x04, 0), report("release", 0, 0)}
                      )
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

void BridgeKeyboardTests::altShortcutWithControlAndAltHeld()
{
  // the mask has Alt: not AltGr
  Dongle dongle;
  auto keyboard = germanKeyboard(dongle);
  keyboard.keyDown(kKeyControl_L, KeyModifierControl, kLeftControl);
  keyboard.keyDown(kKeyAlt_L, KeyModifierControl | KeyModifierAlt, kLeftAlt);
  keyboard.keyDown('x', KeyModifierAlt, 0x2D);

  QCOMPARE(
      dongle.reports, QStringList({report("press", 0x01, 0), report("press", 0x05, 0), report("press", 0x05, 0x1B)})
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

void BridgeKeyboardTests::keystrokeWithoutKeys()
{
  // a keystroke the server makes, with no modifier key held and no key code: by its character
  Dongle dongle;
  auto keyboard = germanKeyboard(dongle);
  tap(keyboard, 'c', KeyModifierControl, 0);

  QCOMPARE(dongle.reports, QStringList({report("press", 0x01, 0x06), report("release", 0, 0x06)}));
}

void BridgeKeyboardTests::keypadNavigation()
{
  // keypad 7 with Num Lock off: the key, for the device's Num Lock to read
  Dongle dongle;
  auto keyboard = germanKeyboard(dongle);
  tap(keyboard, kKeyKP_Home, 0, 0x47);

  QCOMPARE(dongle.reports, pressAndRelease(0, 0x5F));
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
    QVERIFY2(dongle.reports == c.reports, qPrintable(mismatch(QString::fromUtf8(c.name), dongle.reports, c.reports)));
  }
}

void BridgeKeyboardTests::usInternationalDeadKeys()
{
  const auto keyboardFor = [](Dongle &dongle) {
    return makeKeyboard(dongle, KeyCodes::Windows, KeyboardLayout::SameAsComputer, deadKeysOf(kUsInternational));
  };

  // ' then e: the hook sends é on the e key
  {
    Dongle dongle;
    auto keyboard = keyboardFor(dongle);
    tap(keyboard, 0xE9, 0, 0x12);
    QCOMPARE(dongle.reports, pressAndRelease(0, 0x34) + pressAndRelease(0, 0x08));
  }
  // Shift+' then u
  {
    Dongle dongle;
    auto keyboard = keyboardFor(dongle);
    tap(keyboard, 0xFC, 0, 0x16);
    QCOMPARE(dongle.reports, pressAndRelease(0x02, 0x34) + pressAndRelease(0, 0x18));
  }
  // AltGr+E types é by itself
  {
    Dongle dongle;
    auto keyboard = keyboardFor(dongle);
    pressAltGr(keyboard);
    tap(keyboard, 0xE9, 0, 0x12);
    releaseAltGr(keyboard);
    QCOMPARE(
        dongle.reports, QStringList({report("press", 0x01, 0), report("press", 0x41, 0)}) +
                            pressAndRelease(0x40, 0x08) +
                            QStringList({report("release", 0x01, 0), report("release", 0, 0)})
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
  QCOMPARE(dongle.consumer, QStringList({consumerReport("press", 0xE2), consumerReport("release", 0xE2)}));
}

void BridgeKeyboardTests::onlyWindowsSendsKeysAsPressed()
{
  Dongle dongle;
  QVERIFY(makeKeyboard(dongle, KeyCodes::Windows, KeyboardLayout::SameAsComputer).sendsKeysAsPressed());
  QVERIFY(!makeKeyboard(dongle, KeyCodes::Windows, KeyboardLayout::Us).sendsKeysAsPressed());
  QVERIFY(!makeKeyboard(dongle, KeyCodes::MacOS, KeyboardLayout::SameAsComputer).sendsKeysAsPressed());
  QVERIFY(!makeKeyboard(dongle, KeyCodes::Linux, KeyboardLayout::SameAsComputer).sendsKeysAsPressed());

  // so a Mac and Linux go by the character, as on US: z, on the key that is y on US
  auto onMac = makeKeyboard(dongle, KeyCodes::MacOS, KeyboardLayout::SameAsComputer);
  tap(onMac, 'z', 0, 17);
  auto onLinux = makeKeyboard(dongle, KeyCodes::Linux, KeyboardLayout::SameAsComputer);
  tap(onLinux, 'z', 0, 29);
  QCOMPARE(dongle.reports, pressAndRelease(0, 0x1D) + pressAndRelease(0, 0x1D));
}

void BridgeKeyboardTests::layoutChangeTakesEffect()
{
  Dongle dongle;
  auto keyboard = makeKeyboard(dongle, KeyCodes::Windows, KeyboardLayout::Us);
  tap(keyboard, 'z', 0, 0x15);
  keyboard.setLayout(KeyboardLayout::SameAsComputer);
  tap(keyboard, 'z', 0, 0x15);
  keyboard.setLayout(KeyboardLayout::Us);
  tap(keyboard, 'z', 0, 0x15);

  QCOMPARE(dongle.reports, pressAndRelease(0, 0x1D) + pressAndRelease(0, 0x1C) + pressAndRelease(0, 0x1D));
}

QTEST_MAIN(BridgeKeyboardTests)
