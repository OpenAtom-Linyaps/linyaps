/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_UTIL_CONNECTION_H_
#define LINGLONG_SRC_MODULE_UTIL_CONNECTION_H_

#include "singleton.h"

#include <QMutex>
#include <QMutexLocker>
#include <QQueue>
#include <QString>
#include <QtSql>

#define DATABASE_NAME "linglong.db"

namespace linglong {
namespace util {
class Connection : public QObject
{
    Q_OBJECT

public:
    explicit Connection(QObject *parent = nullptr);
    ~Connection();
    QSqlQuery execute(const QString &sql); // 执行sql语句
    QSqlQuery execute(const QString &sql, const QVariantMap &valueMap);

private:
    QSqlDatabase getConnection();                   // 创建数据库连接
    void closeConnection(QSqlDatabase &connection); // 关闭连接

private:
    QString databaseName; // 如果是 SQLite 则为数据库文件名
    QString databaseType;

    QString testStateSql; // 测试访问数据库的 SQL

    QSqlDatabase connection;

    static QMutex mutex;
};
} // namespace util
} // namespace linglong

Q_DECLARE_LOGGING_CATEGORY(database)
#endif