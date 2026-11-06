/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "DisplayLayout.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <map>

namespace deskflow::gui {

namespace {

using enum LayoutSide;

constexpr std::array kSides = {Left, Right, Up, Down};

struct Span
{
  int start = 0;
  int end = 0;

  int length() const
  {
    return end - start;
  }
};

struct Spot
{
  QRect rect;
  int along = 0;
};

LayoutSide opposite(LayoutSide side)
{
  constexpr std::array kOpposite = {Right, Left, Down, Up};
  return kOpposite.at(static_cast<std::size_t>(side));
}

bool runsDown(LayoutSide side)
{
  return side == Left || side == Right;
}

/// up and left are where smaller coordinates are further out
bool outwardIsSmaller(LayoutSide side)
{
  return side == Left || side == Up;
}

Span along(const QRect &rect, LayoutSide side)
{
  return runsDown(side) ? Span{rect.y(), rect.y() + rect.height()} : Span{rect.x(), rect.x() + rect.width()};
}

int edge(const QRect &rect, LayoutSide side)
{
  const int start = runsDown(side) ? rect.x() : rect.y();
  const int length = runsDown(side) ? rect.width() : rect.height();
  return outwardIsSmaller(side) ? start : start + length;
}

Span clip(const Span &first, const Span &second)
{
  return {std::max(first.start, second.start), std::min(first.end, second.end)};
}

int lengthAlong(const QSize &size, LayoutSide side)
{
  return runsDown(side) ? size.height() : size.width();
}

QRect flush(int at, LayoutSide side, const QSize &size, int position)
{
  const int across = runsDown(side) ? size.width() : size.height();
  const int start = outwardIsSmaller(side) ? at - across : at;
  return runsDown(side) ? QRect(QPoint(start, position), size) : QRect(QPoint(position, start), size);
}

QList<LayoutSegment> clientSegments(const QRect &rect, LayoutSide side)
{
  const auto span = along(rect, side);
  return {{span.start, span.end, edge(rect, side)}};
}

/// a computer pushed in from outside stops at the first edge under it, and there is always one
int depth(const QList<LayoutSegment> &segments, LayoutSide side, int position, int length)
{
  QList<int> under;
  for (const auto &segment : segments) {
    if (segment.start < position + length && segment.end > position)
      under.append(segment.at);
  }
  return outwardIsSmaller(side) ? *std::ranges::min_element(under) : *std::ranges::max_element(under);
}

bool collides(const QRect &rect, const QList<QRect> &obstacles)
{
  return std::ranges::any_of(obstacles, [&rect](const QRect &obstacle) { return rect.intersects(obstacle); });
}

/// the free spot on this side closest to `desired` that still shares a crossing with an edge
std::optional<Spot>
fit(LayoutSide side, const QList<LayoutSegment> &segments, const QSize &size, int desired,
    const QList<QRect> &obstacles)
{
  const int length = lengthAlong(size, side);
  QList<int> tries;
  for (const auto &segment : segments) {
    const int need = std::min({DisplayLayout::kMinCrossing, length, segment.end - segment.start});
    const int low = segment.start - length + need;
    const int high = segment.end - need;
    const auto clamp = [low, high](int value) { return std::clamp(value, low, high); };
    tries << clamp(desired) << clamp(segment.start) << clamp(segment.end - length);
    for (const auto &obstacle : obstacles) {
      const auto span = along(obstacle, side);
      tries << clamp(span.start - length) << clamp(span.end);
    }
  }
  std::ranges::stable_sort(tries, [desired](int first, int second) {
    return std::abs(first - desired) < std::abs(second - desired);
  });

  for (const int position : std::as_const(tries)) {
    const int at = depth(segments, side, position, length);
    int touching = 0;
    for (const auto &segment : segments) {
      if (segment.at == at)
        touching += std::max(0, clip({segment.start, segment.end}, {position, position + length}).length());
    }
    if (touching < std::min(DisplayLayout::kMinCrossing, length))
      continue;
    const auto rect = flush(at, side, size, position);
    if (!collides(rect, obstacles))
      return Spot{rect, position};
  }
  return std::nullopt;
}

QList<QRect> blockers(const LayoutState &state, const QRect &box)
{
  return state.freeEdges ? state.monitors : QList<QRect>{box};
}

bool touches(const QRect &rect, LayoutSide side, const QRect &other)
{
  return edge(rect, side) == edge(other, opposite(side)) && clip(along(rect, side), along(other, side)).length() > 0;
}

/// true if a client other than these two touches this side
bool clientTouches(
    const LayoutState &state, const SolvedLayout &solved, const QRect &rect, LayoutSide side, const QString &first,
    const QString &second
)
{
  return std::ranges::any_of(state.clients, [&](const LayoutClient &client) {
    return client.name != first && client.name != second && solved.rects.contains(client.name) &&
           touches(rect, side, solved.rects.value(client.name));
  });
}

/// true if a free edge of the server's displays touches this side
bool serverTouches(const LayoutState &state, const SolvedLayout &solved, const QRect &rect, LayoutSide side)
{
  const auto span = along(rect, side);
  const auto segments = DisplayLayout::reach(state.monitors, solved.box, opposite(side), state.freeEdges);
  return std::ranges::any_of(segments, [&](const LayoutSegment &segment) {
    return segment.at == edge(rect, side) && clip({segment.start, segment.end}, span).length() > 0;
  });
}

/// true if anything but its anchor touches the side of a client that faces the anchor
bool facingSideShared(const LayoutState &state, const SolvedLayout &solved, const LayoutClient &client)
{
  const auto &placement = *client.placement;
  const auto rect = solved.rects.value(client.name);
  const auto facing = opposite(placement.side);
  if (clientTouches(state, solved, rect, facing, client.name, placement.anchor))
    return true;
  return !placement.anchor.isEmpty() && serverTouches(state, solved, rect, facing);
}

bool usesWholeEdge(const LayoutState &state, const SolvedLayout &solved, const LayoutClient &client)
{
  return client.placement->wholeEdge && DisplayLayout::wholeEdgeAllowed(state, solved, client);
}

LayoutRange percent(const Span &part, const Span &whole)
{
  return {
      static_cast<double>(part.start - whole.start) / whole.length() * 100,
      static_cast<double>(part.end - whole.start) / whole.length() * 100
  };
}

QString sideName(LayoutSide side)
{
  constexpr std::array kNames = {"left", "right", "up", "down"};
  return QString::fromLatin1(kNames.at(static_cast<std::size_t>(side)));
}

QString formatRange(const LayoutRange &range)
{
  if (range == LayoutRange{})
    return {};
  return QStringLiteral("(%1,%2)").arg(
      DisplayLayout::formatPercent(range.start), DisplayLayout::formatPercent(range.end)
  );
}

class LinkCollector
{
public:
  void
  add(const QString &source, LayoutSide side, const LayoutRange &range, const QString &target,
      const LayoutRange &targetRange)
  {
    m_links[source].append({side, range, target, targetRange});
    m_links[target].append({opposite(side), targetRange, source, range});
  }

