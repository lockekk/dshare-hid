/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#include "DisplayLayoutView.h"

#include "DisplayMonitors.h"
#include "common/Settings.h"
#include "gui/ComputerSetupModel.h"
#include "gui/core/BridgeClientManager.h"
#include "gui/dialogs/ComputerSettingsDialog.h"
#include "gui/widgets/ComputerSetupView.h"
#include "gui/widgets/RemoveComputerWidget.h"
#include "gui/widgets/SettingsDialogButtonBox.h"

#include <QBoxLayout>
#include <QContextMenuEvent>
#include <QDataStream>
#include <QDialog>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFontMetricsF>
#include <QHelpEvent>
#include <QIODevice>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QTextLayout>
#include <QTextOption>
#include <QToolTip>

#include <algorithm>

namespace deskflow::gui {

namespace {

constexpr int kMapMargin = 18;
constexpr int kDragThreshold = 4;
constexpr int kMagnetPixels = 16;
constexpr int kChipHeight = 28;
constexpr int kChipGap = 8;
constexpr int kShelfHeader = 26;
constexpr int kShelfPadding = 10;
constexpr int kNudge = 20;
constexpr int kNudgeLarge = 100;
constexpr double kCornerRadius = 4;
constexpr double kLabelInset = 6;  ///< keeps a label clear of the lines drawn on a rectangle's edges
constexpr double kBadgeInset = 12; ///< keeps it clear of the device badge in the corner too

const QColor kCrossing(15, 138, 95);
const QColor kBridge(233, 196, 106);

QColor blend(const QColor &first, const QColor &second, double amount)
{
  return QColor::fromRgbF(
      first.redF() + (second.redF() - first.redF()) * amount,
      first.greenF() + (second.greenF() - first.greenF()) * amount,
      first.blueF() + (second.blueF() - first.blueF()) * amount
  );
}

QString sizeText(const QSize &size)
{
  return QStringLiteral("%1 × %2").arg(size.width()).arg(size.height());
}

QStringList wrapped(const QFont &font, const QString &text, double width)
{
  QTextLayout layout(text, font);
  QTextOption option;
  option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
  layout.setTextOption(option);

  QStringList lines;
  layout.beginLayout();
  for (auto line = layout.createLine(); line.isValid(); line = layout.createLine()) {
    line.setLineWidth(width);
    lines.append(text.mid(line.textStart(), line.textLength()).trimmed());
  }
  layout.endLayout();
  return lines;
}

void drawLabel(QPainter &painter, const QRectF &room, const QString &name, const QString &detail)
{
  const auto lines = DisplayLayoutView::labelLines(painter.font(), name, detail, room.size());
  painter.drawText(room, Qt::AlignCenter | Qt::TextDontClip, lines.join(QLatin1Char('\n')));
}

} // namespace

DisplayLayoutView *DisplayLayoutView::install(QDialog *dialog, ComputerSetupView *grid, ComputerSetupModel &model)
{
  auto *layout = qobject_cast<QBoxLayout *>(grid->parentWidget()->layout());
  if (layout == nullptr) {
    qWarning("the computers tab has no box layout, keeping the grid");
    return nullptr;
  }

  auto *view = new DisplayLayoutView(grid->parentWidget(), grid, model);
  const int gridPosition = layout->indexOf(grid);
  layout->insertWidget(gridPosition + 1, view, 1);

  view->m_modeButton = new QPushButton(grid->parentWidget());
  layout->insertWidget(gridPosition + 2, view->m_modeButton, 0, Qt::AlignRight);
  connect(view->m_modeButton, &QPushButton::clicked, view, [view] { view->setClassic(!view->m_classic); });

  connect(dialog, &QDialog::accepted, view, &DisplayLayoutView::commit);
  if (auto *trash = dialog->findChild<RemoveComputerWidget *>())
    connect(trash, &RemoveComputerWidget::computerRemoved, view, [view] { view->m_trashed = true; });
  if (auto *buttons = dialog->findChild<SettingsDialogButtonBox *>())
    connect(buttons, &SettingsDialogButtonBox::reset, view, &DisplayLayoutView::reload);

  view->reload();
  return view;
}

bool DisplayLayoutView::isModified(const QDialog *dialog)
{
  const auto *view = dialog->findChild<DisplayLayoutView *>();
  return view != nullptr && view->modified();
}

DisplayLayoutView::DisplayLayoutView(QWidget *parent, ComputerSetupView *grid, ComputerSetupModel &model)
    : QWidget(parent),
      m_grid(grid),
      m_model(model)
{
  setFocusPolicy(Qt::StrongFocus);
  setAcceptDrops(true);
  setMouseTracking(false);
  setMinimumHeight(320);
  setAccessibleName(tr("Screens drawn to scale"));

  connect(&m_model, &ComputerSetupModel::computersChanged, this, &DisplayLayoutView::syncFromModel);
}

bool DisplayLayoutView::modified() const
{
  return m_classic != m_savedClassic || m_state.clients != m_savedClients;
}

void DisplayLayoutView::reload()
{
  const auto stored = DisplayLayoutStore::load();
  m_classic = stored.saved && !stored.active;
  m_savedClassic = m_classic;

  QString serverName;
  for (const auto &computer : m_model.m_computers) {
    if (computer.isServer())
      serverName = computer.name();
  }
  m_state = DisplayLayoutStore::buildState(m_model.m_computers, serverName, stored);
  m_savedClients = m_state.clients;
  m_renames.clear();
  m_drag.reset();
  if (client(m_selected) == nullptr)
    m_selected.clear();

  applyClassic();
  changed(true);
}

void DisplayLayoutView::commit()
{
  auto stored = DisplayLayoutStore::load();
  stored.saved = true;
  stored.active = !m_classic;
  stored.clients = m_state.clients;
  DisplayLayoutStore::save(stored);
  for (const auto &[oldName, newName] : std::as_const(m_renames))
    DisplayLayoutStore::renameInProfiles(oldName, newName);
  m_renames.clear();
  m_savedClients = m_state.clients;
  m_savedClassic = m_classic;
}

void DisplayLayoutView::syncFromModel()
{
  StoredLayout current;
  current.saved = true;
  current.clients = m_state.clients;
  for (const auto &entry : std::as_const(m_state.clients)) {
    if (entry.sizeKnown)
      current.sizes.insert(entry.name, entry.size);
  }

  QString serverName = m_state.serverName;
  for (const auto &computer : m_model.m_computers) {
    if (computer.isServer())
      serverName = computer.name();
  }

  m_state = DisplayLayoutStore::buildState(m_model.m_computers, serverName, current);
  if (client(m_selected) == nullptr)
    m_selected.clear();
  resolve();
  layoutChips();
  update();
}

void DisplayLayoutView::resolve()
{
  m_solved = DisplayLayout::solve(m_state);
}

void DisplayLayoutView::changed(bool refit)
{
  resolve();
  layoutChips();
  if (refit)
    fitView();
  update();
}

void DisplayLayoutView::fitView()
{
  m_extents = m_solved.box;
  for (const auto &entry : std::as_const(m_state.clients)) {
    if (m_solved.rects.contains(entry.name))
      m_extents = m_extents.united(m_solved.rects.value(entry.name));
  }
  const auto area = mapArea().adjusted(kMapMargin, kMapMargin, -kMapMargin, -kMapMargin);
  if (m_extents.isEmpty() || area.isEmpty()) {
    m_scale = 1;
    m_offset = area.topLeft();
    return;
  }
  m_scale = std::min(
      static_cast<double>(area.width()) / m_extents.width(), static_cast<double>(area.height()) / m_extents.height()
  );
  m_offset = QPointF(
      area.x() + (area.width() - m_extents.width() * m_scale) / 2,
      area.y() + (area.height() - m_extents.height() * m_scale) / 2
  );
}

void DisplayLayoutView::layoutChips()
{
  m_chips.clear();
  const auto metrics = fontMetrics();
  int x = kShelfPadding;
  int row = 0;
  for (const auto &entry : std::as_const(m_state.clients)) {
    if (entry.placement.has_value() && !m_solved.parked.contains(entry.name))
      continue;
    const int width = metrics.horizontalAdvance(displayName(entry.name)) + 24;
    if (x + width > this->width() - kShelfPadding && x > kShelfPadding) {
      x = kShelfPadding;
      ++row;
    }
    m_chips.append({entry.name, QRect(x, row * (kChipHeight + kChipGap), width, kChipHeight), entry.bridge});
    x += width + kChipGap;
  }

  const auto shelf = shelfArea();
  for (auto &chip : m_chips)
    chip.rect.translate(shelf.x(), shelf.y() + kShelfHeader);
}

QRect DisplayLayoutView::shelfArea() const
{
  int rows = 1;
  for (const auto &chip : m_chips)
    rows = std::max(rows, 1 + (chip.rect.y() - m_chips.first().rect.y()) / (kChipHeight + kChipGap));
  const int height = kShelfHeader + rows * (kChipHeight + kChipGap) + kShelfPadding / 2;
  return {0, this->height() - height, width(), height};
}

QRect DisplayLayoutView::mapArea() const
{
  auto area = rect();
  area.setBottom(shelfArea().top() - 1);
  return area;
}

QPointF DisplayLayoutView::toWidget(const QPointF &point) const
{
  return m_offset + (point - m_extents.topLeft()) * m_scale;
}

QRectF DisplayLayoutView::toWidget(const QRect &rect) const
{
  return {toWidget(QPointF(rect.topLeft())), QSizeF(rect.size()) * m_scale};
}

QPoint DisplayLayoutView::toDesktop(const QPointF &point) const
{
  return {
      static_cast<int>(std::lround((point.x() - m_offset.x()) / m_scale)) + m_extents.x(),
      static_cast<int>(std::lround((point.y() - m_offset.y()) / m_scale)) + m_extents.y()
  };
}

QPoint DisplayLayoutView::centreOf(const QString &name) const
{
  for (const auto &chip : m_chips) {
    if (chip.name == name)
      return chip.rect.center();
  }
  return m_solved.rects.contains(name) ? toWidget(m_solved.rects.value(name)).center().toPoint() : QPoint();
}

QString DisplayLayoutView::clientAt(const QPoint &point) const
{
  for (const auto &chip : m_chips) {
    if (chip.rect.contains(point))
      return chip.name;
  }
  for (const auto &entry : m_state.clients) {
    if (m_solved.rects.contains(entry.name) && toWidget(m_solved.rects.value(entry.name)).contains(point))
      return entry.name;
  }
  return {};
}

bool DisplayLayoutView::monitorAt(const QPoint &point) const
{
  return std::ranges::any_of(m_state.monitors, [this, &point](const QRect &monitor) {
    return toWidget(monitor).contains(point);
  });
}

LayoutClient *DisplayLayoutView::client(const QString &name)
{
  // the list is shared with m_savedClients, and only non-const access detaches it before a write
  const auto found = std::ranges::find(m_state.clients, name, &LayoutClient::name);
  return found == m_state.clients.end() ? nullptr : &*found;
}

int DisplayLayoutView::cellOf(const QString &name) const
{
  // an empty name is what marks an empty cell
  if (name.isEmpty())
    return -1;
  for (int i = 0; i < m_model.m_computers.size(); ++i) {
    if (m_model.m_computers.at(i).name() == name)
      return i;
  }
  return -1;
}

QModelIndex DisplayLayoutView::indexOf(const QString &name) const
{
  const int cell = cellOf(name);
  if (cell < 0)
    return {};
  return m_model.index(cell / m_model.columnCount(), cell % m_model.columnCount());
}

QString DisplayLayoutView::displayName(const QString &name) const
{
  const auto index = indexOf(name);
  return index.isValid() ? m_model.data(index, Qt::DisplayRole).toString() : name;
}

QString DisplayLayoutView::uniqueName(const QString &name) const
{
  auto candidate = name;
  for (int number = 2; cellOf(candidate) >= 0; ++number)
    candidate = QStringLiteral("%1%2").arg(name).arg(number);
  return candidate;
}

void DisplayLayoutView::select(const QString &name)
{
  m_selected = name;
  update();
}

void DisplayLayoutView::applyClassic()
{
  setVisible(!m_classic);
  m_grid->setVisible(m_classic);
  if (m_modeButton != nullptr)
    m_modeButton->setText(m_classic ? tr("Use the to-scale layout") : tr("Use the classic grid"));
}

void DisplayLayoutView::setClassic(bool classic)
{
  m_classic = classic;
  applyClassic();
  Q_EMIT m_model.computersChanged();
}

void DisplayLayoutView::unlinkComputer(const QString &name)
{
  if (client(name) == nullptr)
    return;
  DisplayLayout::unlink(m_state, name);
  changed(true);
  Q_EMIT m_model.computersChanged();
}

void DisplayLayoutView::removeComputer(const QString &name)
{
  const int cell = cellOf(name);
  if (cell < 0 || name == m_state.serverName || !m_grid->confirmRemove(name))
    return;

  m_model.m_computers[cell] = Computer();
  DisplayLayout::removeClient(m_state, name);
  if (m_selected == name)
    m_selected.clear();
  changed(true);

  // the dialog listens to these for the grid: new computer button, and the bridge client prompt
  if (auto *trash = window()->findChild<RemoveComputerWidget *>())
    Q_EMIT trash->computerRemoved();
  Q_EMIT m_grid->screenDeleted(name);
  Q_EMIT m_model.computersChanged();
}

void DisplayLayoutView::openSettings(const QString &name)
{
  const int cell = cellOf(name);
  if (cell < 0)
    return;

  auto &computer = m_model.m_computers[cell];
  ComputerSettingsDialog dialog(this, &computer, &m_model.m_computers);
  dialog.exec();

  const auto newName = computer.name();
  if (newName != name) {
    if (name == m_state.serverName)
      m_state.serverName = newName;
    else
      DisplayLayout::renameClient(m_state, name, newName);
    m_renames.append({name, newName});
    if (m_selected == name)
      m_selected = newName;
    changed(false);
  }
  Q_EMIT m_model.computersChanged();
}

void DisplayLayoutView::nudge(LayoutClient &entry, int delta)
{
  auto &placement = *entry.placement;
  placement.offset += delta;

  // a placement clamps to what fits, so the offset has to follow where it was drawn
  resolve();
  if (m_solved.rects.contains(entry.name)) {
    const auto rect = m_solved.rects.value(entry.name);
    const auto parent = placement.anchor.isEmpty() ? m_solved.box : m_solved.rects.value(placement.anchor);
    const bool down = placement.side == LayoutSide::Left || placement.side == LayoutSide::Right;
    placement.offset = down ? rect.y() - parent.y() : rect.x() - parent.x();
  }
  changed(false);
  Q_EMIT m_model.computersChanged();
}

bool DisplayLayoutView::event(QEvent *event)
{
  if (event->type() == QEvent::ToolTip) {
    const auto *help = static_cast<QHelpEvent *>(event);
    const auto name = clientAt(help->pos());
    const auto index = indexOf(name.isEmpty() && monitorAt(help->pos()) ? m_state.serverName : name);
    if (index.isValid())
      QToolTip::showText(help->globalPos(), m_model.data(index, Qt::ToolTipRole).toString(), this);
    else
      QToolTip::hideText();
    return true;
  }
  return QWidget::event(event);
}

void DisplayLayoutView::resizeEvent(QResizeEvent *event)
{
  QWidget::resizeEvent(event);
  layoutChips();
  fitView();
}

void DisplayLayoutView::paintEvent(QPaintEvent *)
{
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.fillRect(mapArea(), palette().color(QPalette::AlternateBase));

  drawDesktop(painter);

  const auto moving =
      m_drag.has_value() && m_drag->moving ? DisplayLayout::subtree(m_state, m_drag->name) : QSet<QString>();
  for (const auto &entry : m_state.clients) {
    if (m_solved.rects.contains(entry.name) && !moving.contains(entry.name))
      drawClient(painter, entry, toWidget(m_solved.rects.value(entry.name)), false);
  }
  if (moving.isEmpty())
    drawCrossings(painter);

  drawShelf(painter);

  if (m_drag.has_value() && m_drag->moving) {
    if (m_drag->snap.has_value()) {
      painter.setPen(QPen(palette().color(QPalette::Highlight), 2, Qt::DashLine));
      painter.setBrush(blend(palette().color(QPalette::AlternateBase), palette().color(QPalette::Highlight), 0.2));
      painter.drawRoundedRect(toWidget(m_drag->snap->rect), kCornerRadius, kCornerRadius);
    }
    painter.setOpacity(0.75);
    drawClient(painter, *client(m_drag->name), toWidget(m_drag->free), true);
    painter.setOpacity(1);
  }
}

QStringList
DisplayLayoutView::labelLines(const QFont &font, const QString &name, const QString &detail, const QSizeF &room)
{
  const QFontMetricsF metrics(font);
  const auto capacity = static_cast<qsizetype>(room.height() / metrics.lineSpacing());
  const auto fits = [&](const QStringList &lines) {
    return lines.size() <= capacity && std::ranges::all_of(lines, [&](const QString &line) {
             return metrics.horizontalAdvance(line) <= room.width();
           });
  };

  // a name cut in the middle keeps the end that tells two devices apart
  const QStringList shortName{metrics.elidedText(name, Qt::ElideMiddle, room.width())};
  const auto fullName = wrapped(font, name, room.width());
  const auto size = wrapped(font, detail, room.width());

  for (const auto &lines : {fullName + size, shortName + size, fullName, shortName}) {
    if (fits(lines))
      return lines;
  }
  return {};
}

void DisplayLayoutView::drawDesktop(QPainter &painter) const
{
  const auto text = palette().color(QPalette::Text);
  const auto accent = palette().color(QPalette::Highlight);

  // the part of the box with no display has no edge to cross
  if (!m_solved.box.isEmpty()) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(blend(palette().color(QPalette::AlternateBase), text, 0.25), Qt::BDiagPattern));
    painter.drawRect(toWidget(m_solved.box));
  }

  const auto primary = DisplayMonitors::primary();
  int number = 1;
  for (const auto &monitor : m_state.monitors) {
    const auto area = toWidget(monitor);
    painter.setPen(QPen(blend(accent, text, 0.5), 1));
    painter.setBrush(blend(palette().color(QPalette::Base), accent, 0.25));
    painter.drawRoundedRect(area, kCornerRadius, kCornerRadius);

    painter.setPen(text);
    const auto title = monitor == primary ? tr("%1 · main").arg(number) : QString::number(number);
    const auto room = area.adjusted(kLabelInset, kLabelInset, -kLabelInset, -kLabelInset);
    drawLabel(painter, room, title, sizeText(monitor.size()));
    ++number;
  }

  painter.setPen(QPen(accent, m_drag.has_value() && m_drag->moving ? 4 : 2, Qt::SolidLine, Qt::RoundCap));
  using enum LayoutSide;
  for (const auto side : {Left, Right, Up, Down}) {
    const auto segments = DisplayLayout::reach(m_state.monitors, m_solved.box, side, m_state.freeEdges);
    for (const auto &segment : segments) {
      const bool down = side == Left || side == Right;
      const QRect line = down ? QRect(segment.at, segment.start, 0, segment.end - segment.start)
                              : QRect(segment.start, segment.at, segment.end - segment.start, 0);
      const auto area = toWidget(line);
      painter.drawLine(area.topLeft(), area.bottomRight());
    }
  }
}

