/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     huqinghong <huqinghong@uniontech.com>
 *
 * Maintainer: huqinghong <huqinghong@uniontech.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include "module/package/package.h"

namespace linglong {
namespace util {

/*
 * 创建数据库连接
 *
 * @param dbConn: QSqlDatabase 数据库
 *
 * @return int: 0:成功 其它:失败
 */
int openCacheDbConn(QSqlDatabase &dbConn);

/*
 * 检查appinfo cache信息数据库表
 *
 * @return int: 0:成功 其它:失败
 */
int checkAppCache();

/*
 * 根据关键字从缓存中查询应用缓存数据
 *
 * @param key: 缓存信息关键字
 * @param appData: 查询得到的应用缓存json数据
 *
 * @return int: 0:成功 其它:失败
 */
int queryLocalCache(const QString &key, QString &appData);

/*
 * 根据关键字更新应用缓存数据
 *
 * @param key: 缓存信息关键字
 * @param appData: 应用缓存json数据
 *
 * @return int: 0:成功 其它:失败
 */
int updateCache(const QString &key, const QString &appData);
} // namespace util
} // namespace linglong