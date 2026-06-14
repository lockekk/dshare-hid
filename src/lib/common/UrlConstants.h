/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2024 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QString>

// important: this is used for settings paths on some platforms,
// and must not be a url. qt automatically converts this to reverse domain
// notation (rdn), e.g. org.deskflow
const auto kOrgDomain = QStringLiteral("github.com/lockekk/dshare-hid");

const auto kUrlSourceQuery = QStringLiteral("source=gui");
const auto kUrlApp = QStringLiteral("https://github.com/lockekk/dshare-hid");
const auto kUrlHelp = QStringLiteral("https://github.com/lockekk/dshare-hid/issues");
const auto kUrlDownload = QStringLiteral("%1/releases?%2").arg(kUrlApp, kUrlSourceQuery);

// Point to raw version file for update check
const auto kUrlUpdateCheck =
    QStringLiteral("https://raw.githubusercontent.com/lockekk/dshare-hid-release/main/version");

#if defined(Q_OS_LINUX)
const auto kUrlGnomeTrayFix = QStringLiteral("https://extensions.gnome.org/extension/615/appindicator-support/");
#endif
