/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QString>
#include <QScopedPointer>

namespace linglong {
namespace util {

class DesktopEntryPrivate;
class DesktopEntry
{
public:
    explicit DesktopEntry(const QString &filePath) noexcept;
    ~DesktopEntry();

    QString rawValue(const QString &key, const QString &section = "Desktop Entry",
                     const QString &defaultValue = QString());

private:
    QScopedPointer<DesktopEntryPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), DesktopEntry)
};

} // namespace util
} // namespace linglong