/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#pragma once

#include "DisplayLayout.h"

#include <QHash>
#include <QPoint>
#include <QSize>
#include <QString>

#include <optional>

class ComputerList;
class QTextStream;
class ServerConfig;

namespace deskflow::gui {

struct StoredLayout
{
  bool saved = false;  ///< a layout was saved, so the grid no longer seeds it
  bool active = false; ///< the links come from the layout, not the grid
  QList<LayoutClient> clients;
  QHash<QString, QSize> sizes; ///< the last size seen for each computer, even before a layout exists
};

/**
 * @brief The to-scale layout on disk, kept apart from the settings
 *
 * It lives in its own file so upstream's settings migrations and key whitelist never touch it, and
 * so that deleting the file brings back the grid's behaviour. The grid itself is never rewritten.
 */
class DisplayLayoutStore
{
public:
  static QString filePath();

  /// an empty layout when the file is missing, unreadable, or from a newer version
  static StoredLayout load();
  static bool save(const StoredLayout &layout);

  static bool isActive();

  /**
   * @brief The layout for these computers
   *
   * Computers that are gone drop out, a new one takes its default place, and with nothing saved yet
   * the layout comes from the grid.
   */
  static LayoutState buildState(const ComputerList &computers, const QString &serverName, const StoredLayout &stored);

  /// adds the layout's links to a config, or nothing while the layout isn't active
  static void writeLinks(QTextStream &stream, const ServerConfig &config);

  /// the links a config would get now
  static QString linkText(const ServerConfig &config);

  /// the links as last written for the core
  static QString writtenLinks();

  /// false if the core doesn't read these links, so changing them can't be applied by a restart
  static bool coreUsesLinks(bool serverMode);

  static void rememberSize(const QString &name, const QSize &size);

  /// the size of a client the core has just heard from, as it sends it
  static void rememberShape(const QString &message);

  static void rename(const QString &oldName, const QString &newName);

  /// the same for the placements saved in Bridge profiles, which name their anchor too
  static void renameInProfiles(const QString &oldName, const QString &newName);

  /// a placement as text for a bridge profile, and back
  static QString encodePlacement(const std::optional<LayoutPlacement> &placement);
  static std::optional<std::optional<LayoutPlacement>> decodePlacement(const QString &text);

  /// the side a grid offset means, for profiles saved before the layout existed
  static std::optional<LayoutPlacement> placementFromGridOffset(const QPoint &offset);

  static std::optional<LayoutPlacement> placementOf(const QString &name);

  /// moves a computer, returning true if that changed anything
  static bool applyPlacement(const QString &name, const std::optional<LayoutPlacement> &placement);

  /**
   * @brief Moves a bonded computer to where its bridge profile keeps it
   *
   * A profile saved before the layout existed only has a grid offset, which says a side at most.
   *
   * @return true if that changed anything
   */
  static bool applyProfile(const QString &configPath, int profile, const QString &name);

  /// keeps a computer's place in its bridge profile
  static void rememberProfile(const QString &configPath, int profile, const QString &name);
};

} // namespace deskflow::gui
