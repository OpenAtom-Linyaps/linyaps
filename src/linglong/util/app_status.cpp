/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "app_status.h"

#include "linglong/util/connection.h"
#include "linglong/util/file.h"
#include "linglong/util/status_code.h"
#include "linglong/util/version/version.h"

#include <QMutexLocker>
#include <QThread>

// 安装数据库路径
const QString installedAppInfoPath = linglong::util::getLinglongRootPath();
// 安装数据库版本
const QString infoDbVersion = "1.0.0";

namespace linglong {
namespace util {

/*
 * 检查安装信息数据库表及版本信息表
 *
 * @return int: 0:成功 其它:失败
 */
int checkInstalledAppDb()
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
    Connection connection;
    QSqlQuery sqlQuery = connection.execute(createInfoTable);
    if (QSqlError::NoError != sqlQuery.lastError().type()) {
        qCritical() << "execute createInfoTable error:" << sqlQuery.lastError().text();
        return STATUS_CODE(kFail);
    }

    QString createVersionTable = "CREATE TABLE IF NOT EXISTS appInfoDbVersion(\
         version VARCHAR(32) PRIMARY KEY,description NVARCHAR)";
    sqlQuery = connection.execute(createVersionTable);
    if (QSqlError::NoError != sqlQuery.lastError().type()) {
        qCritical() << "fail to create appInfoDbVersion, err:" << sqlQuery.lastError().text();
        return STATUS_CODE(kFail);
    }