void DisplayLayoutView::drawClient(
    QPainter &painter, const LayoutClient &entry, const QRectF &area, bool dragging
) const
{
  const auto text = palette().color(QPalette::Text);
  const auto fill = palette().color(QPalette::Base);
  const bool selected = entry.name == m_selected && !dragging;

  QPen pen(selected ? palette().color(QPalette::Highlight) : blend(fill, text, 0.5), selected ? 2 : 1);
  if (!entry.sizeKnown)
    pen.setStyle(Qt::DashLine);
  painter.setPen(pen);
  painter.setBrush(entry.bridge ? blend(fill, kBridge, 0.5) : fill);
  painter.drawRoundedRect(area, kCornerRadius, kCornerRadius);

  // a bridge client whose device is plugged in has a badge, as in the grid
  const bool badge = !BridgeClientManager::layoutToolTip(entry.name).isEmpty();

  painter.setPen(text);
  const auto inset = badge ? kBadgeInset : kLabelInset;
  const auto detail = entry.sizeKnown ? sizeText(entry.size) : tr("size unknown");
  drawLabel(painter, area.adjusted(kLabelInset, inset, -kLabelInset, -inset), displayName(entry.name), detail);

  if (badge) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(kCrossing);
    painter.drawEllipse(QPointF(area.right() - 8, area.top() + 8), 3, 3);
  }
}