  QList<LayoutLinks> sorted(const LayoutState &state)
  {
    QList<LayoutLinks> blocks;
    const auto append = [this, &blocks](const QString &name) {
      const auto found = m_links.find(name);
      if (found == m_links.end())
        return;
      auto links = found->second;
      std::ranges::stable_sort(links, [](const LayoutLink &first, const LayoutLink &second) {
        if (first.side != second.side)
          return first.side < second.side;
        return first.range.start < second.range.start;
      });
      blocks.append({name, links});
    };
    append(state.serverName);
    for (const auto &client : state.clients)
      append(client.name);
    return blocks;
  }

private:
  std::map<QString, QList<LayoutLink>> m_links;
};

} // namespace

QRect DisplayLayout::boundingBox(const QList<QRect> &monitors)
{
  QRect box;
  for (const auto &monitor : monitors)
    box = box.isNull() ? monitor : box.united(monitor);
  return box;
}

int DisplayLayout::edgeOf(const QRect &rect, LayoutSide side)
{
  return edge(rect, side);
}

QList<LayoutSegment>
DisplayLayout::reach(const QList<QRect> &monitors, const QRect &box, LayoutSide side, bool freeEdges)
{
  QList<int> cuts;
  for (const auto &monitor : monitors) {
    const auto span = along(monitor, side);
    cuts << span.start << span.end;
  }
  std::ranges::sort(cuts);
  cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());