    return STATUS_CODE(kSuccess);
}

/*
 * 更新安装数据库版本
 *
 * @return int: 0:成功 其它:失败
 */
int updateInstalledAppInfoDb()
{
    // 版本升序排列
    QString selectSql = "SELECT * FROM appInfoDbVersion order by version ASC ";
    Connection connection;
    QSqlQuery sqlQuery = connection.execute(selectSql);
    if (QSqlError::NoError != sqlQuery.lastError().type()) {
        qCritical() << "execute selectSql error:" << sqlQuery.lastError().text();
        return STATUS_CODE(kFail);
    }

    // sqlite3不支持size属性
    sqlQuery.last();
    int recordCount = sqlQuery.at() + 1;
    // 首次安装
    QString description = "create appinfodb version";
    QString insertSql = "";
    if (recordCount < 1) {
        insertSql = QString("INSERT INTO appInfoDbVersion VALUES('%1','%2')")
                            .arg(infoDbVersion)
                            .arg(description);
    } else {
        // fix to do update version
        QString currentVersion = sqlQuery.value(0).toString().trimmed();
        if (currentVersion < infoDbVersion) {
            insertSql = QString("INSERT INTO appInfoDbVersion VALUES('%1','%2')")
                                .arg(infoDbVersion)
                                .arg(description);
        }
        qDebug() << "installedAppInfoDb currentVersion:" << currentVersion
                 << ", dstVersion:" << infoDbVersion;
    }
    if (!insertSql.isEmpty()) {
        sqlQuery = connection.execute(insertSql);
        if (QSqlError::NoError != sqlQuery.lastError().type()) {
            qCritical() << "execute insertSql error:" << sqlQuery.lastError().text();
            return STATUS_CODE(kFail);
        }
    }

    return STATUS_CODE(kSuccess);
}

/*
 * 增加软件包安装信息
 *
 * @param package: 软件包信息
 * @param installType: 软件包安装类型
 * @param userName: 用户名
 *
 * @return int: 0:成功 其它:失败
 */
int insertAppRecord(QSharedPointer<linglong::package::AppMetaInfo> package, const QString &userName)
{
    Connection connection;
    QString insertSql = "INSERT INTO "
                        "installedAppInfo(appId,name,version,arch,kind,runtime,uabUrl,repoName,"
                        "description,user,size,channel,module) "
                        "VALUES(:appId,:name,:version,:arch,:kind,:runtime,:uabUrl,:repoName,:"
                        "description,:user,:size,:channel,:module)";
    QVariantMap valueMap;
    valueMap.insert(":appId", package->appId);
    valueMap.insert(":name", package->name);
    valueMap.insert(":version", package->version);
    valueMap.insert(":arch", package->arch);
    valueMap.insert(":kind", package->kind);
    valueMap.insert(":runtime", package->runtime);
    valueMap.insert(":uabUrl", package->uabUrl);
    valueMap.insert(":repoName", package->repoName);
    valueMap.insert(":description", package->description);
    valueMap.insert(":user", userName);
    valueMap.insert(":size", package->size);
    valueMap.insert(":channel", package->channel);
    valueMap.insert(":module", package->module);
    QSqlQuery sqlQuery = connection.execute(insertSql, valueMap);
    if (QSqlError::NoError != sqlQuery.lastError().type()) {
        qCritical() << "execute insertSql error:" << sqlQuery.lastError().text();
        return STATUS_CODE(kFail);
    }
    qDebug() << "insertAppRecord app:" << package->appId << ", version:" << package->version
             << " success";
    return STATUS_CODE(kSuccess);
}

/*
 * 删除软件包安装信息
 *
 * @param appId: 软件包包名
 * @param appVer: 软件包对应的版本号
 * @param appArch: 软件包对应的架构
 * @param channel: 软件包对应的渠道
 * @param module: 软件包类型
 * @param userName: 用户名
 *
 * @return int: 0:成功 其它:失败
 */
int deleteAppRecord(const QString &appId,
                    const QString &appVer,
                    const QString &appArch,
                    const QString &channel,
                    const QString &module,
                    const QString &userName)
{
    // 若未指定版本，则查找最高版本
    QString dstVer = appVer;
    QString deleteSql =
            QString("DELETE FROM installedAppInfo WHERE appId = '%1' AND version = '%2'")
                    .arg(appId)
                    .arg(dstVer);
    QString condition = "";
    if (!appArch.isEmpty()) {
        condition.append(QString(" AND arch like '%%1%'").arg(appArch));
    }

    if (!channel.isEmpty()) {
        condition.append(QString(" AND channel = '%1'").arg(channel));
    }
    if (!module.isEmpty()) {
        condition.append(QString(" AND module = '%1'").arg(module));
    }

    if (!userName.isEmpty()) {
        condition.append(QString(" AND user = '%1'").arg(userName));
    }
    deleteSql.append(condition);

    qDebug().noquote() << "sql:" << deleteSql;

    Connection connection;
    QSqlQuery sqlQuery = connection.execute(deleteSql);
    if (QSqlError::NoError != sqlQuery.lastError().type()) {
        qCritical() << "execute deleteSql error:" << sqlQuery.lastError().text();
        return STATUS_CODE(kFail);
    }
    qDebug() << "delete app:" << appId << ", version:" << dstVer << ", arch:" << appArch
             << " success";
    return STATUS_CODE(kSuccess);
}

/*
 * 判断软件包类型是否为runtime
 *
 * @param appId: 软件包包名
 *
 * @return bool: true:是 其它: 否
 */
bool isRuntime(const QString &appId)
{
    // FIXME: 通过元信息判断是否为runtime
    const QString runtimeId = "org.deepin.Runtime";
    const QString wineRuntimeId = "org.deepin.Wine";
    if (runtimeId == appId || wineRuntimeId == appId) {
        return true;
    }
    return false;
}

/*
 * 查询软件包是否安装
 *
 * @param appId: 软件包包名
 * @param appVer: 软件包对应的版本号
 * @param appArch: 软件包对应的架构
 * @param channel: 软件包对应的渠道
 * @param module: 软件包类型
 * @param userName: 用户名
 *
 * @return bool: true:已安装 false:未安装
 */
bool getAppInstalledStatus(const QString &appId,
                           const QString &appVer,
                           const QString &appArch,
                           const QString &channel,
                           const QString &module,
                           const QString &userName)
{
    QString selectSql = QString("SELECT * FROM installedAppInfo WHERE appId = '%1'").arg(appId);
    QString condition = "";
    // FIXME: 预装应用类型应该为system，目前未实现
    // 暂时不用区分用户，当前应用安装目录未区分用户
    // 若区分用户，当前升级镜像后切普通用户预装应用可以重复安装
    // runtime不区分用户
    if (!isRuntime(appId) && !userName.isEmpty()) {
        condition.append(QString(" AND user = '%1'").arg(userName));
    }
    if (!appVer.isEmpty()) {
        condition.append(QString(" AND version = '%1'").arg(appVer));
    }
    if (!appArch.isEmpty()) {
        condition.append(QString(" AND arch like '%%1%'").arg(appArch));
    }
    if (!channel.isEmpty()) {
        condition.append(QString(" AND channel = '%1'").arg(channel));
    }
    if (!module.isEmpty()) {
        condition.append(QString(" AND module = '%1'").arg(module));
    }
    selectSql.append(condition);

    qDebug().noquote() << "sql:" << selectSql;
    Connection connection;
    QSqlQuery sqlQuery = connection.execute(selectSql);
    if (QSqlError::NoError != sqlQuery.lastError().type()) {
        qCritical() << "execute sql error:" << sqlQuery.lastError().text();
        return false;
    }
    // 指定用户和版本找不到
    // sqlite3不支持size属性
    sqlQuery.last();
    int recordCount = sqlQuery.at() + 1;
    if (recordCount < 1) {
        qDebug() << "getAppInstalledStatus app:" + appId + ",version:" + appVer
                        + ",channel:" + channel + ",module:" + module + ",userName:" + userName
                        + " not installed";
        return false;
    }
    return true;
}

/*
 * 查询已安装软件包的所有版本信息
 *
 * @param appId: 软件包包名
 * @param appVer: 软件包对应的版本号
 * @param appArch: 软件包对应的架构
 * @param userName: 用户名
 * @param pkgList: 查询结果
 *
 * @return bool: true:成功 false:失败
 */
bool getAllVerAppInfo(const QString &appId,
                      const QString &appVer,
                      const QString &appArch,
                      const QString &userName,
                      QList<QSharedPointer<linglong::package::AppMetaInfo>> &pkgList)
{
    if (!getAppInstalledStatus(appId, appVer, appArch, "", "", userName)) {
        qCritical() << "getAllVerAppInfo app:" + appId + ",version:" + appVer
                        + ",userName:" + userName + " not installed";
        return false;
    }

    QString selectSql = QString("SELECT * FROM installedAppInfo WHERE appId = '%1'").arg(appId);
    QString condition = "";
    if (!userName.isEmpty()) {
        condition.append(QString(" AND user = '%1'").arg(userName));
    }
    if (!appVer.isEmpty()) {
        condition.append(QString(" AND version = '%1'").arg(appVer));
    }
    if (!appArch.isEmpty()) {
        condition.append(QString(" AND arch like '%%1%'").arg(appArch));
    }

    selectSql.append(condition);
    selectSql.append(" order by version ASC");
    qDebug() << selectSql;

    Connection connection;
    QSqlQuery sqlQuery = connection.execute(selectSql);
    if (QSqlError::NoError != sqlQuery.lastError().type()) {
        qCritical() << "execute selectSql error:" << sqlQuery.lastError().text();
        return false;
    }
    sqlQuery.first();
    do {
        auto info =
                QSharedPointer<linglong::package::AppMetaInfo>(new linglong::package::AppMetaInfo);
        info->appId = sqlQuery.value(1).toString().trimmed();
        info->name = sqlQuery.value(2).toString().trimmed();
        info->arch = sqlQuery.value(4).toString().trimmed();
        info->description = sqlQuery.value(9).toString().trimmed();
        info->user = sqlQuery.value(10).toString().trimmed();
        info->version = sqlQuery.value(3).toString().trimmed();
        info->channel = sqlQuery.value(13).toString().trimmed();
        info->module = sqlQuery.value(14).toString().trimmed();
        pkgList.push_back(info);

    } while (sqlQuery.next());
    return true;
}

/*
 * 查询已安装软件包信息
 *
 * @param appId: 软件包包名
 * @param appVer: 软件包对应的版本号
 * @param appArch: 软件包对应的架构
 * @param channel: 软件包对应的渠道
 * @param module: 软件包类型
 * @param userName: 用户名
 * @param pkgList: 查询结果
 *
 * @return bool: true:成功 false:失败
 */
bool getInstalledAppInfo(const QString &appId,
                         const QString &appVer,
                         const QString &appArch,
                         const QString &channel,
                         const QString &module,
                         const QString &userName,
                         QList<QSharedPointer<linglong::package::AppMetaInfo>> &pkgList)
{
    if (!getAppInstalledStatus(appId, appVer, appArch, channel, module, userName)) {
        qCritical() << "getInstalledAppInfo app:" + appId + ",version:" + appVer
                        + ",channel:" + channel + ",module:" + module + ",userName:" + userName
                        + " not installed";
        return false;
    }

    QString selectSql = QString("SELECT * FROM installedAppInfo WHERE appId = '%1'").arg(appId);
    QString condition = "";
    if (!userName.isEmpty()) {
        condition.append(QString(" AND user = '%1'").arg(userName));
    }
    if (!appVer.isEmpty()) {
        condition.append(QString(" AND version = '%1'").arg(appVer));
    }
    if (!appArch.isEmpty()) {
        condition.append(QString(" AND arch like '%%1%'").arg(appArch));
    }
    if (!channel.isEmpty()) {
        condition.append(QString(" AND channel = '%1'").arg(channel));
    }
    if (!module.isEmpty()) {
        condition.append(QString(" AND module = '%1'").arg(module));
    }

    selectSql.append(condition);
    selectSql.append(" order by version ASC");
    qDebug() << selectSql;

    Connection connection;
    QSqlQuery sqlQuery = connection.execute(selectSql);
    if (QSqlError::NoError != sqlQuery.lastError().type()) {
        qCritical() << "execute selectSql error:" << sqlQuery.lastError().text();
        return false;
    }

    // int fieldNo = sqlQuery.record().indexOf("appId");
    // 多个版本返回最高版本信息
    sqlQuery.last();
    int recordCount = sqlQuery.at() + 1;
    if (recordCount > 0) {
        auto info =
                QSharedPointer<linglong::package::AppMetaInfo>(new linglong::package::AppMetaInfo);
        info->appId = sqlQuery.value(1).toString().trimmed();
        info->name = sqlQuery.value(2).toString().trimmed();
        info->arch = sqlQuery.value(4).toString().trimmed();
        info->description = sqlQuery.value(9).toString().trimmed();
        info->user = sqlQuery.value(10).toString().trimmed();
        info->channel = sqlQuery.value(13).toString().trimmed();
        info->module = sqlQuery.value(14).toString().trimmed();
        QString dstVer = sqlQuery.value(3).toString().trimmed();
        // sqlite逐字符比较导致获取的最高版本号不准，e.g: 5.9.1比5.10.1版本高
        sqlQuery.first();
        do {
            QString verIter = sqlQuery.value(3).toString().trimmed();
            linglong::util::AppVersion versionIter(verIter);
            linglong::util::AppVersion dstVersion(dstVer);
            if (versionIter.isValid() && versionIter.isBigThan(dstVersion)) {
                dstVer = verIter;
                info->description = sqlQuery.value(9).toString().trimmed();
            }
        } while (sqlQuery.next());
        info->version = dstVer;
        pkgList.push_back(info);
        return true;
    }
    return false;
}

/*
 * 查询所有已安装软件包信息
 *
 * @param userName: 用户名
 * @param result: 查询结果
 * @param err: 错误信息
 *
 * @return bool: true: 成功 false: 失败
 */
bool queryAllInstalledApp(const QString &userName, QString &result, QString &err)
{
    // 默认不查找版本
    QString selectSql = "";
    if (userName.isEmpty()) {
        selectSql = QString("SELECT * FROM installedAppInfo order by appId,version");
    } else {
        selectSql =
                QString("SELECT * FROM installedAppInfo WHERE user = '%1' order by appId,version")
                        .arg(userName);
    }

    Connection connection;
    QSqlQuery sqlQuery = connection.execute(selectSql);
    if (QSqlError::NoError != sqlQuery.lastError().type()) {
        qCritical() << "execute selectSql error:" << sqlQuery.lastError().text();
        err = "SQL error check log for detail";
        return false;
    }
    QJsonArray appList;
    while (sqlQuery.next()) {
        QJsonObject appItem;
        appItem["appId"] = sqlQuery.value(1).toString().trimmed();
        appItem["name"] = sqlQuery.value(2).toString().trimmed();
        appItem["version"] = sqlQuery.value(3).toString().trimmed();
        appItem["arch"] = sqlQuery.value(4).toString().trimmed();
        appItem["kind"] = sqlQuery.value(5).toString().trimmed();
        appItem["runtime"] = sqlQuery.value(6).toString().trimmed();
        appItem["uabUrl"] = sqlQuery.value(7).toString().trimmed();
        appItem["repoName"] = sqlQuery.value(8).toString().trimmed();
        appItem["description"] = sqlQuery.value(9).toString().trimmed();

        appItem["channel"] = sqlQuery.value(13).toString().trimmed();
        appItem["module"] = sqlQuery.value(14).toString().trimmed();
        appList.append(appItem);
    }
    QJsonDocument document = QJsonDocument(appList);
    result = QString(document.toJson());
    return true;
}

/**
 * @brief 将json字符串转化为软件包列表
 *
 * @param jsonString json字符串
 * @param appList 软件包元信息列表
 *
 * @return bool: true: 成功 false: 失败
 */
bool getAppMetaInfoListByJson(const QString &jsonString,
                              QList<QSharedPointer<linglong::package::AppMetaInfo>> &appList)
{
    QJsonParseError parseJsonErr;
    QJsonDocument document = QJsonDocument::fromJson(jsonString.toUtf8(), &parseJsonErr);
    if (jsonString.isEmpty() || QJsonParseError::NoError != parseJsonErr.error) {
        return false;
    }
    QJsonArray array(document.array());
    for (int i = 0; i < array.size(); ++i) {
        QJsonObject dataObj = array.at(i).toObject();
        const QString jsonItem = QString(QJsonDocument(dataObj).toJson(QJsonDocument::Compact));
        auto appItem = linglong::util::loadJsonString<linglong::package::AppMetaInfo>(jsonItem);
        appList.push_back(QSharedPointer<linglong::package::AppMetaInfo>(appItem));
    }
    return true;
}

} // namespace util
} // namespace linglong
