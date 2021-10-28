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

#include "module/package/package.h"
#include "module/package/pkginfo.h"
#include "module/util/singleton.h"
#include "package_manager_proxy_base.h"
#include "qdbus_retmsg.h"

using namespace linglong::service::util;

// 当前OUAP在线包对应的包名/版本/架构/存储URL信息 to do fix
struct AppStreamPkgInfo {
    QString appId;
    QString appName;
    QString appVer;
    QString appArch;
    QString appUrl;

    QString summary;
    QString runtime;
    QString reponame;

};

class PackageManagerImpl : public PackageManagerProxyBase
    , public Single<PackageManagerImpl>
{
public:
    RetMessageList Download(const QStringList &packageIDList, const QString &savePath);
    RetMessageList Install(const QStringList &packageIDList);
    QString Uninstall(const QStringList &packageIDList);
    PKGInfoList Query(const QStringList &packageIDList);

private:
    AppStreamPkgInfo appStreamPkgInfo;

    /*
     * 查询系统架构
     *
     * @return QString: 系统架构字符串
     */
    QString getHostArch();

    /*
     * 查询当前登陆用户名
     *
     * @return QString: 当前登陆用户名
     */
    QString getUserName();

    /*
     * 根据OUAP在线包数据生成对应的离线包
     *
     * @param cfgPath: OUAP在线包数据存储路径
     * @param dstPath: 生成离线包存储路径
     *
     * @return bool: true:成功 false:失败
     */
    bool makeUAPbyOUAP(const QString &cfgPath, const QString &dstPath);

    /*
     * 更新本地AppStream.json文件
     *
     * @param savePath: AppStream.json文件存储路径
     * @param remoteName: 远端仓库名称
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    bool updateAppStream(const QString &savePath, const QString &remoteName, QString &err);

    /*
     * 根据AppStream.json查询目标软件包信息
     *
     * @param savePath: AppStream.json文件存储路径
     * @param remoteName: 远端仓库名称
     * @param pkgName: 软件包包名
     * @param pkgArch: 软件包对应的架构
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    bool getAppInfoByAppStream(const QString &savePath, const QString &remoteName, const QString &pkgName, const QString &pkgArch, QString &err);

    /*
     * 通过AppStream.json更新OUAP在线包
     *
     * @param xmlPath: AppStream.json文件存储路径
     * @param savePath: OUAP在线包存储路径
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    bool updateOUAP(const QString &xmlPath, const QString &savePath, QString &err);

    /*
     * 查询软件包安装状态
     *
     * @param pkgName: 软件包包名
     * @param userName: 用户名，默认为当前用户
     *
     * @return bool: 1:已安装 0:未安装 -1查询失败
     */
    int getIntallStatus(const QString &pkgName, const QString &userName = "");

    /*
     * 根据OUAP在线包解压出来的uap文件查询软件包信息
     *
     * @param fileDir: OUAP在线包文件解压后的uap文件存储路径
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    bool getAppInfoByOUAPFile(const QString &fileDir, QString &err);

    /*
     * 根据OUAP在线包文件安装软件包
     *
     * @param filePath: OUAP在线包文件路径
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    bool installOUAPFile(const QString &filePath, QString &err);

    /*
     * 解析OUAP在线包中的info.json配置信息
     *
     * @param infoPath: info.json文件存储路径
     *
     * @return bool: true:成功 false:失败
     */
    bool resolveOUAPCfg(const QString &infoPath);

    /*
     * 将OUAP在线包解压到指定目录
     *
     * @param ouapPath: ouap在线包存储路径
     * @param savePath: 解压后文件存储路径
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    bool extractOUAP(const QString &ouapPath, const QString &savePath, QString &err);

    /*
     * 将OUAP在线包数据部分签出到指定目录
     *
     * @param pkgName: 软件包包名
     * @param pkgArch: 软件包对应的架构
     * @param dstPath: OUAP在线包数据部分存储路径
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    bool downloadOUAPData(const QString &pkgName, const QString &pkgArch, const QString &dstPath, QString &err);

    /*
     * 建立box运行应用需要的软链接
     */
    void buildRequestedLink();

    /*
     * 更新应用安装状态到本地文件
     *
     * @param appStreamPkgInfo: 安装成功的软件包信息
     *
     * @return bool: true:成功 false:失败
     */
    bool updateAppStatus(AppStreamPkgInfo appStreamPkgInfo);

    /*
     * 查询未安装软件包信息
     *
     * @param pkgName: 软件包包名
     * @param pkgList: 查询结果
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    bool getUnInstalledAppInfo(const QString &pkgName, PKGInfoList &pkgList, QString &err);

    /*
     * 查询已安装软件包信息
     *
     * @param pkgName: 软件包包名
     * @param pkgList: 查询结果
     *
     * @return bool: true:成功 false:失败(软件包未安装)
     */
    bool getInstalledAppInfo(const QString &pkgName, PKGInfoList &pkgList);

    /*
     * 查询当前用户已安装的软件包
     *
     * @return PKGInfoList: 查询结果
     */
    PKGInfoList queryAllInstalledApp();

    /*
     * 根据匹配的软件包列表查找最新版本软件包信息
     *
     * @param verMap: 目标软件包版本信息
     *
     * @return QString: 最新版本软件包信息
     */
    QString getLatestAppInfo(const QMap<QString, QString> &verMap);

    /*
     * 比较给定的软件包版本
     *
     * @param curVersion: 当前软件包版本
     * @param dstVersion: 目标软件包版本
     *
     * @return bool: ver2 比 ver1版本新返回true,否则返回false
     */
    bool cmpAppVersion(const QString &curVersion, const QString &dstVersion);
};