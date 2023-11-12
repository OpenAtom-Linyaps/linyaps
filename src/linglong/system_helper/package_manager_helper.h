/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_SYSTEM_HELPER_SYSTEM_HELPER_H_
#define LINGLONG_SRC_SYSTEM_HELPER_SYSTEM_HELPER_H_

#include <QDBusContext>
#include <QObject>

namespace linglong {
namespace system {
namespace helper {

class PackageManagerHelper : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.linglong.PackageManagerHelper")
public:
    using QObject::QObject;

    void RebuildInstallPortal(const QString &installPath,
                              const QString &ref,
                              const QVariantMap &options);
    void RuinInstallPortal(const QString &installPath,
                           const QString &ref,
                           const QVariantMap &options);
};

} // namespace helper
} // namespace system
} // namespace linglong

#endif // LINGLONG_SRC_SYSTEM_HELPER_SYSTEM_HELPER_H_
