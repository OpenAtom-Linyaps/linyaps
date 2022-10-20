/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     huqinghong <huqinghong@uniontech.com>
 *
 * Maintainer: huqinghong <huqinghong@uniontech.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "appinfo_cache.h"

// app 缓存有效期 分钟为单位
const int kCacheValidTime = 10;

namespace linglong {
namespace util {

/*
 * 创建数据库连接
 *
 * @param dbConn: QSqlDatabase 数据库
 *
 * @return int: 0:成功 其它:失败
 */
int openCacheDbConn(QSqlDatabase &dbConn)
{
    // 多用户支持 deepin-linglong 无home目录
    const QString appInfoCachePath = linglong::util::getLinglongRootPath();
    qDebug() << "app cache path:" << appInfoCachePath;
    QString err = "";
    // 添加数据库驱动，并指定连接名称cache_package_connection
    if (QSqlDatabase::contains("cache_package_connection")) {
        dbConn = QSqlDatabase::database("cache_package_connection");
    } else {
        dbConn = QSqlDatabase::addDatabase("QSQLITE", "cache_package_connection");
        dbConn.setDatabaseName(appInfoCachePath + "/.appInfoCache.db");
    }
    if (!dbConn.isOpen() && !dbConn.open()) {
        err = "open " + appInfoCachePath + " .appInfoCache.db failed";
        qCritical() << err;
        return STATUS_CODE(kFail);
    }
    return STATUS_CODE(kSuccess);
}

/*
 * 检查appinfo cache信息数据库表
 *
 * @return int: 0:成功 其它:失败
 */
int checkAppCache()
{
    QString err = "";
    QSqlDatabase dbConn;
    if (STATUS_CODE(kSuccess) != openCacheDbConn(dbConn)) {
        return STATUS_CODE(kFail);
    }

    QString createCacheTable = "CREATE TABLE IF NOT EXISTS appInfo(\
         ID INTEGER PRIMARY KEY AUTOINCREMENT,\
         key VARCHAR(32) NOT NULL,\
         data VARCHAR,\
         timestamp VARCHAR)";

    QSqlQuery sqlQuery(createCacheTable, dbConn);
    if (!sqlQuery.exec()) {
        err = "fail to create cache appinfo table, err:" + sqlQuery.lastError().text();
        qCritical() << err;
        dbConn.close();
        return STATUS_CODE(kFail);
    }
    dbConn.close();
    return STATUS_CODE(kSuccess);
}

/*
 * 根据关键字从缓存中查询应用缓存数据
 *
 * @param key: 缓存信息关键字
 * @param appData: 查询得到的应用缓存json数据
 *
 * @return int: 0:成功 其它:失败
 */
int queryLocalCache(const QString &key, QString &appData)
{
    QString err = "";
    QString selectSql = QString("SELECT * FROM appInfo WHERE key = '%1'").arg(key);
    QSqlDatabase dbConn;
    if (STATUS_CODE(kSuccess) != openCacheDbConn(dbConn)) {
        return STATUS_CODE(kFail);
    }

    QSqlQuery sqlQuery(dbConn);
    sqlQuery.prepare(selectSql);
    if (!sqlQuery.exec()) {
        err = "queryLocalCache fail to exec sql:" + selectSql + ", error:" + sqlQuery.lastError().text();
        qCritical() << err;
        dbConn.close();
        return STATUS_CODE(kFail);
    }

    sqlQuery.last();
    int recordCount = sqlQuery.at() + 1;
    QString insertSql = "";
    if (recordCount < 1) {
        err = "queryLocalCache fail key:" + key + " not found";
        qDebug() << err;
        dbConn.close();
        return STATUS_CODE(kFail);
    }
    // 检查缓存结果是否有效
    QString recordTime = sqlQuery.value(3).toString().trimmed();
    QDateTime recordDateTime = QDateTime::fromString(recordTime, "yyyy-MM-dd hh:mm:ss");
    QDateTime current = QDateTime::currentDateTime();
    // 10 分钟有效
    QDateTime validDate = recordDateTime.addSecs(kCacheValidTime * 60);
    if (current < validDate) {
        appData = sqlQuery.value(2).toString().trimmed();
        dbConn.close();
        return STATUS_CODE(kSuccess);
    } else {
        QString deleteSql = QString("DELETE FROM appInfo WHERE key = '%1'").arg(key);
        sqlQuery.prepare(deleteSql);
        if (!sqlQuery.exec()) {
            err = "queryLocalCache fail to exec sql:" + deleteSql + ", error:" + sqlQuery.lastError().text();
            qCritical() << err;
            dbConn.close();
            return STATUS_CODE(kFail);
        }
    }
    dbConn.close();
    qDebug() << key << " not valid in cache";
    return STATUS_CODE(kFail);
}

/*
 * 根据关键字更新应用缓存数据
 *
 * @param key: 缓存信息关键字
 * @param appData: 应用缓存json数据
 *
 * @return int: 0:成功 其它:失败
 */
int updateCache(const QString &key, const QString &appData)
{
    QString err = "";
    QSqlDatabase dbConn;
    if (STATUS_CODE(kSuccess) != openCacheDbConn(dbConn)) {
        return STATUS_CODE(kFail);
    }
    // 调用方保证插入cache表前已经调用queryLocalCache查询了cache
    QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    QString insertSql = QString("INSERT INTO appInfo(key,data,timestamp) VALUES(?,?,?)");

    QSqlQuery sqlQuery(dbConn);
    sqlQuery.prepare(insertSql);
    sqlQuery.bindValue(0, key);
    sqlQuery.bindValue(1, appData);
    sqlQuery.bindValue(2, currentTime);
    if (!sqlQuery.exec()) {
        err = "updateCache fail to exec sql:" + insertSql + ", error:" + sqlQuery.lastError().text();
        qCritical() << err;
        dbConn.close();
        return STATUS_CODE(kFail);
    }
    dbConn.close();
    qDebug() << key << " update cache success";
    return STATUS_CODE(kSuccess);
}
} // namespace util
} // namespace linglong
