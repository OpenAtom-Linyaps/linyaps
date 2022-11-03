/*
 * Copyright (c) 2022. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_SYSTEM_HELPER_PRIVILEGE_PRIVILEGE_INSTALL_PORTAL_H_
#define LINGLONG_SRC_SYSTEM_HELPER_PRIVILEGE_PRIVILEGE_INSTALL_PORTAL_H_

#include <QString>
#include <QVariantMap>

#include "module/util/serialize/serialize.h"
#include "module/util/serialize/json.h"
#include "module/util/result.h"

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
    Q_SERIALIZE_PROPERTY(linglong::system::helper::FilePortalRuleList, fileRuleList)
};
Q_SERIALIZE_DECLARE_LIST_MAP(PortalRule)

util::Error rebuildPrivilegeInstallPortal(const QString &repoRoot, const QString &ref, const QVariantMap &options);
util::Error ruinPrivilegeInstallPortal(const QString &repoRoot, const QString &ref, const QVariantMap &options);

} // namespace helper
} // namespace system
} // namespace linglong

Q_SERIALIZE_REGISTER_TYPE(linglong::system::helper::FilePortalRule, linglong::system::helper::FilePortalRuleList,
                          linglong::system::helper::FilePortalRuleStrMap)
Q_SERIALIZE_REGISTER_TYPE(linglong::system::helper::PortalRule, linglong::system::helper::PortalRuleList,
                          linglong::system::helper::PortalRuleStrMap)

#endif // LINGLONG_SRC_SYSTEM_HELPER_PRIVILEGE_PRIVILEGE_INSTALL_PORTAL_H_
