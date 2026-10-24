/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#pragma once

#include <QKeySequence>
#include <QString>
#include <QStringList>

class QTextStream;

namespace deskflow::gui {

/**
 * @brief The keyboard shortcuts that switch a bridge client's device to one of its profiles
 *
 * A chosen set of modifiers plus a digit, the profile's position on the device counting
 * from 1.  They are hotkeys of the server (server/BridgeProfileAction), added to the server
 * configuration the GUI writes, so they work whichever computer the cursor is on.  The
 * server passes a pressed one on to the GUI, which picks the bridge client to switch.
 */
class BridgeProfileShortcuts
{
public:
  /// the modifier sets to choose from, as stored in Settings::Bridge::ProfileShortcuts
  static QStringList choices();
  static const QString kOff;

  /// the chosen modifier set: one of choices(), or kOff
  static QString choice();
  static void setChoice(const QString &choice);

  /// a choice's shortcuts as the user sees them, e.g. "Ctrl+Alt+Shift+1…2"
  static QString describe(const QString &choice);

  /// the shortcut for a profile position, or an empty sequence when the server doesn't have
  /// one (it registers its hotkeys when it starts)
  static QKeySequence shortcut(int position);

  /// how many profiles the shortcuts cover: as many as the bridge clients' devices last
  /// reported, at most (0 without bridge clients)
  static int count();

  /// add the hotkeys to the options section of a server configuration, when there are
  /// bridge clients to switch
  static void writeHotkeys(QTextStream &stream);

private:
  static int s_writtenCount; // how many writeHotkeys() last wrote
};

} // namespace deskflow::gui