  const int outer = edge(box, side);
  QList<LayoutSegment> segments;
  for (qsizetype i = 1; i < cuts.size(); ++i) {
    const int start = cuts.at(i - 1);
    const int end = cuts.at(i);
    std::optional<int> at;
    for (const auto &monitor : monitors) {
      const auto span = along(monitor, side);
      if (span.start > start || span.end < end)
        continue;
      const int position = edge(monitor, side);
      if (!at.has_value())
        at = position;
      else
        at = outwardIsSmaller(side) ? std::min(*at, position) : std::max(*at, position);
    }
    if (!at.has_value() || (!freeEdges && *at != outer))
      continue;
    if (!segments.isEmpty() && segments.last().at == *at && segments.last().end == start)
      segments.last().end = end;
    else
      segments.append({start, end, *at});
  }
  return segments;
}

SolvedLayout DisplayLayout::solve(const LayoutState &state)
{
  SolvedLayout solved;
  solved.box = boundingBox(state.monitors);

  QList<const LayoutClient *> pending;
  for (const auto &client : state.clients) {
    if (client.placement.has_value())
      pending.append(&client);
  }
  std::ranges::stable_sort(pending, [](const LayoutClient *first, const LayoutClient *second) {
    return first->placement->anchor.isEmpty() && !second->placement->anchor.isEmpty();
  });

  auto obstacles = blockers(state, solved.box);
  for (bool moved = true; moved;) {
    moved = false;
    for (auto it = pending.begin(); it != pending.end();) {
      const auto *client = *it;
      const auto &placement = *client->placement;
      if (!placement.anchor.isEmpty() && !solved.rects.contains(placement.anchor)) {
        ++it;
        continue;
      }
      it = pending.erase(it);
      moved = true;

      const bool onServer = placement.anchor.isEmpty();
      const auto parent = onServer ? solved.box : solved.rects.value(placement.anchor);
      const auto segments = onServer ? reach(state.monitors, solved.box, placement.side, state.freeEdges)
                                     : clientSegments(parent, placement.side);
      std::optional<Spot> spot;
      if (!client->size.isEmpty())
        spot =
            fit(placement.side, segments, client->size, along(parent, placement.side).start + placement.offset,
                obstacles);
      if (spot.has_value()) {
        solved.rects.insert(client->name, spot->rect);
        obstacles.append(spot->rect);
      } else {
        solved.parked.append(client->name);
      }
    }
  }

  // what's left waits for an anchor that never came: it was parked, removed, or part of a loop
  for (const auto *client : std::as_const(pending))
    solved.parked.append(client->name);
  return solved;
}

bool DisplayLayout::sideShared(const LayoutState &state, const SolvedLayout &solved, const LayoutClient &client)
{
  const auto &placement = *client.placement;
  if (!placement.anchor.isEmpty()) {
    const auto parent = solved.rects.value(placement.anchor);
    return clientTouches(state, solved, parent, placement.side, client.name, placement.anchor) ||
           serverTouches(state, solved, parent, placement.side);
  }

  const auto segments = reach(state.monitors, solved.box, placement.side, state.freeEdges);
  return std::ranges::any_of(state.clients, [&](const LayoutClient &other) {
    if (other.name == client.name || !solved.rects.contains(other.name))
      return false;
    const auto rect = solved.rects.value(other.name);
    return std::ranges::any_of(segments, [&](const LayoutSegment &segment) {
      return edge(rect, opposite(placement.side)) == segment.at &&
             clip({segment.start, segment.end}, along(rect, placement.side)).length() > 0;
    });
  });
}

bool DisplayLayout::wholeEdgeAllowed(const LayoutState &state, const SolvedLayout &solved, const LayoutClient &client)
{
  // a whole-edge link covers both faces, and the core refuses a config whose ranges overlap
  if (sideShared(state, solved, client) || facingSideShared(state, solved, client))
    return false;

  const auto &placement = *client.placement;
  return !placement.anchor.isEmpty() ||
         edge(solved.rects.value(client.name), opposite(placement.side)) == edge(solved.box, placement.side);
}

