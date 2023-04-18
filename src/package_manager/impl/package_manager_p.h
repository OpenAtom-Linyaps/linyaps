/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_PACKAGE_MANAGER_PACKAGE_MANAGER_P_H_
#define LINGLONG_SRC_PACKAGE_MANAGER_PACKAGE_MANAGER_P_H_

#include "module/dbus_ipc/package_manager_param.h"
#include "module/dbus_ipc/param_option.h"
#include "module/dbus_ipc/reply.h"
#include "module/dbus_gen_package_manager_helper_interface.h"
#include "module/package/package.h"

namespace linglong {
namespace service {
class PackageManager;

class PackageManagerPrivate : public QObject
{
    Q_OBJECT
public:
    explicit PackageManagerPrivate(PackageManager *parent);
    ~PackageManagerPrivate() override = default;

private:
    Reply Install(const InstallParamOption &installParamOption);
    Reply Uninstall(const UninstallParamOption &paramOption);
    QueryReply Query(const QueryParamOption &paramOption);
    Reply Update(const ParamOption &paramOption);

    /**
     * @brief 查询软件包下载安装状态
     *
     * @param paramOption 查询参数
     * @param type 查询类型 0:查询应用安装进度 1:查询应用更新进度
     *
     * @return Reply dbus方法调用应答 \n
     *          code:状态码 \n
     *          message:信息
     */
    Reply GetDownloadStatus(const ParamOption &paramOption, int type);

    /*
     * 从给定的软件包列表中查找最新版本的runtime
     *
     * @param appId: 待匹配runtime的appId
     * @param appList: 待搜索的软件包列表信息
     *
     * @return AppMetaInfo: 最新版本的runtime
     *
     */
    QSharedPointer<linglong::package::AppMetaInfo>
    getLatestRuntime(const QString &appId,
                     const QString &version,
                     const QList<QSharedPointer<linglong::package::AppMetaInfo>> &appList);
    /*
     * 从给定的软件包列表中查找最新版本的软件包
     *
     * @param appId: 待匹配应用的appId
     * @param appList: 待搜索的软件包列表信息
     *
     * @return AppMetaInfo: 最新版本的软件包
     *
     */
    QSharedPointer<linglong::package::AppMetaInfo>
    getLatestApp(const QString &appId,
                 const QList<QSharedPointer<linglong::package::AppMetaInfo>> &appList);

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
    bool loadAppInfo(const QString &jsonString,
                     QList<QSharedPointer<linglong::package::AppMetaInfo>> &appList,
                     QString &err);

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
    bool getAppInfofromServer(const QString &pkgName,
                              const QString &pkgVer,
                              const QString &pkgArch,
                              QString &appData,
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
    bool downloadAppData(const QString &pkgName,
                         const QString &pkgVer,
                         const QString &pkgArch,
                         const QString &channel,
                         const QString &module,
                         const QString &dstPath,
                         QString &err);

    /*
     * 安装应用runtime
     *
     * @param appInfo: runtime对象
     * @param err: 错误信息
     *
     * @return bool: true:成功 false:失败
     */
    bool installRuntime(QSharedPointer<linglong::package::AppMetaInfo> appInfo, QString &err);

    /*
     * 检查应用runtime安装状态
     *
     * @param runtime: 应用runtime字符串
     * @param channel: 软件包对应的渠道
     * @param module: 软件包类型
     * @param err: 错误信息
     *
     * @return bool: true:安装成功或已安装返回true false:安装失败
     */
    bool checkAppRuntime(const QString &runtime,
                         const QString &channel,
                         const QString &module,
                         QString &err);

    /*
     * 针对非deepin发行版检查应用base安装状态
     *
     * @param runtime: runtime ref
     * @param channel: 软件包对应的渠道
     * @param module: 软件包类型
     * @param err: 错误信息
     *
     * @return bool: true:安装成功或已安装返回true false:安装失败
     */
    bool checkAppBase(const QString &runtime,
                      const QString &channel,
                      const QString &module,
                      QString &err);

    /*
     * 安装应用时更新包括desktop文件在内的配置文件
     *
     * @param appId: 应用的appId
     * @param version: 应用的版本号
     * @param arch: 应用对应的架构
     */
    void addAppConfig(const QString &appId, const QString &version, const QString &arch);

    /*
     * 卸载应用时更新包括desktop文件在内的配置文件
     *
     * @param appId: 应用的appId
     * @param version: 应用的版本号
     * @param arch: 应用对应的架构
     */
    void delAppConfig(const QString &appId, const QString &version, const QString &arch);

    /*
     * 通过用户uid获取对应的用户名
     *
     * @param uid: 用户uid
     *
     * @return QString: uid对应的用户名
     */
    QString getUserName(uid_t uid);

private:
    const QString sysLinglongInstalltions;
    const QString kAppInstallPath;
    const QString kLocalRepoPath;
    QString remoteRepoName = "repo";

    // 记录子线程安装及更新状态 供查询进度信息使用
    QMap<QString, Reply> appState;

    bool noDBusMode = false;

    OrgDeepinLinglongPackageManagerHelperInterface packageManagerHelperInterface;

public:
    PackageManager *const q_ptr;
    Q_DECLARE_PUBLIC(PackageManager);
};
} // namespace service
} // namespace linglong
#endif