void DisplayLayoutView::drawCrossings(QPainter &painter) const
{
  painter.setPen(QPen(kCrossing, 4, Qt::SolidLine, Qt::RoundCap));
  const auto blocks = DisplayLayout::links(m_state, m_solved);
  for (const auto &block : blocks) {
    // the server's ranges are measured on the box, so a client's side shows where it really touches
    if (block.source == m_state.serverName)
      continue;
    const auto rect = m_solved.rects.value(block.source);
    for (const auto &link : block.links) {
      const bool down = link.side == LayoutSide::Left || link.side == LayoutSide::Right;
      const double start = down ? rect.y() : rect.x();
      const double length = down ? rect.height() : rect.width();
      const double at = DisplayLayout::edgeOf(rect, link.side);
      const double first = start + length * link.range.start / 100;
      const double second = start + length * link.range.end / 100;
      painter.drawLine(
          toWidget(down ? QPointF(at, first) : QPointF(first, at)),
          toWidget(down ? QPointF(at, second) : QPointF(second, at))
      );
    }
  }
}

void DisplayLayoutView::drawShelf(QPainter &painter) const
{
  const auto shelf = shelfArea();
  const auto text = palette().color(QPalette::Text);
  const bool over = m_drag.has_value() && m_drag->moving && m_drag->overShelf;
  painter.fillRect(
      shelf, over ? blend(palette().color(QPalette::Window), palette().color(QPalette::Highlight), 0.2)
                  : palette().color(QPalette::Window)
  );

  painter.setPen(text);
  painter.drawText(
      shelf.adjusted(kShelfPadding, 0, -kShelfPadding, 0).translated(0, 4), Qt::AlignLeft | Qt::AlignTop,
      m_chips.isEmpty() ? tr("Not linked: drop a computer here to remove its edges")
                        : tr("Not linked: reach these with a hotkey")
  );

  for (const auto &chip : m_chips) {
    const auto fill = palette().color(QPalette::Base);
    const bool selected = chip.name == m_selected;
    painter.setPen(QPen(selected ? palette().color(QPalette::Highlight) : blend(fill, text, 0.5), selected ? 2 : 1));
    painter.setBrush(chip.bridge ? blend(fill, kBridge, 0.5) : fill);
    painter.drawRoundedRect(chip.rect, kCornerRadius, kCornerRadius);
    painter.setPen(text);
    painter.drawText(chip.rect, Qt::AlignCenter, displayName(chip.name));
  }
}

