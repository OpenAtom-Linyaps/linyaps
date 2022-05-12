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
#include "file.h"

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

private:
    QSqlDatabase getConnection(); // 创建数据库连接
    void closeConnection(QSqlDatabase &connection); // 关闭连接

private:
    QString databaseName; // 如果是 SQLite 则为数据库文件名
    QString databaseType;

    bool testOnBorrow; // 取得连接的时候验证连接是否有效
    QString testOnBorrowSql; // 测试访问数据库的 SQL

    QSqlDatabase connection;

    static QMutex mutex;
};
} // namespace util
} // namespace linglong
