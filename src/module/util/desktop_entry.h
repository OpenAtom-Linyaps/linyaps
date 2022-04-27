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
#include "result.h"

namespace linglong {
namespace util {

class DesktopEntryPrivate;
class DesktopEntry
{
public:
    static const char *SectionDesktopEntry;

    explicit DesktopEntry(const QString &filePath) noexcept;
    ~DesktopEntry();

    void set(const QString &section, const QString &key, const QString &defaultValue = QString());

    QString rawValue(const QString &key, const QString &section = SectionDesktopEntry,
                     const QString &defaultValue = QString());

    linglong::util::Error save(const QString &filepath);

private:
    QScopedPointer<DesktopEntryPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), DesktopEntry)
};

} // namespace util
} // namespace linglong