void DisplayLayoutView::mousePressEvent(QMouseEvent *event)
{
  setFocus();
  if (event->button() != Qt::LeftButton)
    return;

  const auto point = event->position().toPoint();
  const auto name = clientAt(point);
  select(name);
  if (name.isEmpty())
    return;

  const auto *entry = client(name);
  Drag drag;
  drag.name = name;
  drag.pressedAt = point;
  if (m_solved.rects.contains(name)) {
    drag.free = m_solved.rects.value(name);
    drag.grab = toDesktop(point) - drag.free.topLeft();
  } else {
    drag.grab = QPoint(entry->size.width() / 2, entry->size.height() / 2);
    drag.free = QRect(toDesktop(point) - drag.grab, entry->size);
  }
  m_drag = drag;
}

void DisplayLayoutView::updateDrag(const QPoint &point)
{
  m_drag->free.moveTopLeft(toDesktop(point) - m_drag->grab);
  m_drag->overShelf = shelfArea().contains(point);
  const int tolerance = static_cast<int>(kMagnetPixels / m_scale);
  m_drag->snap =
      m_drag->overShelf ? std::nullopt : DisplayLayout::snap(m_state, m_solved, m_drag->name, m_drag->free, tolerance);
}

void DisplayLayoutView::mouseMoveEvent(QMouseEvent *event)
{
  if (!m_drag.has_value())
    return;

  const auto point = event->position().toPoint();
  if (!m_drag->moving && (point - m_drag->pressedAt).manhattanLength() < kDragThreshold)
    return;
  m_drag->moving = true;

  // leaving the map turns this into a real drag, so the trash can take it
  if (!rect().contains(point)) {
    const auto name = m_drag->name;
    m_drag.reset();
    startExternalDrag(name);
    return;
  }
  updateDrag(point);
  update();
}

