/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#include "BridgeProfileShortcuts.h"

#include "common/Settings.h"
#include "gui/core/BridgeClientConfigManager.h"

#include <QSettings>
#include <QTextStream>

#include <algorithm>

namespace deskflow::gui {

namespace {

// the keys as they are labeled: on macOS, Alt is Option and Super is Command
enum Modifier
{
  kControl = 0x1,
  kAlt = 0x2,
  kShift = 0x4,
  kSuper = 0x8
};

struct Choice
{
  QLatin1StringView value;
  int modifiers;
};

constexpr Choice kChoices[] = {
    {QLatin1StringView("ctrl+alt+shift"), kControl | kAlt | kShift},
    {QLatin1StringView("ctrl+alt"), kControl | kAlt},
    {QLatin1StringView("ctrl+shift"), kControl | kShift},
    {QLatin1StringView("alt+shift"), kAlt | kShift},
    {QLatin1StringView("super+alt"), kSuper | kAlt},
};

// the positions the server's action takes (BridgeProfileAction): a device has at most 6
// profiles, and 2 until more are licensed
constexpr int kMinPosition = 1;
constexpr int kMaxPosition = 6;
constexpr int kDefaultProfileCount = 2;

// 0 for kOff; an unset or unknown value is the first choice
int modifiers(const QString &choice)
{
  if (choice == BridgeProfileShortcuts::kOff) {
    return 0;
  }
  for (const auto &c : kChoices) {
    if (choice == c.value) {
      return c.modifiers;
    }
  }
  return kChoices[0].modifiers;
}

// the names the server's configuration takes.  on macOS, the server reads Alt as Command and
// Super as Option (OSXKeyState::mapDeskflowHotKeyToMac)
QString configModifiers(int modifiers)
{
#if defined(Q_OS_MACOS)
  constexpr auto altName = "Super";
  constexpr auto superName = "Alt";
#else
  constexpr auto altName = "Alt";
  constexpr auto superName = "Super";
#endif
  QStringList names;
  if (modifiers & kControl) {
    names.append(QStringLiteral("Control"));
  }
  if (modifiers & kAlt) {
    names.append(QString::fromLatin1(altName));
  }
  if (modifiers & kShift) {
    names.append(QStringLiteral("Shift"));
  }
  if (modifiers & kSuper) {
    names.append(QString::fromLatin1(superName));
  }
  return names.join(QLatin1Char('+'));
}

QKeySequence keySequence(int modifiers, int position)
{
  // on macOS, Qt's Control modifier is Command and its Meta modifier is Control
#if defined(Q_OS_MACOS)
  constexpr auto controlModifier = Qt::MetaModifier;
  constexpr auto superModifier = Qt::ControlModifier;
#else
  constexpr auto controlModifier = Qt::ControlModifier;
  constexpr auto superModifier = Qt::MetaModifier;
#endif
  Qt::KeyboardModifiers qtModifiers;
  if (modifiers & kControl) {
    qtModifiers |= controlModifier;
  }
  if (modifiers & kAlt) {
    qtModifiers |= Qt::AltModifier;
  }
  if (modifiers & kShift) {
    qtModifiers |= Qt::ShiftModifier;
  }
  if (modifiers & kSuper) {
    qtModifiers |= superModifier;
  }
  return QKeySequence(QKeyCombination(qtModifiers, static_cast<Qt::Key>(Qt::Key_0 + position)));
}

} // namespace

const QString BridgeProfileShortcuts::kOff = QStringLiteral("off");
int BridgeProfileShortcuts::s_writtenCount = 0;

QStringList BridgeProfileShortcuts::choices()
{
  QStringList values;
  for (const auto &c : kChoices) {
    values.append(c.value);
  }
  return values;
}

QString BridgeProfileShortcuts::choice()
{
  const QString value = Settings::value(Settings::Bridge::ProfileShortcuts).toString();
  return (value == kOff || choices().contains(value)) ? value : QString(kChoices[0].value);
}

void BridgeProfileShortcuts::setChoice(const QString &choice)
{
  Settings::setValue(Settings::Bridge::ProfileShortcuts, choice);
}

QString BridgeProfileShortcuts::describe(const QString &choice)
{
  // the first shortcut and the range of digits
  const QString first = keySequence(modifiers(choice), kMinPosition).toString(QKeySequence::NativeText);
  const int last = std::max(count(), kDefaultProfileCount);
  return QStringLiteral("%1…%2").arg(first).arg(last);
}

QKeySequence BridgeProfileShortcuts::shortcut(int position)
{
  const int chosen = modifiers(choice());
  if (chosen == 0 || position < kMinPosition || position > s_writtenCount) {
    return {};
  }
  return keySequence(chosen, position);
}

int BridgeProfileShortcuts::count()
{
  int most = 0;
  for (const QString &configPath : BridgeClientConfigManager::getAllConfigFiles()) {
    // read when the device connects; one that hasn't since has the default
    const QSettings config(configPath, QSettings::IniFormat);
    most = std::max(most, config.value(Settings::Bridge::ProfileCount, kDefaultProfileCount).toInt());
  }
  return std::clamp(most, 0, kMaxPosition);
}

void BridgeProfileShortcuts::writeHotkeys(QTextStream &stream)
{
  // only as many as there are profiles: the others would just take the keys from other
  // applications (all of them, without bridge clients)
  const int chosen = modifiers(choice());
  s_writtenCount = (chosen == 0) ? 0 : count();

  const QString names = configModifiers(chosen);
  for (int position = kMinPosition; position <= s_writtenCount; ++position) {
    stream << QStringLiteral("\tkeystroke(%1+%2) = switchBridgeProfile(%2)\n").arg(names).arg(position);
  }
}

} // namespace deskflow::gui
