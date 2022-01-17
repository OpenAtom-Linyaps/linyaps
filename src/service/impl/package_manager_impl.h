/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     huqinghong@uniontech.com
 *
 * Maintainer: huqinghong@uniontech.com
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QDebug>
#include <QDBusArgument>
#include <QDBusContext>
#include <QJsonArray>
#include <QList>
#include <QObject>
#include <QProcess>
#include <QSysInfo>
#include <QThread>

#include <DSingleton>

#include "module/package/package.h"
#include "module/util/fs.h"
#include "package_manager_proxy_base.h"
#include "qdbus_retmsg.h"

class PackageManagerImpl : public PackageManagerProxyBase
    , public Dtk::Core::DSingleton<PackageManagerImpl>
{
public:
    RetMessageList Download(const QStringList &packageIdList, const QString &savePath);
    RetMessageList Install(const QStringList &packageIdList, const ParamStringMap &paramMap = {});
    RetMessageList Uninstall(const QStringList &packageIdList, const ParamStringMap &paramMap = {});
    AppMetaInfoList Query(const QStringList &packageIdList, const ParamStringMap &paramMap = {});

private:
    const QString sysLinglongInstalltions = "/deepin/linglong/entries/share";

    /*
     * 将json字符串转化为软件包数据
     *
     * @param jsonString: 软件包对应的json字符串
     * @param appList: 转换结果
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    bool loadAppInfo(const QString &jsonString, AppMetaInfoList &appList, QString &err);

    /*
     * 从服务器查询指定包名/版本/架构的软件包数据
     *
     * @param pkgName: 软件包包名
     * @param pkgVer: 软件包版本号
     * @param pkgArch: 软件包对应的架构
     * @param appData: 查询结果
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    bool getAppInfofromServer(const QString &pkgName, const QString &pkgVer, const QString &pkgArch,
                                              QString &appData, QString &err);
    /*
     * 将在线包数据部分签出到指定目录
     *
     * @param pkgName: 软件包包名
     * @param pkgVer: 软件包版本号
     * @param pkgArch: 软件包对应的架构
     * @param dstPath: 在线包数据部分存储路径
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    bool downloadAppData(const QString &pkgName, const QString &pkgVer, const QString &pkgArch, const QString &dstPath,
                         QString &err);

    /*
     * 安装应用runtime
     *
     * @param runtimeID: runtime对应的appID
     * @param runtimeVer: runtime版本号
     * @param runtimeArch: runtime对应的架构
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    bool installRuntime(const QString &runtimeID, const QString &runtimeVer, const QString &runtimeArch, QString &err);

    /*
     * 检查应用runtime安装状态
     *
     * @param runtime: 应用runtime字符串
     * @param err: 错误信息
     *
     * @return bool: true:安装成功或已安装返回true false:安装失败
     */
    bool checkAppRuntime(const QString &runtime, QString &err);
};