void DisplayLayoutView::mouseReleaseEvent(QMouseEvent *event)
{
  if (!m_drag.has_value() || event->button() != Qt::LeftButton)
    return;

  const auto drag = *m_drag;
  m_drag.reset();
  if (!drag.moving)
    return;

  if (drag.overShelf) {
    unlinkComputer(drag.name);
    return;
  }
  if (drag.snap.has_value()) {
    client(drag.name)->placement = LayoutPlacement{drag.snap->anchor, drag.snap->side, drag.snap->offset, false};
    changed(true);
    Q_EMIT m_model.computersChanged();
    return;
  }
  update();
}

void DisplayLayoutView::mouseDoubleClickEvent(QMouseEvent *event)
{
  if (event->button() != Qt::LeftButton)
    return;
  m_drag.reset();

  const auto point = event->position().toPoint();
  const auto name = clientAt(point);
  if (!name.isEmpty())
    openSettings(name);
  else if (monitorAt(point))
    openSettings(m_state.serverName);
}

void DisplayLayoutView::keyPressEvent(QKeyEvent *event)
{
  auto *entry = client(m_selected);
  if (entry == nullptr || !entry->placement.has_value()) {
    QWidget::keyPressEvent(event);
    return;
  }

  const int step = event->modifiers().testFlag(Qt::ShiftModifier) ? kNudgeLarge : kNudge;
  const bool down = entry->placement->side == LayoutSide::Left || entry->placement->side == LayoutSide::Right;
  switch (event->key()) {
  case Qt::Key_Up:
    if (down)
      nudge(*entry, -step);
    break;
  case Qt::Key_Down:
    if (down)
      nudge(*entry, step);
    break;
  case Qt::Key_Left:
    if (!down)
      nudge(*entry, -step);
    break;
  case Qt::Key_Right:
    if (!down)
      nudge(*entry, step);
    break;
  case Qt::Key_Delete:
  case Qt::Key_Backspace:
    unlinkComputer(m_selected);
    break;
  default:
    QWidget::keyPressEvent(event);
  }
}

