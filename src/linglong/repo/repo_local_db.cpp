/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "repo_local_db.h"

#include "linglong/utils/error/error.h"

namespace linglong {
namespace repo {
LocalDB::LocalDB(util::Connection &dbConnection, QObject *parent)
    : QObject(parent)
    , dbConnection(dbConnection)
{
    QString createInfoTable = "CREATE TABLE IF NOT EXISTS installedAppInfo(\
         ID INTEGER PRIMARY KEY AUTOINCREMENT,\
         appId VARCHAR(32) NOT NULL,\
         name VARCHAR(32),\
         version VARCHAR(32) NOT NULL,\
         arch VARCHAR,\
         kind CHAR(8) DEFAULT 'app',\
         runtime CHAR(32),\
         uabUrl VARCHAR,\
         repoName CHAR(16),\
         description NVARCHAR,\
         user VARCHAR,\
         installType CHAR(16) DEFAULT 'user',\
         size INTEGER,\
         channel VARCHAR(32),\
         module VARCHAR(32),unique(appId,version,arch,channel,module))";
    auto ret = dbConnection.exec(createInfoTable);
    if (!ret.has_value()) {
        qCritical() << "execute createInfoTable error:" << ret.error().message();
    }
}

// 插入一条应用记录，应该在安装应用时使用
linglong::utils::error::Result<void> LocalDB::insertAppRecord(const package::AppMetaInfo &package,
                                                              const QString &userName)
{
    QString insertSql = "INSERT INTO "
                        "installedAppInfo(appId,name,version,arch,kind,runtime,uabUrl,repoName,"
                        "description,user,size,channel,module) "
                        "VALUES(:appId,:name,:version,:arch,:kind,:runtime,:uabUrl,:repoName,"
                        ":description,:user,:size,:channel,:module)";
    QVariantMap valueMap;
    valueMap.insert(":appId", package.appId);
    valueMap.insert(":name", package.name);
    valueMap.insert(":version", package.version);
    valueMap.insert(":arch", package.arch);
    valueMap.insert(":kind", package.kind);
    valueMap.insert(":runtime", package.runtime);
    valueMap.insert(":uabUrl", package.uabUrl);
    valueMap.insert(":repoName", package.repoName);
    valueMap.insert(":description", package.description);
    valueMap.insert(":size", package.size);
    valueMap.insert(":channel", package.channel);
    valueMap.insert(":module", package.module);
    // 记录应用是谁安装的
    valueMap.insert(":user", userName);

    auto ret = dbConnection.exec(insertSql, valueMap);
    if (!ret.has_value()) {
        return LINGLONG_EWRAP("insertAppRecord", ret.error());
    }
    return LINGLONG_OK;
}

linglong::utils::error::Result<void> LocalDB::deleteAppRecord(const package::AppMetaInfo &package,
                                                              const QString &userName)
{
    QString deleteSql = "DELETE FROM installedAppInfo WHERE "
                        "appId = :appId AND "
                        "version = :version AND "
                        "arch = :arch AND "
                        "channel = :channel AND "
                        "module = :module AND "
                        "user = :user ";
    QVariantMap valueMap;
    valueMap.insert(":appId", package.appId);
    valueMap.insert(":version", package.version);
    valueMap.insert(":arch", package.arch);
    valueMap.insert(":channel", package.channel);
    valueMap.insert(":module", package.module);
    // 记录应用是谁安装的
    valueMap.insert(":user", userName);

    auto ret = dbConnection.exec(deleteSql, valueMap);
    if (!ret.has_value()) {
        return LINGLONG_EWRAP("deleteAppRecord", ret.error());
    }
    return LINGLONG_OK;
}

} // namespace repo
} // namespace linglong