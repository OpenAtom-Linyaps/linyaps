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

// 删除一条应用记录，在卸载时使用。在安装时也应该提前删除记录，避免唯一索引冲突
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

// 查询应用记录
linglong::utils::error::Result<QList<QSharedPointer<package::AppMetaInfo>>>
LocalDB::queryAppRecord(const QString &userName,
                        const QString &appID,
                        const QString &arch,
                        const QString &channel,
                        const QString &module,
                        const QString &version)
{
    QString querySql = "SELECT id,appId,name,arch,description,user,version,channel,module FROM "
                       "installedAppInfo WHERE 1=1";
    QVariantMap valueMap;
    if (!userName.isEmpty()) {
        querySql += " AND user = :user";
        valueMap.insert(":user", userName);
    }
    if (!appID.isEmpty()) {
        querySql += " AND appId = :appId";
        valueMap.insert(":appId", appID);
    }
    if (!arch.isEmpty()) {
        querySql += " AND arch = :arch";
        valueMap.insert(":arch", arch);
    }
    if (!channel.isEmpty()) {
        querySql += " AND channel = :channel";
        valueMap.insert(":channel", channel);
    }
    if (!module.isEmpty()) {
        querySql += " AND module = :module";
        valueMap.insert(":module", module);
    }
    if (!version.isEmpty()) {
        querySql += " AND version = :version";
        valueMap.insert(":version", version);
    }
    auto ret = dbConnection.exec(querySql, valueMap);
    if (!ret.has_value()) {
        return LINGLONG_EWRAP("deleteAppRecord", ret.error());
    }
    QList<QSharedPointer<package::AppMetaInfo>> pkgList;
    if (ret->first()) {
        do {
            auto info = QSharedPointer<package::AppMetaInfo>(new package::AppMetaInfo);
            info->appId = ret->value(1).toString().trimmed();
            info->name = ret->value(2).toString().trimmed();
            info->arch = ret->value(3).toString().trimmed();
            info->description = ret->value(4).toString().trimmed();
            info->user = ret->value(5).toString().trimmed();
            info->version = ret->value(6).toString().trimmed();
            info->channel = ret->value(7).toString().trimmed();
            info->module = ret->value(8).toString().trimmed();
            pkgList.push_back(info);
        } while (ret->next());
    }
    return pkgList;
}

} // namespace repo
} // namespace linglong