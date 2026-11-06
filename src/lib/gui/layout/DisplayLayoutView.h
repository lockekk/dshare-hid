/*
 * dshare-hid -- created by dshare.hid@gmail.com
 */

#pragma once

#include "DisplayLayout.h"
#include "DisplayLayoutStore.h"

#include <QPointer>
#include <QWidget>

#include <optional>

class ComputerSetupModel;
class ComputerSetupView;
class QDialog;
class QPushButton;

namespace deskflow::gui {

/**
 * @brief The Computers tab drawn to scale: the server's real displays and its clients
 *
 * It takes the place of ComputerSetupView, which stays in the dialog, hidden, so upstream's grid and
 * its model carry on as the list of computers. Nothing here changes until the dialog is saved.
 */
class DisplayLayoutView : public QWidget
{
  Q_OBJECT

public:
  /// puts the view beside the dialog's grid and connects it to the dialog's Save and Reset
  static DisplayLayoutView *install(QDialog *dialog, ComputerSetupView *grid, ComputerSetupModel &model);

  /// true if the dialog's layout differs from the saved one
  static bool isModified(const QDialog *dialog);

  /// what a rectangle can show in full, wrapped to fit: a line that won't fit is left out, never cut
  static QStringList labelLines(const QFont &font, const QString &name, const QString &detail, const QSizeF &room);

  DisplayLayoutView(QWidget *parent, ComputerSetupView *grid, ComputerSetupModel &model);

  const LayoutState &state() const
  {
    return m_state;
  }
  const SolvedLayout &solved() const
  {
    return m_solved;
  }
  const QString &selected() const
  {
    return m_selected;
  }
  bool isClassic() const
  {
    return m_classic;
  }
  bool modified() const;

  /// where a computer is drawn, as a point on this widget
  QPoint centreOf(const QString &name) const;
  /// the Not linked area
  QRect shelfArea() const;

  void select(const QString &name);
  void setClassic(bool classic);
  void unlinkComputer(const QString &name);
  void removeComputer(const QString &name);
  void openSettings(const QString &name);

  /// a computer dragged off the map came back to it
  void dropOwnComputer(const QString &name, const QPoint &point);
  /// a computer dragged off the map is gone if the trash took it
  void finishExternalDrag(const QString &name, Qt::DropAction action);

  /// saves the layout, for the dialog's Save
  void commit();
  /// back to what's saved, for the dialog's Reset
  void reload();

protected:
  bool event(QEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dropEvent(QDropEvent *event) override;

private:
  struct Drag
  {
    QString name;
    QPoint pressedAt;
    QPoint grab; ///< from the computer's corner, in desktop units
    bool moving = false;
    bool overShelf = false;
    QRect free;
    std::optional<LayoutSnap> snap;
  };

  struct Chip
  {
    QString name;
    QRect rect;
    bool bridge = false;
  };

  void applyClassic();
  void syncFromModel();
  void changed(bool refit);
  void resolve();
  void fitView();
  void layoutChips();

  QRect mapArea() const;
  QPointF toWidget(const QPointF &point) const;
  QRectF toWidget(const QRect &rect) const;
  QPoint toDesktop(const QPointF &point) const;
  QString clientAt(const QPoint &point) const;
  bool monitorAt(const QPoint &point) const;

  void drawDesktop(QPainter &painter) const;
  void drawClient(QPainter &painter, const LayoutClient &client, const QRectF &rect, bool dragging) const;
  void drawCrossings(QPainter &painter) const;
  void drawShelf(QPainter &painter) const;

  void updateDrag(const QPoint &point);
  void startExternalDrag(const QString &name);
  void nudge(LayoutClient &entry, int delta);

  int cellOf(const QString &name) const;
  QModelIndex indexOf(const QString &name) const;
  QString displayName(const QString &name) const;
  QString uniqueName(const QString &name) const;
  LayoutClient *client(const QString &name);

  ComputerSetupView *m_grid;
  ComputerSetupModel &m_model;
  LayoutState m_state;
  SolvedLayout m_solved;
  QList<LayoutClient> m_savedClients;
  bool m_classic = false;
  bool m_savedClassic = false;
  QString m_selected;
  std::optional<Drag> m_drag;
  bool m_externalDropHandled = false;
  QList<Chip> m_chips;

  QRect m_extents;
  double m_scale = 1;
  QPointF m_offset;

  QPointer<QPushButton> m_modeButton;
  QList<std::pair<QString, QString>> m_renames; ///< to reach the Bridge profiles on Save
};

} // namespace deskflow::gui
