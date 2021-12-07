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

#include "service/impl/dbus_retcode.h"

#include "module/package/package.h"
#include "module/package/pkginfo.h"

namespace linglong {
namespace util {

/*
 * 创建数据库连接
 *
 * @param dbConn: QSqlDatabase 数据库
 *
 * @return int: 0:创建成功 其它:失败
 */
int openDatabaseConnection(QSqlDatabase &dbConn);

/*
 * 检查安装信息数据库表及版本信息表
 *
 * @return int: 0:成功 其它:失败
 */
int checkInstalledAppDb();

/*
 * 更新安装数据库版本
 *
 * @return int: 0:成功 其它:失败
 */
int updateInstalledAppInfoDb();

/*
 * 增加软件包安装信息
 *
 * @param package: 软件包信息
 * @param installType: 软件包安装类型
 * @param userName: 用户名
 *
 * @return int: 0:成功 其它:失败
 */
int insertAppRecord(AppMetaInfo *package, const QString &installType, const QString &userName);

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
int deleteAppRecord(const QString &appId, const QString &appVer, const QString &appArch, const QString &userName);

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
bool getAppInstalledStatus(const QString &appId, const QString &appVer, const QString &appArch,
                           const QString &userName);

/*
 * 查询已安装软件包信息
 *
 * @param appId: 软件包包名
 * @param appVer: 软件包对应的版本号
 * @param appArch: 软件包对应的架构
 * @param userName: 用户名
 * @param pkgList: 查询结果
 *
 * @return bool: true:已安装 false:未安装
 */
bool getInstalledAppInfo(const QString &appId, const QString &appVer, const QString &appArch, const QString &userName,
                         PKGInfoList &pkgList);

/*
 * 查询所有已安装软件包信息
 *
 * @param userName: 用户名
 *
 * @return pkgList:查询结果
 */
PKGInfoList queryAllInstalledApp(const QString &userName = "");
} // namespace util
} // namespace linglong