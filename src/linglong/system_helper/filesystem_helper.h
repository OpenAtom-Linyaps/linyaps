/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_FILESYSTEM_HELPER_H
#define LINGLONG_FILESYSTEM_HELPER_H

#include <QDBusContext>
#include <QObject>

namespace linglong {
namespace system {
namespace helper {

class FilesystemHelper : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.linglong.FilesystemHelper")
public:
    using QObject::QObject;

    void Mount(const QString &source,
               const QString &mountPoint,
               const QString &fsType,
               const QVariantMap &options);

    void Umount(const QString &mountPoint, const QVariantMap &options);
};

} // namespace helper
} // namespace system
} // namespace linglong

#endif // LINGLONG_FILESYSTEM_HELPER_H
