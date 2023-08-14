/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_SYSTEM_HELPER_PRIVILEGE_PRIVILEGE_INSTALL_PORTAL_H_
#define LINGLONG_SRC_SYSTEM_HELPER_PRIVILEGE_PRIVILEGE_INSTALL_PORTAL_H_

#include "linglong/util/error.h"
#include "linglong/util/qserializer/deprecated.h"

#include <QString>
#include <QVariantMap>

namespace linglong {
namespace system {
namespace helper {

class FilePortalRule : public Serialize
{
    Q_OBJECT
    Q_SERIALIZE_CONSTRUCTOR(FilePortalRule)
public:
    Q_SERIALIZE_PROPERTY(QString, source)
    Q_SERIALIZE_PROPERTY(QString, target)
};
Q_SERIALIZE_DECLARE_LIST_MAP(FilePortalRule)

class PortalRule : public Serialize
{
    Q_OBJECT
    Q_SERIALIZE_CONSTRUCTOR(PortalRule)
public:
    Q_SERIALIZE_PROPERTY(QStringList, whiteList)
    Q_SERIALIZE_PROPERTY(QList<QSharedPointer<linglong::system::helper::FilePortalRule>>,
                         fileRuleList)
};
Q_SERIALIZE_DECLARE_LIST_MAP(PortalRule)

util::Error rebuildPrivilegeInstallPortal(const QString &repoRoot,
                                          const QString &ref,
                                          const QVariantMap &options);
util::Error ruinPrivilegeInstallPortal(const QString &repoRoot,
                                       const QString &ref,
                                       const QVariantMap &options);

} // namespace helper
} // namespace system
} // namespace linglong
#endif // LINGLONG_SRC_SYSTEM_HELPER_PRIVILEGE_PRIVILEGE_INSTALL_PORTAL_H_
