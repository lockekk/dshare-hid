/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QFlags>
#include <QTableView>

#include <functional> // DShare-HID

class QWidget;
class QMouseEvent;
class QResizeEvent;
class QDragEnterEvent;
class ComputerSetupModel;

class ComputerSetupView : public QTableView
{
  Q_OBJECT

public:
  explicit ComputerSetupView(QWidget *parent);
  void setModel(QAbstractItemModel *model) override;
  ComputerSetupModel *model() const;

  // DShare-HID: asked before a computer is removed, and false keeps it
  void setRemoveConfirmation(std::function<bool(const QString &)> confirm)
  {
    m_confirmRemove = std::move(confirm);
  }
  bool confirmRemove(const QString &screenName) const
  {
    return !m_confirmRemove || m_confirmRemove(screenName);
  }

Q_SIGNALS:
  void screenDeleted(const QString &screenName);

private:
  void showComputerConfig(int col, int row);
  std::function<bool(const QString &)> m_confirmRemove; // DShare-HID

protected:
  void mouseDoubleClickEvent(QMouseEvent *) override;
  void setTableSize();
  void resizeEvent(QResizeEvent *) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void startDrag(Qt::DropActions supportedActions) override;
  void initViewItemOption(QStyleOptionViewItem *option) const override;
  void scrollTo(const QModelIndex &, ScrollHint) override
  {
    // do nothing
  }
};