QList<LayoutLinks> DisplayLayout::links(const LayoutState &state, const SolvedLayout &solved)
{
  LinkCollector collector;
  QList<const LayoutClient *> placed;
  for (const auto &client : state.clients) {
    if (solved.rects.contains(client.name))
      placed.append(&client);
  }

  for (const auto *client : std::as_const(placed)) {
    const auto rect = solved.rects.value(client->name);
    for (const auto side : kSides) {
      if (client->placement->anchor.isEmpty() && client->placement->side == side &&
          usesWholeEdge(state, solved, *client)) {
        collector.add(state.serverName, side, {}, client->name, {});
        continue;
      }
      const auto span = along(rect, side);
      for (const auto &segment : reach(state.monitors, solved.box, side, state.freeEdges)) {
        if (edge(rect, opposite(side)) != segment.at)
          continue;
        const auto part = clip({segment.start, segment.end}, span);
        if (part.length() >= std::min(kMinCrossing, span.length()))
          collector.add(
              state.serverName, side, percent(part, along(solved.box, side)), client->name, percent(part, span)
          );
      }
    }
  }

  for (qsizetype i = 0; i < placed.size(); ++i) {
    for (qsizetype j = i + 1; j < placed.size(); ++j) {
      const auto *first = placed.at(i);
      const auto *second = placed.at(j);
      const auto firstRect = solved.rects.value(first->name);
      const auto secondRect = solved.rects.value(second->name);
      for (const auto side : kSides) {
        if (edge(firstRect, side) != edge(secondRect, opposite(side)))
          continue;
        const auto firstSpan = along(firstRect, side);
        const auto secondSpan = along(secondRect, side);
        const auto part = clip(firstSpan, secondSpan);
        if (part.length() <= 0 || part.length() < std::min({kMinCrossing, firstSpan.length(), secondSpan.length()}))
          continue;
        // two computers touch on one side only, so an anchor's side can't be any other
        const LayoutClient *child = nullptr;
        if (second->placement->anchor == first->name)
          child = second;
        else if (first->placement->anchor == second->name)
          child = first;
        if (child != nullptr && usesWholeEdge(state, solved, *child))
          collector.add(first->name, side, {}, second->name, {});
        else
          collector.add(first->name, side, percent(part, firstSpan), second->name, percent(part, secondSpan));
      }
    }
  }
  return collector.sorted(state);
}

QString DisplayLayout::formatPercent(double percent)
{
  const auto scaled = static_cast<long long>(std::llround(percent * 10000));
  if (scaled % 10000 == 0)
    return QString::number(scaled / 10000);
  return QStringLiteral("%1e-4").arg(scaled);
}

QString DisplayLayout::linkText(const LayoutState &state, const SolvedLayout &solved)
{
  QString text;
  const auto blocks = links(state, solved);
  for (const auto &block : blocks) {
    text += QStringLiteral("\t%1:\n").arg(block.source);
    for (const auto &link : block.links) {
      text += QStringLiteral("\t\t%1%2 = %3%4\n")
                  .arg(sideName(link.side), formatRange(link.range), link.target, formatRange(link.targetRange));
    }
  }
  return text;
}

std::optional<LayoutSnap> DisplayLayout::snap(
    const LayoutState &state, const SolvedLayout &solved, const QString &name, const QRect &free, int tolerance
)
{
  const auto moving = subtree(state, name);
  auto obstacles = blockers(state, solved.box);
  QList<std::pair<QString, QRect>> placed;
  for (const auto &client : state.clients) {
    if (solved.rects.contains(client.name) && !moving.contains(client.name)) {
      placed.append({client.name, solved.rects.value(client.name)});
      obstacles.append(solved.rects.value(client.name));
    }
  }

  std::optional<LayoutSnap> best;
  double bestDistance = 0;
  const auto consider = [&](const QString &anchor, const QRect &parent, LayoutSide side,
                            const QList<LayoutSegment> &segments) {
    const int length = lengthAlong(free.size(), side);
    const int position = runsDown(side) ? free.y() : free.x();
    int desired = position;
    for (const auto &segment : segments) {
      for (const int magnet : {segment.start, segment.end - length, (segment.start + segment.end - length) / 2}) {
        if (std::abs(position - magnet) < tolerance)
          desired = magnet;
      }
    }
    const auto spot = fit(side, segments, free.size(), desired, obstacles);
    if (!spot.has_value())
      return;
    const auto delta = QPointF(spot->rect.center() - free.center());
    const double distance = std::hypot(delta.x(), delta.y());
    if (!best.has_value() || distance < bestDistance) {
      bestDistance = distance;
      best = LayoutSnap{anchor, side, spot->along - along(parent, side).start, spot->rect};
    }
  };

  for (const auto side : kSides) {
    consider({}, solved.box, side, reach(state.monitors, solved.box, side, state.freeEdges));
    for (const auto &[anchor, rect] : std::as_const(placed))
      consider(anchor, rect, side, clientSegments(rect, side));
  }
  return best;
}

