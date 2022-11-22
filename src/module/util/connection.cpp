/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "connection.h"

#include "module/util/file.h"

#define DATABASE_TYPE "QSQLITE"
#define TEST_STATE_SQL "SELECT 1"

Q_LOGGING_CATEGORY(database, "linglong.database", QtWarningMsg)

QMutex linglong::util::Connection::mutex;

namespace linglong {
namespace util {
Connection::Connection(QObject *parent)
    : QObject(parent)
    , databaseName(getLinglongRootPath() + "/" + QString(DATABASE_NAME))
    , databaseType(DATABASE_TYPE)
    , testStateSql(TEST_STATE_SQL)
{
    qCDebug(database) << "open databaseName" << databaseName;
}

Connection::~Connection()
{
    closeConnection(connection);
    qCDebug(database) << "close databaseName" << databaseName;
}

QSqlDatabase Connection::getConnection()
{
    QSqlDatabase connection;
    QString connectionName = QStringLiteral("connection_%1").arg(qintptr(QThread::currentThreadId()), 0, 16);
    // 连接已经创建过了，复用它，而不是重新创建
    if (QSqlDatabase::contains(connectionName)) {
        connection = QSqlDatabase::database(connectionName);
        return connection;
    }

    // 创建一个新的连接
    connection = QSqlDatabase::addDatabase(databaseType, connectionName);
    // 设置sqlite数据库路径
    connection.setDatabaseName(databaseName);
    if (!connection.open()) {
        qCritical() << "open database failed:" << connection.lastError().text();
    } else {
        QSqlQuery query(testStateSql, connection);
        if (QSqlError::NoError != query.lastError().type()) {
            qCritical() << "open database error:" << query.lastError().text();
        }
    }

    return connection;
}

void Connection::closeConnection(QSqlDatabase &connection)
{
    QMutexLocker locker(&mutex); // 加锁，加锁不成功则阻塞
    QString connectionName = connection.connectionName();
    connection.close();
    connection = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
    qCDebug(database) << "connectionNames:" << QSqlDatabase::connectionNames();
}

QSqlQuery Connection::execute(const QString &sql)
{
    QMutexLocker locker(&mutex); // 加锁，加锁不成功则阻塞

    connection = getConnection();
    QSqlQuery query(sql, connection);
    if (QSqlError::NoError != query.lastError().type()) {
        qCritical() << "execute sql error:" << query.lastError().text();
    }
    return query;
}

QSqlQuery Connection::execute(const QString &sql, const QVariantMap &valueMap)
{
    QMutexLocker locker(&mutex);
    connection = getConnection();
    QSqlQuery query(connection);
    query.prepare(sql);
    for (const auto &key : valueMap.keys()) {
        if (":size" == key) {
            query.bindValue(key, valueMap.value(key).toInt());
            continue;
        }
        query.bindValue(key, valueMap.value(key).toString());
    }
    query.exec();
    if (QSqlError::NoError != query.lastError().type()) {
        qCritical() << "execute pre sql error:" << query.lastError().text();
    }
    return query;
}

} // namespace util
} // namespace linglong
