/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "AboutDialog.h"
#include "ui_AboutDialog.h"

#include "common/Constants.h"
#include "common/VersionInfo.h"

#include <QClipboard>

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent), ui{std::make_unique<Ui::AboutDialog>()}
{
  ui->setupUi(this);

  const int px = (fontMetrics().height() * 6);
  const QSize pixmapSize(px, px);
  ui->lblIcon->setFixedSize(pixmapSize);

  ui->lblIcon->setPixmap(QPixmap(QIcon::fromTheme(kRevFqdnName).pixmap(QSize().scaled(pixmapSize, Qt::KeepAspectRatio)))
  );

  ui->btnCopyVersion->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::EditCopy));
  connect(ui->btnCopyVersion, &QPushButton::clicked, this, &AboutDialog::copyVersionText);

  ui->lblVersion->setText(kDisplayVersion);
  // Simplified About Dialog as requested
  // Line 1: Description
  // Line 2: Fork info + Acknowledgement
  // Line 3: Support

  // Note: kAppDescription is "Keyboard and mouse sharing utility - HID version" (verified in Constants.h.in)
  // We construct the HTML text manually for rich text support in the description label if needed,
  // or just use newlines. The user requested:
  // Forked from Deskflow(<- with link to it, and show ackorelage to them plolitely)

  ui->lblDescription->setTextFormat(Qt::RichText);
  ui->lblDescription->setOpenExternalLinks(true);

  const QString text =
      QStringLiteral("%1<br/><br/>"
                     "Forked from <a href='https://github.com/deskflow/deskflow'>Deskflow</a>. "
                     "We gratefully acknowledge their work and thank the original contributors.<br/><br/>"
                     "Support: %2")
          .arg(kAppDescription, kSupportEmail);

  ui->lblDescription->setText(text);

  // Clear the copyright label if not needed, or keep it. User said "Just keep them items".
  // The user listed 3 items. I will hide the other labels to be safe/clean as requested ("Clean UI").
  ui->lblCopyright->setVisible(false);

  ui->btnOk->setDefault(true);
  connect(ui->btnOk, &QPushButton::clicked, this, &AboutDialog::close);
}

void AboutDialog::copyVersionText() const
{
  QString infoString = QStringLiteral("%1: %2 (%3)\nQt: %4\nSystem: %5")
                           .arg(kAppName, kVersion, kVersionGitSha, qVersion(), QSysInfo::prettyProductName());
#ifdef Q_OS_LINUX
  infoString.append(QStringLiteral("\nSession: %1 (%2)")
                        .arg(qEnvironmentVariable("XDG_CURRENT_DESKTOP"), qEnvironmentVariable("XDG_SESSION_TYPE")));
#endif
  QGuiApplication::clipboard()->setText(infoString);
}

AboutDialog::~AboutDialog() = default;
