/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     huqinghong <huqinghong@uniontech.com>
 *
 * Maintainer: huqinghong <huqinghong@uniontech.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "app_status.h"

#include <QMutexLocker>
#include <QThread>

// 安装数据库路径
const QString installedAppInfoPath = linglong::util::getLinglongRootPath();
// 安装数据库版本
const QString infoDbVersion = "1.0.0";

namespace linglong {
namespace util {

/*
 * 创建数据库连接
 *
 * @param dbConn: QSqlDatabase 数据库
 *
 * @return int: 0:创建成功 其它:失败
 */
int openDatabaseConnection(QSqlDatabase &dbConn)
{
    QString dbConnName =
        QStringLiteral("installed_package_connection_%1").arg(qintptr(QThread::currentThreadId()), 0, 16);
    static QMutex appSqliteDatabaseMutex;
    QMutexLocker locker(&appSqliteDatabaseMutex);
    QString err = "";
    // 添加数据库驱动，并指定连接名称installed_package_connection
    if (QSqlDatabase::contains(dbConnName)) {
        dbConn = QSqlDatabase::database(dbConnName);
    } else {
        dbConn = QSqlDatabase::addDatabase("QSQLITE", dbConnName);
        dbConn.setDatabaseName(installedAppInfoPath + "/linglong.db");
    }
    if (!dbConn.isOpen() && !dbConn.open()) {
        err = "open " + installedAppInfoPath + "/linglong.db failed";
        qCritical() << err;
        return STATUS_CODE(kFail);
    }
    return STATUS_CODE(kSuccess);
}

/*
 * 关闭数据库连接
 *
 * @param dbConn: QSqlDatabase 数据库连接
 */
void closeDbConnection(QSqlDatabase &dbConn)
{
    QString connName = dbConn.connectionName();
    dbConn.close();
    dbConn = QSqlDatabase();
    QSqlDatabase::removeDatabase(connName);
}

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
         installType CHAR(16) DEFAULT 'user',unique(appId,version,arch))";
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