void DisplayLayoutView::contextMenuEvent(QContextMenuEvent *event)
{
  const auto name = clientAt(event->pos());
  if (name.isEmpty())
    return;
  select(name);

  QMenu menu(this);
  connect(menu.addAction(tr("Settings…")), &QAction::triggered, this, [this, name] { openSettings(name); });
  auto *unlink = menu.addAction(tr("Move to Not linked"));
  unlink->setEnabled(client(name)->placement.has_value());
  connect(unlink, &QAction::triggered, this, [this, name] { unlinkComputer(name); });
  connect(menu.addAction(tr("Remove")), &QAction::triggered, this, [this, name] { removeComputer(name); });
  menu.exec(event->globalPos());
}

void DisplayLayoutView::startExternalDrag(const QString &name)
{
  const auto index = indexOf(name);
  auto *drag = new QDrag(this);
  drag->setMimeData(m_model.mimeData({index}));
  drag->setPixmap(m_model.data(index, Qt::DecorationRole).value<QPixmap>());

  m_trashed = false;
  update();
  // what exec() returns can't be trusted: on Wayland a drop on the trash comes back as IgnoreAction
  drag->exec(Qt::MoveAction, Qt::MoveAction);
  finishExternalDrag(name);
}

void DisplayLayoutView::finishExternalDrag(const QString &name)
{
  if (m_trashed)
    removeComputer(name);
  // removeComputer() signals the trash too, so this clears after it
  m_trashed = false;
}

