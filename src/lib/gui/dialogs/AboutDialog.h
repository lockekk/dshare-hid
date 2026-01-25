/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QDialog>

namespace Ui {
class AboutDialog;
}

class AboutDialog : public QDialog
{
  Q_OBJECT
public:
  explicit AboutDialog(QWidget *parent = nullptr);
  ~AboutDialog() override;

private:
  std::unique_ptr<Ui::AboutDialog> ui;
  void copyVersionText() const;

  inline static const QString kSupportEmail = QStringLiteral("dshare.hid@gmail.com");
};
