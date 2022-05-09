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
const QString installedAppInfoPath = "/deepin/linglong/layers/";
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
        dbConn.setDatabaseName(installedAppInfoPath + "InstalledAppInfo.db");
    }
    if (!dbConn.isOpen() && !dbConn.open()) {
        err = "open " + installedAppInfoPath + "InstalledAppInfo.db failed";
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
    QString err = "";
    QSqlDatabase dbConn;
    if (STATUS_CODE(kSuccess) != openDatabaseConnection(dbConn)) {
        return STATUS_CODE(kFail);
    }
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
         installType CHAR(16) DEFAULT 'user')";

    QSqlQuery sqlQuery(createInfoTable, dbConn);
    if (!sqlQuery.exec()) {
        err = "fail to create installed appinfo table, err:" + sqlQuery.lastError().text();
        qCritical() << err;
        closeDbConnection(dbConn);
        return STATUS_CODE(kFail);
    }

    QString createVersionTable = "CREATE TABLE IF NOT EXISTS appInfoDbVersion(\
         version VARCHAR(32) PRIMARY KEY,description NVARCHAR)";
    sqlQuery.prepare(createVersionTable);
    if (!sqlQuery.exec()) {
        err = "fail to create appInfoDbVersion, err:" + sqlQuery.lastError().text();
        qCritical() << err;
        closeDbConnection(dbConn);
        return STATUS_CODE(kFail);
    }
    closeDbConnection(dbConn);
    return STATUS_CODE(kSuccess);
}

/*
 * 更新安装数据库版本
 *
 * @return int: 0:成功 其它:失败
 */
int updateInstalledAppInfoDb()
{
    QString err = "";
    QSqlDatabase dbConn;
    if (STATUS_CODE(kSuccess) != openDatabaseConnection(dbConn)) {
        return STATUS_CODE(kFail);
    }

    // 版本升序排列
    QString selectSql = "SELECT * FROM appInfoDbVersion order by version ASC ";
    QSqlQuery sqlQuery(selectSql, dbConn);
    if (!sqlQuery.exec()) {
        err = "checkAppDbUpgrade fail to exec sql:" + selectSql + ", error:" + sqlQuery.lastError().text();
        qCritical() << err;
        closeDbConnection(dbConn);
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
        sqlQuery.prepare(insertSql);
        if (!sqlQuery.exec()) {
            err = "checkAppDbUpgrade fail to exec sql:" + insertSql + ", error:" + sqlQuery.lastError().text();
            qCritical() << err;
            closeDbConnection(dbConn);
            return STATUS_CODE(kFail);
        }
    }
    closeDbConnection(dbConn);
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
    QString err = "";
    QSqlDatabase dbConn;
    if (STATUS_CODE(kSuccess) != openDatabaseConnection(dbConn)) {
        return STATUS_CODE(kFail);
    }

    QSqlQuery sqlQuery(dbConn);
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

    sqlQuery.prepare(insertSql);
    if (!sqlQuery.exec()) {
        err = "insertAppRecord fail to exec sql:" + insertSql + ", error:" + sqlQuery.lastError().text();
        qCritical() << err;
        closeDbConnection(dbConn);
        return STATUS_CODE(kFail);
    }
    closeDbConnection(dbConn);
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
    QString err = "";
    // 查询是否安装由调用方负责
    QSqlDatabase dbConn;
    if (STATUS_CODE(kSuccess) != openDatabaseConnection(dbConn)) {
        return STATUS_CODE(kFail);
    }

    // 若未指定版本，则查找最高版本
    QString dstVer = appVer;
    QSqlQuery sqlQuery(dbConn);
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
    sqlQuery.prepare(deleteSql);
    if (!sqlQuery.exec()) {
        err = "deleteAppRecord fail to exec sql:" + deleteSql + ", error:" + sqlQuery.lastError().text();
        qCritical() << err;
        closeDbConnection(dbConn);
        return STATUS_CODE(kFail);
    }
    closeDbConnection(dbConn);
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
    QString err = "";
    QSqlDatabase dbConn;
    if (STATUS_CODE(kSuccess) != openDatabaseConnection(dbConn)) {
        return false;
    }
    QSqlQuery sqlQuery(dbConn);

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
    sqlQuery.prepare(selectSql);
    if (!sqlQuery.exec()) {
        err = "getAppInstalledStatus fail to exec sql:" + selectSql + ", error:" + sqlQuery.lastError().text();
        qCritical() << err;
        closeDbConnection(dbConn);
        return false;
    }
    // 指定用户和版本找不到
    // sqlite3不支持size属性
    sqlQuery.last();
    int recordCount = sqlQuery.at() + 1;
    if (recordCount < 1) {
        err = "getAppInstalledStatus app:" + appId + ",version:" + appVer + ",userName:" + userName + " not found";
        qDebug() << err;
        closeDbConnection(dbConn);
        return false;
    }
    closeDbConnection(dbConn);
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
    QString err = "";
    if (!getAppInstalledStatus(appId, appVer, appArch, userName)) {
        err = "getInstalledAppInfo app:" + appId + ",version:" + appVer + ",userName:" + userName + " not installed";
        qCritical() << err;
        return false;
    }

    QSqlDatabase dbConn;
    if (STATUS_CODE(kSuccess) != openDatabaseConnection(dbConn)) {
        return false;
    }

    QSqlQuery sqlQuery(dbConn);
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
    sqlQuery.prepare(selectSql);
    if (!sqlQuery.exec()) {
        err = "getInstalledAppInfo fail to exec sql:" + selectSql + ", error:" + sqlQuery.lastError().text();
        qCritical() << err;
        closeDbConnection(dbConn);
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
        closeDbConnection(dbConn);
        return true;
    }
    closeDbConnection(dbConn);
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
    QSqlDatabase dbConn;
    if (STATUS_CODE(kSuccess) != openDatabaseConnection(dbConn)) {
        err = "openDatabaseConnection err";
        return false;
    }
    QSqlQuery sqlQuery(dbConn);

    // 默认不查找版本
    QString selectSql = "";
    if (userName.isEmpty()) {
        selectSql = QString("SELECT * FROM installedAppInfo");
    } else {
        selectSql = QString("SELECT * FROM installedAppInfo WHERE user = '%1'").arg(userName);
    }
    sqlQuery.prepare(selectSql);
    if (!sqlQuery.exec()) {
        err = "queryAllInstalledApp fail to exec sql:" + selectSql + ", error:" + sqlQuery.lastError().text();
        qCritical() << err;
        closeDbConnection(dbConn);
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
    closeDbConnection(dbConn);
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
