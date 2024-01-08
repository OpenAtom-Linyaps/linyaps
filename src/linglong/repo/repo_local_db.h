/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_REPO_REPO_LOCAL_DB_H_
#define LINGLONG_SRC_MODULE_REPO_REPO_LOCAL_DB_H_

#include "linglong/package/package.h"
#include "linglong/util/connection.h"
#include "linglong/utils/error/error.h"

#include <QList>
#include <QObject>
#include <QtSql>

namespace linglong {
namespace repo {

// 使用数据库记录仓库的信息，便于查询应用
class LocalDB : public QObject
{
public:
    explicit LocalDB(util::Connection &dbConnection, QObject *parent = nullptr);

    utils::error::Result<void> insertAppRecord(const package::AppMetaInfo &package,
                                               const QString &userName);

    utils::error::Result<void> deleteAppRecord(const package::AppMetaInfo &package,
                                               const QString &userName);
    linglong::utils::error::Result<QList<QSharedPointer<package::AppMetaInfo>>>
    queryAppRecord(const QString &userName,
                   const QString &appID,
                   const QString &arch,
                   const QString &channel,
                   const QString &module,
                   const QString &version);

private:
    util::Connection &dbConnection;
};

} // namespace repo
} // namespace linglong
#endif // LINGLONG_SRC_MODULE_REPO_REPO_LOCAL_DB_H_