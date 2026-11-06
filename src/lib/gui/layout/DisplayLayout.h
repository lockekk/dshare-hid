/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#pragma once

#include <QHash>
#include <QList>
#include <QRect>
#include <QSet>
#include <QSize>
#include <QString>
#include <QStringList>

#include <optional>

namespace deskflow::gui {

enum class LayoutSide
{
  Left,
  Right,
  Up,
  Down
};

/**
 * @brief Where a client sits: on one side of the server's desktop, or of another client
 *
 * Positions aren't stored. They are worked out from these, outward from the server, so a client
 * keeps its edge when a resolution changes.
 */
struct LayoutPlacement
{
  QString anchor; ///< the computer it sits on, empty for the server
  LayoutSide side = LayoutSide::Left;
  int offset = 0;         ///< along the side, from the start of the anchor's side
  bool wholeEdge = false; ///< cross anywhere on the side: set for a computer seeded from the grid, as it crossed there

  bool operator==(const LayoutPlacement &) const = default;
};

struct LayoutClient
{
  QString name;
  QSize size;
  bool sizeKnown = false;
  bool bridge = false;
  std::optional<LayoutPlacement> placement; ///< none: not linked, reachable by hotkey only

  bool operator==(const LayoutClient &) const = default;
};

struct LayoutState
{
  QString serverName;
  QList<QRect> monitors;
  QList<LayoutClient> clients;
  bool freeEdges = false; ///< dock on any free display edge, not only the outside of the box
};

/**
 * @brief A stretch of one side of the desktop where the cursor can leave
 */
struct LayoutSegment
{
  int start = 0; ///< along the side
  int end = 0;   ///< along the side, exclusive
  int at = 0;    ///< position of the display edge

  bool operator==(const LayoutSegment &) const = default;
};

struct LayoutRange
{
  double start = 0; ///< percent of the side
  double end = 100;

  bool operator==(const LayoutRange &) const = default;
};

struct LayoutLink
{
  LayoutSide side = LayoutSide::Left;
  LayoutRange range;
  QString target;
  LayoutRange targetRange;
};

struct LayoutLinks
{
  QString source;
  QList<LayoutLink> links;
};

struct SolvedLayout
{
  QRect box;                   ///< the server's desktop as the core sees it
  QHash<QString, QRect> rects; ///< every client that has a place
  QStringList parked;          ///< placed clients that don't fit right now
};

struct LayoutSnap
{
  QString anchor;
  LayoutSide side = LayoutSide::Left;
  int offset = 0;
  QRect rect;
};

/**
 * @brief The to-scale layout's geometry, free of any UI or file
 *
 * The core sees the server as one box and links computers by percent ranges along its sides, so
 * this class turns displays and placements into those ranges. A computer pushed in from outside
 * stops at the first display edge under it, which is what makes a notch need no special case.
 */
class DisplayLayout
{
public:
  /// the shortest crossing worth linking, in desktop units
  static constexpr int kMinCrossing = 48;

  static QRect boundingBox(const QList<QRect> &monitors);

  /// where a side of a rectangle lies
  static int edgeOf(const QRect &rect, LayoutSide side);

  /**
   * @brief Where the cursor can leave on one side
   *
   * Each stretch of the side belongs to the first display seen from outside. Without free edges
   * only the stretches on the bounding box count, which is what the core can do unaided.
   */
  static QList<LayoutSegment> reach(const QList<QRect> &monitors, const QRect &box, LayoutSide side, bool freeEdges);

  /**
   * @brief Works out every position, walking out from the server
   *
   * A client that no longer fits is parked and keeps its placement, so it returns when it fits.
   */
  static SolvedLayout solve(const LayoutState &state);

  /// true if another computer touches the side this client sits on
  static bool sideShared(const LayoutState &state, const SolvedLayout &solved, const LayoutClient &client);

  /// a whole-edge link needs both faces of the link to itself, and a client flush with the box
  static bool wholeEdgeAllowed(const LayoutState &state, const SolvedLayout &solved, const LayoutClient &client);

  /// the links between everything that touches, the server's first
  static QList<LayoutLinks> links(const LayoutState &state, const SolvedLayout &solved);

  /**
   * @brief The inside of a config's links section
   *
   * Every range is written so that the core's parser reads it in any locale. See formatPercent().
   */
  static QString linkText(const LayoutState &state, const SolvedLayout &solved);

  /**
   * @brief A percent as text the core reads in any locale
   *
   * The core parses ranges with strtod(), which follows LC_NUMERIC, and QApplication turns that on.
   * "31.8056" fails under a comma locale and stops the server starting, while "318056e-4" has no
   * decimal point to get wrong.
   */
  static QString formatPercent(double percent);

  /// the nearest free spot for a dragged computer, with magnets for the ends and middle of an edge
  static std::optional<LayoutSnap>
  snap(const LayoutState &state, const SolvedLayout &solved, const QString &name, const QRect &free, int tolerance);

  /// a computer and everything that hangs off it
  static QSet<QString> subtree(const LayoutState &state, const QString &name);

  /// moves a computer to Not linked, with whatever reached the server through it
  static void unlink(LayoutState &state, const QString &name);

  static void removeClient(LayoutState &state, const QString &name);
  static void renameClient(LayoutState &state, const QString &oldName, const QString &newName);

  /**
   * @brief Where a newly added computer goes
   *
   * A network client takes the first free side, in the order left, right, top and bottom. A bridge
   * client has no display to fall back on, so it starts on the shelf, away from an accidental edge.
   */
  static std::optional<LayoutPlacement> defaultPlacement(const LayoutState &state, const LayoutClient &client);

  /**
   * @brief Places the clients where the classic grid has them
   *
   * Grid neighbours link whole edges, so the links match the grid's. A cell that reaches the server
   * through no neighbour goes to the shelf.
   *
   * @param cells The grid's computer names, row by row, empty for an empty cell
   */
  static void seedFromGrid(LayoutState &state, const QStringList &cells, int columns);
};

} // namespace deskflow::gui
