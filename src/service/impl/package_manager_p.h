/*
 * Copyright (c) 2022. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     yuanqiliang <yuanqiliang@uniontech.com>
 *
 * Maintainer: yuanqiliang <yuanqiliang@uniontech.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "module/runtime/app.h"
#include "module/repo/repo.h"
#include "module/repo/ostree.h"
#include "module/repo/repohelper_factory.h"
#include "module/util/app_status.h"
#include "module/util/appinfo_cache.h"
#include "module/util/httpclient.h"
#include "module/util/package_manager_param.h"
#include "module/util/sysinfo.h"
#include "module/util/runner.h"
#include "module/util/version.h"

namespace linglong {
namespace service {
class PackageManager;
class PackageManagerPrivate
    : public QObject
    , public PackageManagerInterface
{
    Q_OBJECT
public:
    explicit PackageManagerPrivate(PackageManager *parent);
    ~PackageManagerPrivate() override = default;

private:
    Reply Download(const DownloadParamOption &downloadParamOption);
    Reply Install(const InstallParamOption &installParamOption);
    Reply Uninstall(const UninstallParamOption &paramOption);
    QueryReply Query(const QueryParamOption &paramOption);
    Reply Update(const ParamOption &paramOption);

    /**
     * @brief 查询软件包下载安装状态
     *
     * @param paramOption 查询参数
     *
     * @return Reply dbus方法调用应答 \n
     *          code:状态码 \n
     *          message:信息
     */
    Reply GetDownloadStatus(const ParamOption &paramOption);

    /*
     * 从给定的软件包列表中查找最新版本的软件包
     *
     * @param appList: 待搜索的软件包列表信息
     *
     * @return AppMetaInfo: 最新版本的软件包
     *
     */
    linglong::package::AppMetaInfo *getLatestApp(const linglong::package::AppMetaInfoList &appList);

    /*
     * 从json字符串中提取软件包对应的JsonArray数据
     *
     * @param jsonString: 软件包对应的json字符串
     * @param jsonValue: 转换结果
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    bool getAppJsonArray(const QString &jsonString, QJsonValue &jsonValue, QString &err);

    /*
     * 将json字符串转化为软件包数据
     *
     * @param jsonString: 软件包对应的json字符串
     * @param appList: 转换结果
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    bool loadAppInfo(const QString &jsonString, linglong::package::AppMetaInfoList &appList, QString &err);

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
    bool getAppInfofromServer(const QString &pkgName, const QString &pkgVer, const QString &pkgArch, QString &appData,
                              QString &err);
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
     * @param runtimeId: runtime对应的appId
     * @param runtimeVer: runtime版本号
     * @param runtimeArch: runtime对应的架构
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    bool installRuntime(const QString &runtimeId, const QString &runtimeVer, const QString &runtimeArch, QString &err);

    /*
     * 检查应用runtime安装状态
     *
     * @param runtime: 应用runtime字符串
     * @param err: 错误信息
     *
     * @return bool: true:安装成功或已安装返回true false:安装失败
     */
    bool checkAppRuntime(const QString &runtime, QString &err);

private:
    QMap<QString, QPointer<linglong::runtime::App>> apps;
    linglong::repo::OSTree repo;
    const QString sysLinglongInstalltions;
    const QString kAppInstallPath;
    const QString kLocalRepoPath;
    const QString kRemoteRepoName;
    // Fix to do 记录子线程安装状态 供查询进度信息使用 未序列化 重启service后失效
    QMap<QString, Reply> appState;

public:
    PackageManager *const q_ptr;
    Q_DECLARE_PUBLIC(PackageManager);
};
} // namespace service
} // namespace linglong
