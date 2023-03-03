/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_PACKAGE_MANAGER_IMPL_APP_STATUS_H_
#define LINGLONG_SRC_PACKAGE_MANAGER_IMPL_APP_STATUS_H_

#include "module/package/package.h"
#include "module/util/connection.h"
#include "module/util/status_code.h"
#include "module/util/version/version.h"

#include <QDateTime>
#include <QJsonArray>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace linglong {
namespace util {
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
int insertAppRecord(linglong::package::AppMetaInfo *package,
                    const QString &userName);

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
                    const QString &userName);

/*
 * 判断软件包类型是否为runtime
 *
 * @param appId: 软件包包名
 *
 * @return bool: true:是 其它: 否
 */
bool isRuntime(const QString &appId);

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
                           const QString &userName);

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
                      linglong::package::AppMetaInfoList &pkgList);

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
 * @return bool: true:已安装 false:未安装
 */
bool getInstalledAppInfo(const QString &appId,
                         const QString &appVer,
                         const QString &appArch,
                         const QString &channel,
                         const QString &module,
                         const QString &userName,
                         linglong::package::AppMetaInfoList &pkgList);

/*
 * 查询所有已安装软件包信息
 *
 * @param userName: 用户名
 * @param result: 查询结果
 * @param err: 错误信息
 *
 * @return bool: true: 成功 false: 失败
 */
bool queryAllInstalledApp(const QString &userName, QString &result, QString &err);

/**
 * @brief 将json字符串转化为软件包列表
 *
 * @param jsonString json字符串
 * @param appList 软件包元信息列表
 *
 * @return bool: true: 成功 false: 失败
 */
bool getAppMetaInfoListByJson(const QString &jsonString,
                              linglong::package::AppMetaInfoList &appList);
} // namespace util
} // namespace linglong
#endif