void DisplayLayoutView::dropOwnComputer(const QString &name, const QPoint &point)
{
  auto *entry = client(name);
  if (entry == nullptr)
    return;
  if (shelfArea().contains(point)) {
    unlinkComputer(name);
    return;
  }

  const auto free = QRect(toDesktop(point) - QPoint(entry->size.width() / 2, entry->size.height() / 2), entry->size);
  if (const auto snap = DisplayLayout::snap(m_state, m_solved, name, free, 0)) {
    entry->placement = LayoutPlacement{snap->anchor, snap->side, snap->offset, false};
    changed(true);
    Q_EMIT m_model.computersChanged();
  }
}

void DisplayLayoutView::dragEnterEvent(QDragEnterEvent *event)
{
  if (event->mimeData()->hasFormat(ComputerSetupModel::mimeType()))
    event->acceptProposedAction();
}

void DisplayLayoutView::dragMoveEvent(QDragMoveEvent *event)
{
  // only what dragEnterEvent() accepted gets here
  event->acceptProposedAction();
}

void DisplayLayoutView::dropEvent(QDropEvent *event)
{
  auto data = event->mimeData()->data(ComputerSetupModel::mimeType());
  QDataStream stream(&data, QIODevice::ReadOnly);
  int column = -1;
  int row = -1;
  Computer computer;
  stream >> column >> row >> computer;
  const auto point = event->position().toPoint();

  if (event->source() == this) {
    event->acceptProposedAction();
    dropOwnComputer(computer.name(), point);
    return;
  }

  // a new computer from the dialog's icon
  if (m_model.isFull())
    return;
  event->acceptProposedAction();
  computer.setName(uniqueName(computer.name()));
  const auto name = computer.name();
  m_model.m_computers.addComputerToFirstEmpty(computer);
  syncFromModel();

  auto *entry = client(name);
  if (shelfArea().contains(point)) {
    entry->placement.reset();
  } else {
    const auto free = QRect(toDesktop(point) - QPoint(entry->size.width() / 2, entry->size.height() / 2), entry->size);
    if (const auto snap = DisplayLayout::snap(m_state, m_solved, name, free, 0))
      entry->placement = LayoutPlacement{snap->anchor, snap->side, snap->offset, false};
  }
  select(name);
  changed(true);
  Q_EMIT m_model.computersChanged();
}

} // namespace deskflow::gui
