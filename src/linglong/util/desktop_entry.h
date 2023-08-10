/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_UTIL_DESKTOP_ENTRY_H_
#define LINGLONG_SRC_MODULE_UTIL_DESKTOP_ENTRY_H_

#include "linglong/util/error.h"

#include <QScopedPointer>
#include <QString>

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

    QString rawValue(const QString &key,
                     const QString &section = SectionDesktopEntry,
                     const QString &defaultValue = QString());

    QStringList sections();

    util::Error save(const QString &filepath);

private:
    QScopedPointer<DesktopEntryPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), DesktopEntry)
};

} // namespace util
} // namespace linglong
#endif