QSet<QString> DisplayLayout::subtree(const LayoutState &state, const QString &name)
{
  QSet<QString> names = {name};
  for (bool grew = true; grew;) {
    grew = false;
    for (const auto &client : state.clients) {
      if (client.placement.has_value() && names.contains(client.placement->anchor) && !names.contains(client.name)) {
        names.insert(client.name);
        grew = true;
      }
    }
  }
  return names;
}

void DisplayLayout::unlink(LayoutState &state, const QString &name)
{
  const auto names = subtree(state, name);
  for (auto &client : state.clients) {
    if (names.contains(client.name))
      client.placement.reset();
  }
}

void DisplayLayout::removeClient(LayoutState &state, const QString &name)
{
  unlink(state, name);
  state.clients.removeIf([&name](const LayoutClient &client) { return client.name == name; });
}

void DisplayLayout::renameClient(LayoutState &state, const QString &oldName, const QString &newName)
{
  for (auto &client : state.clients) {
    if (client.name == oldName)
      client.name = newName;
    if (client.placement.has_value() && client.placement->anchor == oldName)
      client.placement->anchor = newName;
  }
}

std::optional<LayoutPlacement> DisplayLayout::defaultPlacement(const LayoutState &state, const LayoutClient &client)
{
  if (client.bridge || client.size.isEmpty())
    return std::nullopt;

  const auto solved = solve(state);
  auto obstacles = blockers(state, solved.box);
  for (const auto &other : state.clients) {
    if (solved.rects.contains(other.name))
      obstacles.append(solved.rects.value(other.name));
  }

  const auto free = [&](LayoutSide side) {
    return std::ranges::none_of(state.clients, [&](const LayoutClient &other) {
      return other.placement.has_value() && other.placement->anchor.isEmpty() && other.placement->side == side &&
             solved.rects.contains(other.name);
    });
  };

  for (const bool emptySidesOnly : {true, false}) {
    for (const auto side : kSides) {
      if (emptySidesOnly && !free(side))
        continue;
      const auto segments = reach(state.monitors, solved.box, side, state.freeEdges);
      const auto span = along(solved.box, side);
      const int desired = span.start + (span.length() - lengthAlong(client.size, side)) / 2;
      const auto spot = fit(side, segments, client.size, desired, obstacles);
      if (spot.has_value())
        return LayoutPlacement{{}, side, spot->along - span.start, false};
    }
  }
  return std::nullopt;
}

void DisplayLayout::seedFromGrid(LayoutState &state, const QStringList &cells, int columns)
{
  for (auto &client : state.clients)
    client.placement.reset();

  const auto serverCell = cells.indexOf(state.serverName);
  if (serverCell < 0 || columns <= 0)
    return;

  const auto box = boundingBox(state.monitors);
  QHash<QString, QSize> sizes;
  for (const auto &client : std::as_const(state.clients))
    sizes.insert(client.name, client.size);

  const auto centred = [&](const QString &parent, LayoutSide side, const QSize &size) {
    const int parentLength = parent.isEmpty() ? along(box, side).length() : lengthAlong(sizes.value(parent), side);
    return (parentLength - lengthAlong(size, side)) / 2;
  };

  QList<qsizetype> queue = {serverCell};
  QSet<qsizetype> visited = {serverCell};
  for (qsizetype next = 0; next < queue.size(); ++next) {
    const auto cell = queue.at(next);
    const auto column = cell % columns;
    const auto parent = cell == serverCell ? QString() : cells.at(cell);
    const std::array<std::pair<LayoutSide, qsizetype>, 4> neighbours = {{
        {Left, column > 0 ? cell - 1 : -1},
        {Right, column < columns - 1 ? cell + 1 : -1},
        {Up, cell - columns},
        {Down, cell + columns},
    }};
    for (const auto &[side, neighbour] : neighbours) {
      if (neighbour < 0 || neighbour >= cells.size() || visited.contains(neighbour) || cells.at(neighbour).isEmpty())
        continue;
      const auto found = std::ranges::find(state.clients, cells.at(neighbour), &LayoutClient::name);
      if (found == state.clients.end())
        continue;
      found->placement = LayoutPlacement{parent, side, centred(parent, side, found->size), true};
      visited.insert(neighbour);
      queue.append(neighbour);
    }
  }
}

} // namespace deskflow::gui
