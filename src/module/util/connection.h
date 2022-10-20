/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     yuanqiliang <yuanqiliang@uniontech.com>
 *
 * Maintainer: yuanqiliang <yuanqiliang@uniontech.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QtSql>
#include <QQueue>
#include <QString>
#include <QMutex>
#include <QMutexLocker>

#include "singleton.h"

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
    QSqlDatabase getConnection(); // 创建数据库连接
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