    // 设置 db 文件权限
    QFile file(installedAppInfoPath + "/" + QString(DATABASE_NAME));
    QFile::Permissions permissions = file.permissions();
    permissions |= QFile::ReadGroup;
    permissions |= QFile::WriteGroup;
    permissions |= QFile::ReadOther;
    permissions |= QFile::WriteOther;
    file.setPermissions(permissions);
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
        insertSql = QString("INSERT INTO appInfoDbVersion VALUES('%1','%2')").arg(infoDbVersion).arg(description);
    } else {
        // fix to do update version
        QString currentVersion = sqlQuery.value(0).toString().trimmed();
        if (currentVersion < infoDbVersion) {
            insertSql = QString("INSERT INTO appInfoDbVersion VALUES('%1','%2')").arg(infoDbVersion).arg(description);
        }
        qDebug() << "installedAppInfoDb currentVersion:" << currentVersion << ", dstVersion:" << infoDbVersion;
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
int insertAppRecord(linglong::package::AppMetaInfo *package, const QString &installType, const QString &userName)
{
    // installType 字段暂时保留
    QString insertSql =
        QString("INSERT INTO installedAppInfo(appId,name,version,arch,kind,runtime,uabUrl,repoName,description,user)\
    VALUES('%1','%2','%3','%4','%5','%6','%7','%8','%9','%10')")
            .arg(package->appId)
            .arg(package->name)
            .arg(package->version)
            //.arg(package.appArch.join(","))
            .arg(package->arch)
            .arg(package->kind)
            .arg(package->runtime)
            .arg(package->uabUrl)
            .arg(package->repoName)
            .arg(package->description)
            .arg(userName);

    Connection connection;
    QSqlQuery sqlQuery = connection.execute(insertSql);
    if (QSqlError::NoError != sqlQuery.lastError().type()) {
        qCritical() << "execute insertSql error:" << sqlQuery.lastError().text();
        return STATUS_CODE(kFail);
    }
    qDebug() << "insertAppRecord app:" << package->appId << ", version:" << package->version << " success";
    return STATUS_CODE(kSuccess);
}

/*
 * 删除软件包安装信息
 *
 * @param appId: 软件包包名
 * @param appVer: 软件包对应的版本号
 * @param appArch: 软件包对应的架构
 * @param userName: 用户名
 *
 * @return int: 0:成功 其它:失败
 */
int deleteAppRecord(const QString &appId, const QString &appVer, const QString &appArch, const QString &userName)
{
    // 若未指定版本，则查找最高版本
    QString dstVer = appVer;
    QString deleteSql =
        QString("DELETE FROM installedAppInfo WHERE appId = '%1' AND version = '%2'").arg(appId).arg(dstVer);
    QString condition = "";
    if (!appArch.isEmpty()) {
        condition.append(QString(" AND arch like '%%1%'").arg(appArch));
    }
    if (!userName.isEmpty()) {
        condition.append(QString(" AND user = '%1'").arg(userName));
    }
    qDebug() << "sql condition:" << condition;
    deleteSql.append(condition);

    Connection connection;
    QSqlQuery sqlQuery = connection.execute(deleteSql);
    if (QSqlError::NoError != sqlQuery.lastError().type()) {
        qCritical() << "execute deleteSql error:" << sqlQuery.lastError().text();
        return STATUS_CODE(kFail);
    }
    qDebug() << "delete app:" << appId << ", version:" << dstVer << ", arch:" << appArch << " success";
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
 * @param userName: 用户名
 *
 * @return bool: true:已安装 false:未安装
 */
bool getAppInstalledStatus(const QString &appId, const QString &appVer, const QString &appArch, const QString &userName)
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
    qDebug() << "sql condition:" << condition;
    selectSql.append(condition);

    Connection connection;
    QSqlQuery sqlQuery = connection.execute(selectSql);
    if (QSqlError::NoError != sqlQuery.lastError().type()) {
        qCritical() << "execute deleteSql error:" << sqlQuery.lastError().text();
        return false;
    }
    // 指定用户和版本找不到
    // sqlite3不支持size属性
    sqlQuery.last();
    int recordCount = sqlQuery.at() + 1;
    if (recordCount < 1) {
        qWarning() << "getAppInstalledStatus app:" + appId + ",version:" + appVer + ",userName:" + userName
                           + " not found";
        return false;
    }
    return true;
}

/*
 * 查询已安装软件包信息
 *
 * @param appId: 软件包包名
 * @param appVer: 软件包对应的版本号
 * @param appArch: 软件包对应的架构
 * @param userName: 用户名
 * @param pkgList: 查询结果
 *
 * @return bool: true:成功 false:失败
 */
bool getInstalledAppInfo(const QString &appId, const QString &appVer, const QString &appArch, const QString &userName,
                         linglong::package::AppMetaInfoList &pkgList)
{
    if (!getAppInstalledStatus(appId, appVer, appArch, userName)) {
        qCritical() << "getInstalledAppInfo app:" + appId + ",version:" + appVer + ",userName:" + userName
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
    qDebug() << "sql condition:" << condition;
    selectSql.append(condition);
    selectSql.append(" order by version ASC");
    qDebug() << selectSql;

    Connection connection;
    QSqlQuery sqlQuery = connection.execute(selectSql);
    if (QSqlError::NoError != sqlQuery.lastError().type()) {
        qCritical() << "execute selectSql error:" << sqlQuery.lastError().text();
        return false;
    }

    // 多个版本返回最高版本信息
    sqlQuery.last();
    int recordCount = sqlQuery.at() + 1;
    if (recordCount > 0) {
        auto info = QPointer<linglong::package::AppMetaInfo>(new linglong::package::AppMetaInfo);
        info->appId = sqlQuery.value(1).toString().trimmed();
        info->name = sqlQuery.value(2).toString().trimmed();
        info->arch = sqlQuery.value(4).toString().trimmed();
        info->description = sqlQuery.value(9).toString().trimmed();
        info->user = sqlQuery.value(10).toString().trimmed();
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
        selectSql = QString("SELECT * FROM installedAppInfo");
    } else {
        selectSql = QString("SELECT * FROM installedAppInfo WHERE user = '%1'").arg(userName);
    }

    Connection connection;
    QSqlQuery sqlQuery = connection.execute(selectSql);
    if (QSqlError::NoError != sqlQuery.lastError().type()) {
        qCritical() << "execute selectSql error:" << sqlQuery.lastError().text();
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
bool getAppMetaInfoListByJson(const QString &jsonString, linglong::package::AppMetaInfoList &appList)
{
    QJsonParseError parseJsonErr;
    QJsonDocument document = QJsonDocument::fromJson(jsonString.toUtf8(), &parseJsonErr);
    if (jsonString.isEmpty() || QJsonParseError::NoError != parseJsonErr.error) {
        qDebug() << "parse json data err " << jsonString;
        return false;
    }
    QJsonArray array(document.array());
    for (int i = 0; i < array.size(); ++i) {
        QJsonObject dataObj = array.at(i).toObject();
        const QString jsonItem = QString(QJsonDocument(dataObj).toJson(QJsonDocument::Compact));
        auto appItem = linglong::util::loadJSONString<linglong::package::AppMetaInfo>(jsonItem);
        appList.push_back(appItem);
    }
    return true;
}

} // namespace util
} // namespace linglong
