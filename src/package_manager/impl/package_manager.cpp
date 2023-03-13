/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "package_manager.h"

#include "module/dbus_ipc/dbus_system_helper_common.h"
#include "module/repo/ostree_repohelper.h"
#include "module/util/app_status.h"
#include "module/util/appinfo_cache.h"
#include "module/util/file.h"
#include "module/util/http/httpclient.h"
#include "module/util/runner.h"
#include "module/util/status_code.h"
#include "module/util/sysinfo.h"
#include "module/util/version/semver.h"
#include "module/util/version/version.h"
#include "package_manager_p.h"

#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>
#include <QJsonArray>
#include <QSettings>

#include <pwd.h>
#include <sys/types.h>

namespace linglong {
namespace service {
PackageManagerPrivate::PackageManagerPrivate(PackageManager *parent)
    : sysLinglongInstalltions(linglong::util::getLinglongRootPath() + "/entries/share")
    , kAppInstallPath(linglong::util::getLinglongRootPath() + "/layers/")
    , kLocalRepoPath(linglong::util::getLinglongRootPath())
    , systemHelperInterface(linglong::SystemHelperDBusName,
                            linglong::SystemHelperDBusPath,
                            []() -> QDBusConnection {
                                auto address = QString(getenv("LINGLONG_SYSTEM_HELPER_ADDRESS"));
                                if (address.length()) {
                                    return QDBusConnection::connectToPeer(address,
                                                                          "ll-package-manager");
                                }
                                return QDBusConnection::systemBus();
                            }())
    , q_ptr(parent)
{
    linglong::util::getLocalConfig("repoName", remoteRepoName);
}

/*
 * 从json字符串中提取软件包对应的JsonArray数据
 *
 * @param jsonString: 软件包对应的json字符串
 * @param jsonValue: 转换结果
 * @param err: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
bool PackageManagerPrivate::getAppJsonArray(const QString &jsonString,
                                            QJsonValue &jsonValue,
                                            QString &err)
{
    QJsonParseError parseJsonErr;
    QJsonDocument document = QJsonDocument::fromJson(jsonString.toUtf8(), &parseJsonErr);
    if (QJsonParseError::NoError != parseJsonErr.error) {
        err = "parse server's json data failed, please check the network " + jsonString;
        return false;
    }

    QJsonObject jsonObject = document.object();
    if (jsonObject.size() == 0) {
        err = "query failed, receive data is empty";
        qCritical().noquote() << jsonString;
        return false;
    }

    if (!jsonObject.contains("code") || !jsonObject.contains("data")) {
        err = "query failed, receive data format err";
        qCritical().noquote() << jsonString;
        return false;
    }

    int code = jsonObject["code"].toInt();
    if (code != 200) {
        err = "app not found in repo";
        qCritical().noquote() << jsonString;
        return false;
    }

    jsonValue = jsonObject.value(QStringLiteral("data"));
    // 转义服务端传的ｎull
    if (jsonValue.isNull()) {
        jsonValue = QJsonArray::fromStringList({});
        qWarning().noquote() << "recevied from server:" << jsonString;
    }
    if (!jsonValue.isArray()) {
        err = "query failed, jsonString from server data format is not array";
        qCritical().noquote() << jsonString;
        return false;
    }
    return true;
}

/*
 * 将json字符串转化为软件包数据
 *
 * @param jsonString: 软件包对应的json字符串
 * @param appList: 转换结果
 * @param err: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
bool PackageManagerPrivate::loadAppInfo(
        const QString &jsonString,
        QList<QSharedPointer<linglong::package::AppMetaInfo>> &appList,
        QString &err)
{
    QJsonValue arrayValue;
    auto ret = getAppJsonArray(jsonString, arrayValue, err);
    if (!ret) {
        qCritical().noquote() << jsonString;
        return false;
    }

    // 多个结果以json 数组形式返回
    QJsonArray arr = arrayValue.toArray();
    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject dataObj = arr.at(i).toObject();
        const QString appString = QString(QJsonDocument(dataObj).toJson(QJsonDocument::Compact));
        // qInfo().noquote() << appString;
        QSharedPointer<package::AppMetaInfo> appItem(
                util::loadJsonString<package::AppMetaInfo>(appString));
        appList.push_back(appItem);
    }
    return true;
}

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
bool PackageManagerPrivate::getAppInfofromServer(const QString &pkgName,
                                                 const QString &pkgVer,
                                                 const QString &pkgArch,
                                                 QString &appData,
                                                 QString &err)
{
    bool ret = HTTPCLIENT->queryRemoteApp(remoteRepoName, pkgName, pkgVer, pkgArch, appData);
    if (!ret) {
        err = "getAppInfofromServer err, " + appData + " ,please check the network";
        qCritical().noquote() << "receive from server:" << appData;
        return false;
    }

    return true;
}

/*
 * 将在线包数据部分签出到指定目录
 *
 * @param pkgName: 软件包包名
 * @param pkgVer: 软件包版本号
 * @param pkgArch: 软件包对应的架构
 * @param channel: 软件包对应的渠道
 * @param module: 软件包类型
 * @param dstPath: 在线包数据部分存储路径
 * @param err: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
bool PackageManagerPrivate::downloadAppData(const QString &pkgName,
                                            const QString &pkgVer,
                                            const QString &pkgArch,
                                            const QString &channel,
                                            const QString &module,
                                            const QString &dstPath,
                                            QString &err)
{
    bool ret = OSTREE_REPO_HELPER->ensureRepoEnv(kLocalRepoPath, err);
    if (!ret) {
        qCritical() << err;
        return false;
    }

    // new format --> linglong/org.deepin.downloader/5.3.69/x86_64/devel
    QString matchRef = QString("%1/%2/%3/%4/%5")
                               .arg(channel)
                               .arg(pkgName)
                               .arg(pkgVer)
                               .arg(pkgArch)
                               .arg(module);
    qInfo() << "downloadAppData ref:" << matchRef;

    // ret = repo.repoPull(repoPath, qrepoList[0], pkgName, err);
    ret = OSTREE_REPO_HELPER->repoPullbyCmd(kLocalRepoPath, remoteRepoName, matchRef, err);
    if (!ret) {
        qCritical() << err;
        return false;
    }
    // checkout 目录
    // const QString dstPath = repoPath + "/AppData";
    ret = OSTREE_REPO_HELPER->checkOutAppData(kLocalRepoPath,
                                              remoteRepoName,
                                              matchRef,
                                              dstPath,
                                              err);
    if (!ret) {
        qCritical() << err;
        return false;
    }
    qInfo() << "downloadAppData success, path:" << dstPath;

    return ret;
}

Reply PackageManagerPrivate::GetDownloadStatus(const ParamOption &paramOption, int type)
{
    Reply reply;
    QString appId = paramOption.appId.trimmed();
    QString version = paramOption.version.trimmed();
    QString arch = linglong::util::hostArch();
    QString channel = paramOption.channel.trimmed();
    QString appModule = paramOption.appModule.trimmed();
    QString latestVersion = version;
    if (version.isEmpty() || type == 1) {
        if (type == 1) {
            // 判断是否已安装 bug 146229
            if (!linglong::util::getAppInstalledStatus(appId, "", arch, channel, appModule, "")) {
                reply.message = appId + ", version:" + version + ", arch:" + arch
                        + ", channel:" + channel + ", module:" + appModule + " not installed";
                reply.code = STATUS_CODE(kPkgNotInstalled);
                appState.insert(appId + "/" + version + "/" + arch, reply);
                return reply;
            }
        }
        QString appData = "";
        // 安装不查缓存
        auto ret = getAppInfofromServer(appId, "", arch, appData, reply.message);
        if (!ret) {
            reply.code = STATUS_CODE(kPkgInstallFailed);
            return reply;
        }

        QList<QSharedPointer<linglong::package::AppMetaInfo>> appList;
        ret = loadAppInfo(appData, appList, reply.message);
        if (!ret || appList.size() < 1) {
            reply.message =
                    "app:" + appId + ", version:" + paramOption.version + " not found in repo";
            qCritical() << reply.message;
            reply.code = STATUS_CODE(kPkgInstallFailed);
            return reply;
        }

        // 查找最高版本
        QSharedPointer<linglong::package::AppMetaInfo> appInfo = getLatestApp(appId, appList);
        latestVersion = appInfo->version;
    }

    if (type > 0) {
        reply.message = appId + " is updating...";
    } else {
        reply.message = appId + " is installing...";
    }

    // 获取到完成状态则返回
    QString key = appId + "/" + version + "/" + arch;
    int dstState = type > 0 ? STATUS_CODE(kErrorPkgUpdateSuccess) : STATUS_CODE(kPkgInstallSuccess);
    if (appState.contains(key) && appState[key].code == dstState) {
        return appState[key];
    } else {
        // Fix to do get more specific param 首次安装应用的时候 安装runtime 提示不准
        QString fileName = QStringList{ channel, appId, latestVersion, arch, appModule }.join("-");
        QString filePath = "/tmp/.linglong/" + fileName;
        QFile progressFile(filePath);
        if (progressFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QStringList ret = QString(progressFile.readAll())
                                      .split(QRegExp("[\r\n]"),
#ifdef QT_DEPRECATED_VERSION_5_15
                                             Qt::SkipEmptyParts
#else
                                             QString::SkipEmptyParts
#endif
                                      );
            if (ret.size() > 1) {
                QStringList processList = ret.at(1).trimmed().split("\u001B8");
                reply.message = processList.at(processList.size() - 1).trimmed();
            }
            qInfo() << reply.message;
            progressFile.close();
        }
        if (type > 0) {
            reply.code = STATUS_CODE(kPkgUpdating);
        } else {
            reply.code = STATUS_CODE(kPkgInstalling);
        }
    }
    // 下载过程中有异常直接返回
    if (appState.contains(key)) {
        // 对于更新操作，下载完成了需要等待卸载完成
        if (type == 1) {
            if (appState[key].code == STATUS_CODE(kPkgInstallSuccess)) {
                appState[key].code = STATUS_CODE(kPkgUpdating);
            }
        }
        return appState[key];
    }
    return reply;
}

/*
 * 安装应用runtime
 *
 * @param appInfo: runtime对象
 * @param err: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
bool PackageManagerPrivate::installRuntime(QSharedPointer<linglong::package::AppMetaInfo> appInfo,
                                           QString &err)
{
    QString savePath =
            kAppInstallPath + appInfo->appId + "/" + appInfo->version + "/" + appInfo->arch;
    if ("devel" == appInfo->module) {
        savePath.append("/" + appInfo->module);
    }
    bool ret = downloadAppData(appInfo->appId,
                               appInfo->version,
                               appInfo->arch,
                               appInfo->channel,
                               appInfo->module,
                               savePath,
                               err);
    if (!ret) {
        err = "installRuntime download runtime data err";
        return false;
    }

    // 更新本地数据库文件
    QString userName = linglong::util::getUserName();
    if (noDBusMode) {
        userName = "deepin-linglong";
    }
    appInfo->kind = "runtime";
    linglong::util::insertAppRecord(appInfo, userName);

    return true;
}

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
bool PackageManagerPrivate::checkAppRuntime(const QString &runtime,
                                            const QString &channel,
                                            const QString &module,
                                            QString &err)
{
    // runtime ref in repo org.deepin.Runtime/20/x86_64
    QStringList runtimeInfo = runtime.split("/");
    if (runtimeInfo.size() < 3) {
        err = "app runtime:" + runtime + " runtime format err";
        return false;
    }
    const QString runtimeId = runtimeInfo.at(0);
    const QString runtimeVer = runtimeInfo.at(1);
    const QString runtimeArch = runtimeInfo.at(2);

    // runtimeId 校验
    if (runtimeId.isEmpty()) {
        err = "app runtime:" + runtime + " runtimeId format err";
        return false;
    }

    QStringList runtimeVersion = runtimeVer.split(".");
    // runtime更新只匹配前面三位，info.json中的runtime version格式必须是3位或4位点分十进制
    if (runtimeVersion.size() < 3) {
        err = "app runtime:" + runtime + " runtime version format err";
        return false;
    }

    QString version = "";
    if (runtimeVersion.size() == 4) {
        version = runtimeVer;
    }
    QString appData = "";
    bool ret = getAppInfofromServer(runtimeId, version, runtimeArch, appData, err);
    if (!ret) {
        return false;
    }
    QList<QSharedPointer<linglong::package::AppMetaInfo>> appList;
    ret = loadAppInfo(appData, appList, err);
    if (!ret || appList.size() < 1) {
        err = runtime + " not found in repo";
        qCritical() << err;
        return false;
    }
    // 查找最高版本，多版本场景安装应用appId要求完全匹配
    QSharedPointer<linglong::package::AppMetaInfo> appInfo =
            getLatestRuntime(runtimeId, runtimeVer, appList);
    // fix 当前服务端不支持按channel查询，返回的结果是默认channel，需要刷新channel/module
    appInfo->channel = channel;
    appInfo->module = module;
    // 判断app依赖的runtime是否安装 runtime 不区分用户
    if (!linglong::util::getAppInstalledStatus(appInfo->appId,
                                               appInfo->version,
                                               appInfo->arch,
                                               channel,
                                               module,
                                               "")) {
        ret = installRuntime(appInfo, err);
    }
    return ret;
}

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
bool PackageManagerPrivate::checkAppBase(const QString &runtime,
                                         const QString &channel,
                                         const QString &module,
                                         QString &err)
{
    // 通过runtime获取base ref
    QStringList runtimeList = runtime.split("/");
    if (runtimeList.size() < 3) {
        err = "app runtime:" + runtime + " runtime format err";
        return false;
    }
    const QString runtimeId = runtimeList.at(0);
    const QString runtimeVer = runtimeList.at(1);
    const QString runtimeArch = runtimeList.at(2);

    // runtimeId 校验
    if (runtimeId.isEmpty()) {
        err = "app runtime:" + runtime + " runtimeId format err";
        return false;
    }

    QList<QSharedPointer<linglong::package::AppMetaInfo>> appList;
    QString appData = "";

    bool ret = getAppInfofromServer(runtimeId, runtimeVer, runtimeArch, appData, err);
    if (!ret) {
        return false;
    }
    ret = loadAppInfo(appData, appList, err);
    if (!ret || appList.size() < 1) {
        err = runtime + " not found in repo";
        qCritical() << err;
        return false;
    }

    QSharedPointer<linglong::package::AppMetaInfo> latestRuntimeInfo =
            getLatestRuntime(runtimeId, runtimeVer, appList);

    auto baseRef = latestRuntimeInfo->runtime;
    QStringList baseList = baseRef.split('/');
    if (baseList.size() < 3) {
        err = "app base:" + baseRef + " base format err";
        return false;
    }
    const QString baseId = baseList.at(0);
    const QString baseVer = baseList.at(1);
    const QString baseArch = baseList.at(2);

    bool retbase = true;

    QList<QSharedPointer<linglong::package::AppMetaInfo>> baseRuntimeList;
    QString baseData = "";

    ret = getAppInfofromServer(baseId, baseVer, baseArch, baseData, err);
    if (!ret) {
        return false;
    }
    ret = loadAppInfo(baseData, baseRuntimeList, err);
    if (!ret || appList.size() < 1) {
        err = baseRef + " not found in repo";
        qCritical() << err;
        return false;
    }
    // fix to do base runtime debug info, base runtime update
    auto baseInfo = baseRuntimeList.at(0);
    baseInfo->channel = channel;
    baseInfo->module = module;
    // 判断app依赖的runtime是否安装 runtime 不区分用户
    if (!linglong::util::getAppInstalledStatus(baseId, baseVer, baseArch, channel, module, "")) {
        retbase = installRuntime(baseInfo, err);
    }
    return retbase;
}

/*
 * 从给定的软件包列表中查找最新版本的runtime
 *
 * @param appId: 待匹配runtime的appId
 * @param appList: 待搜索的软件包列表信息
 *
 * @return AppMetaInfo: 最新版本的runtime
 *
 */
QSharedPointer<linglong::package::AppMetaInfo> PackageManagerPrivate::getLatestRuntime(
        const QString &appId,
        const QString &version,
        const QList<QSharedPointer<linglong::package::AppMetaInfo>> &appList)
{
    QSharedPointer<linglong::package::AppMetaInfo> latestApp = appList.at(0);
    if (appList.size() == 1) {
        return latestApp;
    }

    QString curVersion = linglong::util::APP_MIN_VERSION;
    for (auto item : appList) {
        linglong::util::AppVersion dstVersion(curVersion);
        linglong::util::AppVersion iterVersion(item->version);
        if (appId == item->appId && iterVersion.isBigThan(dstVersion)
            && item->version.startsWith(version)) {
            curVersion = item->version;
            latestApp = item;
        }
    }
    return latestApp;
}

/*
 * 从给定的软件包列表中查找最新版本的软件包
 *
 * @param appId: 待匹配应用的appId
 * @param appList: 待搜索的软件包列表信息
 *
 * @return AppMetaInfo: 最新版本的软件包
 *
 */
QSharedPointer<linglong::package::AppMetaInfo> PackageManagerPrivate::getLatestApp(
        const QString &appId, const QList<QSharedPointer<linglong::package::AppMetaInfo>> &appList)
{
    QSharedPointer<linglong::package::AppMetaInfo> latestApp = appList.at(0);
    if (appList.size() == 1) {
        return latestApp;
    }

    QString curVersion = linglong::util::APP_MIN_VERSION;
    for (auto item : appList) {
        linglong::util::AppVersion dstVersion(curVersion);
        linglong::util::AppVersion iterVersion(item->version);
        if (appId == item->appId && iterVersion.isBigThan(dstVersion)) {
            curVersion = item->version;
            latestApp = item;
        }
    }
    return latestApp;
}

/*
 * 安装应用时更新包括desktop文件在内的配置文件
 *
 * @param appId: 应用的appId
 * @param version: 应用的版本号
 * @param arch: 应用对应的架构
 */
void PackageManagerPrivate::addAppConfig(const QString &appId,
                                         const QString &version,
                                         const QString &arch)
{
    // 是否为多版本
    if (linglong::util::getAppInstalledStatus(appId, "", arch, "", "", "")) {
        QList<QSharedPointer<linglong::package::AppMetaInfo>> pkgList;
        // 查找当前已安装软件包的最高版本
        linglong::util::getInstalledAppInfo(appId, "", arch, "", "", "", pkgList);
        auto it = pkgList.at(0);
        linglong::util::AppVersion dstVersion(version);
        linglong::util::AppVersion curVersion(it->version);
        if (curVersion.isBigThan(dstVersion)) {
            return;
        }
        // 目标版本较已有版本高，先删除旧的desktop
        const QString installPath = kAppInstallPath + it->appId + "/" + it->version;
        // 删掉安装配置链接文件
        if (linglong::util::dirExists(installPath + "/" + arch + "/outputs/share")) {
            const QString appEntriesDirPath = installPath + "/" + arch + "/outputs/share";
            linglong::util::removeDstDirLinkFiles(appEntriesDirPath, sysLinglongInstalltions);
        } else {
            const QString appEntriesDirPath = installPath + "/" + arch + "/entries";
            linglong::util::removeDstDirLinkFiles(appEntriesDirPath, sysLinglongInstalltions);
        }
    }

    const QString savePath = kAppInstallPath + appId + "/" + version + "/" + arch;
    // 链接应用配置文件到系统配置目录
    if (linglong::util::dirExists(savePath + "/outputs/share")) {
        const QString appEntriesDirPath = savePath + "/outputs/share";
        linglong::util::linkDirFiles(appEntriesDirPath, sysLinglongInstalltions);
    } else {
        const QString appEntriesDirPath = savePath + "/entries";
        linglong::util::linkDirFiles(appEntriesDirPath, sysLinglongInstalltions);
    }
}

/*
 * 卸载应用时更新包括desktop文件在内的配置文件
 *
 * @param appId: 应用的appId
 * @param version: 应用的版本号
 * @param arch: 应用对应的架构
 */
void PackageManagerPrivate::delAppConfig(const QString &appId,
                                         const QString &version,
                                         const QString &arch)
{
    // 是否为多版本
    if (linglong::util::getAppInstalledStatus(appId, "", arch, "", "", "")) {
        QList<QSharedPointer<linglong::package::AppMetaInfo>> pkgList;
        // 查找当前已安装软件包的最高版本
        linglong::util::getInstalledAppInfo(appId, "", arch, "", "", "", pkgList);
        auto it = pkgList.at(0);
        linglong::util::AppVersion dstVersion(version);
        linglong::util::AppVersion curVersion(it->version);
        if (curVersion.isBigThan(dstVersion)) {
            return;
        }

        // 目标版本较已有版本高，先删除旧的desktop
        const QString installPath = kAppInstallPath + appId + "/" + version;
        // 删掉安装配置链接文件
        if (linglong::util::dirExists(installPath + "/" + arch + "/outputs/share")) {
            const QString appEntriesDirPath = installPath + "/" + arch + "/outputs/share";
            linglong::util::removeDstDirLinkFiles(appEntriesDirPath, sysLinglongInstalltions);
        } else {
            const QString appEntriesDirPath = installPath + "/" + arch + "/entries";
            linglong::util::removeDstDirLinkFiles(appEntriesDirPath, sysLinglongInstalltions);
        }

        const QString savePath = kAppInstallPath + appId + "/" + it->version + "/" + arch;
        // 链接应用配置文件到系统配置目录
        if (linglong::util::dirExists(savePath + "/outputs/share")) {
            const QString appEntriesDirPath = savePath + "/outputs/share";
            linglong::util::linkDirFiles(appEntriesDirPath, sysLinglongInstalltions);
        } else {
            const QString appEntriesDirPath = savePath + "/entries";
            linglong::util::linkDirFiles(appEntriesDirPath, sysLinglongInstalltions);
        }
        return;
    }
    // 目标版本较已有版本高，先删除旧的desktop
    const QString installPath = kAppInstallPath + appId + "/" + version;
    // 删掉安装配置链接文件
    if (linglong::util::dirExists(installPath + "/" + arch + "/outputs/share")) {
        const QString appEntriesDirPath = installPath + "/" + arch + "/outputs/share";
        linglong::util::removeDstDirLinkFiles(appEntriesDirPath, sysLinglongInstalltions);
    } else {
        const QString appEntriesDirPath = installPath + "/" + arch + "/entries";
        linglong::util::removeDstDirLinkFiles(appEntriesDirPath, sysLinglongInstalltions);
    }
}

/*!
 * 在线安装软件包
 * @param installParamOption
 */
Reply PackageManagerPrivate::Install(const InstallParamOption &installParamOption)
{
    Reply reply;
    QString userName = linglong::util::getUserName();
    if (noDBusMode) {
        userName = "deepin-linglong";
    }

    QString appId = installParamOption.appId.trimmed();
    QString arch = installParamOption.arch.trimmed().toLower();
    QString version = installParamOption.version.trimmed();
    QString channel = installParamOption.channel.trimmed();
    QString appModule = installParamOption.appModule.trimmed();

    if (arch.isEmpty()) {
        arch = linglong::util::hostArch();
    }

    if (channel.isEmpty()) {
        channel = "linglong";
    }
    if (appModule.isEmpty()) {
        appModule = "runtime";
    }

    package::Ref ref("", channel, appId, version, arch, appModule);

    // 异常后重新安装需要清除上次状态
    appState.remove(appId + "/" + version + "/" + arch);

    if (arch != linglong::util::hostArch()) {
        reply.message = "app arch:" + arch + " not support in host";
        reply.code = STATUS_CODE(kUserInputParamErr);
        appState.insert(appId + "/" + version + "/" + arch, reply);
        return reply;
    }

    QString appData = "";
    // 安装不查缓存
    auto ret = getAppInfofromServer(appId, version, arch, appData, reply.message);
    if (!ret) {
        reply.code = STATUS_CODE(kPkgInstallFailed);
        appState.insert(appId + "/" + version + "/" + arch, reply);
        return reply;
    }

    QList<QSharedPointer<linglong::package::AppMetaInfo>> appList;
    ret = loadAppInfo(appData, appList, reply.message);
    if (!ret || appList.size() < 1) {
        reply.message = "app:" + appId + ", version:" + version + " not found in repo";
        qCritical() << reply.message;
        reply.code = STATUS_CODE(kPkgInstallFailed);
        appState.insert(appId + "/" + version + "/" + arch, reply);
        return reply;
    }

    // 查找最高版本，多版本场景安装应用appId要求完全匹配
    QSharedPointer<linglong::package::AppMetaInfo> appInfo = getLatestApp(appId, appList);
    // 不支持模糊安装
    if (appId != appInfo->appId) {
        reply.message = "app:" + appId + ", version:" + version + " not found in repo";
        qCritical() << "found latest app:" << appInfo->appId << ", " << reply.message;
        reply.code = STATUS_CODE(kPkgInstallFailed);
        appState.insert(appId + "/" + version + "/" + arch, reply);
        return reply;
    }

    // 判断对应版本的应用是否已安装
    if (linglong::util::getAppInstalledStatus(appInfo->appId,
                                              appInfo->version,
                                              "",
                                              channel,
                                              appModule,
                                              "")) {
        reply.code = STATUS_CODE(kPkgAlreadyInstalled);
        reply.message = appInfo->appId + ", version: " + appInfo->version + " already installed";
        qCritical() << reply.message;
        appState.insert(appId + "/" + version + "/" + arch, reply);
        return reply;
    }

    // 检查软件包依赖的runtime安装状态
    ret = checkAppRuntime(appInfo->runtime, channel, appModule, reply.message);
    if (!ret) {
        qCritical() << reply.message;
        reply.code = STATUS_CODE(kInstallRuntimeFailed);
        appState.insert(appId + "/" + version + "/" + arch, reply);
        return reply;
    }

    // 检查软件包依赖的base安装状态
    if (!linglong::util::isDeepinSysProduct()) {
        ret = checkAppBase(appInfo->runtime, channel, appModule, reply.message);
        if (!ret) {
            qCritical() << reply.message;
            reply.code = STATUS_CODE(kInstallBaseFailed);
            appState.insert(appId + "/" + version + "/" + arch, reply);
            return reply;
        }
    }

    // 下载在线包数据到目标目录
    QString savePath =
            kAppInstallPath + appInfo->appId + "/" + appInfo->version + "/" + appInfo->arch;
    if ("devel" == appModule) {
        savePath.append("/" + appModule);
    }
    ret = downloadAppData(appInfo->appId,
                          appInfo->version,
                          appInfo->arch,
                          channel,
                          appModule,
                          savePath,
                          reply.message);
    if (!ret) {
        qCritical() << "downloadAppData app:" << appInfo->appId << ", version:" << appInfo->version
                    << " error";
        reply.code = STATUS_CODE(kLoadPkgDataFailed);
        appState.insert(appId + "/" + version + "/" + arch, reply);
        return reply;
    }

    // 链接应用配置文件到系统配置目录
    addAppConfig(appInfo->appId, appInfo->version, appInfo->arch);

    // 更新desktop database
    auto retRunner = linglong::runner::Runner("update-desktop-database",
                                              { sysLinglongInstalltions + "/applications/" },
                                              1000 * 60 * 1);
    if (!retRunner) {
        qWarning() << "warning: update desktop database of " + sysLinglongInstalltions
                        + "/applications/ failed!";
    }

    // 更新mime type database
    if (linglong::util::dirExists(sysLinglongInstalltions + "/mime/packages")) {
        auto retUpdateMime = linglong::runner::Runner("update-mime-database",
                                                      { sysLinglongInstalltions + "/mime/" },
                                                      1000 * 60 * 1);
        if (!retUpdateMime) {
            qWarning() << "warning: update mime type database of " + sysLinglongInstalltions
                            + "/mime/ failed!";
        }
    }

    // 更新 glib-2.0/schemas
    if (linglong::util::dirExists(sysLinglongInstalltions + "/glib-2.0/schemas")) {
        auto retUpdateSchemas =
                linglong::runner::Runner("glib-compile-schemas",
                                         { sysLinglongInstalltions + "/glib-2.0/schemas" },
                                         1000 * 60 * 1);
        if (!retUpdateSchemas) {
            qWarning() << "warning: update schemas of " + sysLinglongInstalltions
                            + "/glib-2.0/schemas failed!";
        }
    }

    // 更新本地数据库文件
    appInfo->kind = "app";
    // fix 当前服务端不支持按channel查询，返回的结果是默认channel，需要刷新channel/module
    appInfo->channel = channel;
    appInfo->module = appModule;

    linglong::util::insertAppRecord(appInfo, userName);

    // process portal after install
    {
        auto installPath = savePath;
        qDebug() << "call systemHelperInterface.RebuildInstallPortal" << installPath,
                ref.toLocalFullRef();
        QDBusReply<void> helperRet =
                systemHelperInterface.RebuildInstallPortal(installPath, ref.toString(), {});
        if (!helperRet.isValid()) {
            qWarning() << "process post install portal failed:" << helperRet.error();
        }
    }

    reply.code = STATUS_CODE(kPkgInstallSuccess);
    reply.message = "install " + appInfo->appId + ", version:" + appInfo->version + " success";
    qInfo() << reply.message;
    appState.insert(appId + "/" + version + "/" + arch, reply);
    return reply;
}

/*
 * 查询软件包
 *
 * @param paramOption 查询命令参数
 *
 * @return QueryReply dbus方法调用应答
 */
QueryReply PackageManagerPrivate::Query(const QueryParamOption &paramOption)
{
    QueryReply reply;
    QString appId = paramOption.appId.trimmed();
    bool ret = false;
    if ("installed" == appId) {
        ret = linglong::util::queryAllInstalledApp("", reply.result, reply.message);
        if (ret) {
            reply.code = STATUS_CODE(kErrorPkgQuerySuccess);
            reply.message = "query " + appId + " success";
        } else {
            reply.code = STATUS_CODE(kErrorPkgQueryFailed);
        }
        return reply;
    }

    QList<QSharedPointer<linglong::package::AppMetaInfo>> pkgList;
    QString arch = linglong::util::hostArch();

    QString appData = "";
    int status = STATUS_CODE(kFail);
    if (!paramOption.force) {
        status = linglong::util::queryLocalCache(appId, appData);
    }

    bool fromServer = false;
    // 缓存查不到从服务器查
    if (status != STATUS_CODE(kSuccess)) {
        ret = getAppInfofromServer(appId, "", arch, appData, reply.message);
        if (!ret) {
            reply.code = STATUS_CODE(kErrorPkgQueryFailed);
            qCritical() << reply.message;
            return reply;
        }
        fromServer = true;
    }

    QJsonValue jsonValue;
    ret = getAppJsonArray(appData, jsonValue, reply.message);
    if (!ret) {
        reply.code = STATUS_CODE(kErrorPkgQueryFailed);
        qCritical() << reply.message;
        return reply;
    }
    // 若从服务器查询得到正确的数据则更新缓存
    if (fromServer) {
        linglong::util::updateCache(appId, appData);
    }

    QJsonDocument document = QJsonDocument(jsonValue.toArray());
    reply.code = STATUS_CODE(kErrorPkgQuerySuccess);
    reply.message = "query " + appId + " success";
    reply.result = QString(document.toJson());
    return reply;
}

Reply PackageManagerPrivate::Uninstall(const UninstallParamOption &paramOption)
{
    Q_Q(PackageManager);
    Reply reply;

    QString appId = paramOption.appId.trimmed();
    QString version = paramOption.version.trimmed();
    QString arch = paramOption.arch.trimmed().toLower();

    if (arch.isEmpty()) {
        arch = linglong::util::hostArch();
    }

    if (!version.isEmpty() && paramOption.delAllVersion) {
        reply.message =
                "uninstall " + appId + "/" + version + " is in conflict with all-version param";
        reply.code = STATUS_CODE(kUserInputParamErr);
        qCritical() << reply.message;
        return reply;
    }

    QString channel = paramOption.channel.trimmed();
    QString appModule = paramOption.appModule.trimmed();
    if (channel.isEmpty()) {
        channel = "linglong";
    }
    if (appModule.isEmpty()) {
        appModule = "runtime";
    }

    package::Ref ref("", channel, appId, version, arch, appModule);

    // 判断是否已安装 不校验用户名
    QString userName = linglong::util::getUserName();
    if (!linglong::util::getAppInstalledStatus(appId, version, arch, channel, appModule, "")) {
        reply.message = appId + ", version:" + version + ", arch:" + arch + ", channel:" + channel
                + ", module:" + appModule + " not installed";
        reply.code = STATUS_CODE(kPkgNotInstalled);
        qCritical() << reply.message;
        return reply;
    }

    QList<QSharedPointer<linglong::package::AppMetaInfo>> pkgList;
    if (paramOption.delAllVersion) {
        linglong::util::getAllVerAppInfo(appId, "", arch, "", pkgList);
    } else {
        // 根据已安装文件查询已安装软件包信息
        linglong::util::getInstalledAppInfo(appId, version, arch, channel, appModule, "", pkgList);
    }

    QStringList delVersionList;
    for (auto it : pkgList) {
        bool isRoot = (getgid() == 0) ? true : false;
        qInfo() << "install app user:" << it->user << ", current user:" << userName
                << ", has root permission:" << isRoot;

        // 非root用户卸载不属于该用户安装的应用
        if (userName != it->user && !isRoot) {
            reply.code = STATUS_CODE(kPkgUninstallFailed);
            reply.message = appId + " uninstall permission deny";
            qCritical() << reply.message;
            return reply;
        }

        if (paramOption.delAllVersion && (it->channel != channel || it->module != appModule)) {
            continue;
        }

        QString err = "";
        // 更新本地repo仓库
        bool ret = OSTREE_REPO_HELPER->ensureRepoEnv(kLocalRepoPath, err);
        if (!ret) {
            qCritical() << err;
            reply.code = STATUS_CODE(kPkgUninstallFailed);
            reply.message = "uninstall local repo not exist";
            return reply;
        }
        // 应从安装数据库获取应用所属仓库信息 to do fix
        QVector<QString> qrepoList;
        ret = OSTREE_REPO_HELPER->getRemoteRepoList(kLocalRepoPath, qrepoList, err);
        if (!ret) {
            qCritical() << err;
            reply.code = STATUS_CODE(kPkgUninstallFailed);
            reply.message = "uninstall remote repo not exist";
            return reply;
        }

        // new ref format --> channel/org.deepin.calculator/1.2.2/x86_64/module
        QString matchRef = QString("%1/%2/%3/%4/%5")
                                   .arg(channel)
                                   .arg(it->appId)
                                   .arg(it->version)
                                   .arg(arch)
                                   .arg(appModule);

        qInfo() << "Uninstall app ref:" << matchRef;
        ret = OSTREE_REPO_HELPER->repoDeleteDatabyRef(kLocalRepoPath, qrepoList[0], matchRef, err);
        if (!ret) {
            qCritical() << err;
            reply.code = STATUS_CODE(kPkgUninstallFailed);
            reply.message = "uninstall " + appId + ", version:" + it->version + " failed";
            return reply;
        }

        // A 用户 sudo 卸载 B 用户安装的软件
        if (isRoot) {
            userName = "";
        }
        // 更新安装数据库
        linglong::util::deleteAppRecord(appId, it->version, arch, channel, appModule, userName);

        delAppConfig(appId, it->version, arch);
        // 更新desktop database
        auto retRunner = linglong::runner::Runner("update-desktop-database",
                                                  { sysLinglongInstalltions + "/applications/" },
                                                  1000 * 60 * 1);
        if (!retRunner) {
            qWarning() << "warning: update desktop database of " + sysLinglongInstalltions
                            + "/applications/ failed!";
        }

        // 更新mime type database
        if (linglong::util::dirExists(sysLinglongInstalltions + "/mime/packages")) {
            auto retUpdateMime = linglong::runner::Runner("update-mime-database",
                                                          { sysLinglongInstalltions + "/mime/" },
                                                          1000 * 60 * 1);
            if (!retUpdateMime) {
                qWarning() << "warning: update mime type database of " + sysLinglongInstalltions
                                + "/mime/ failed!";
            }
        }

        // 更新 glib-2.0/schemas
        if (linglong::util::dirExists(sysLinglongInstalltions + "/glib-2.0/schemas")) {
            auto retUpdateSchemas =
                    linglong::runner::Runner("glib-compile-schemas",
                                             { sysLinglongInstalltions + "/glib-2.0/schemas" },
                                             1000 * 60 * 1);
            if (!retUpdateSchemas) {
                qWarning() << "warning: update schemas of " + sysLinglongInstalltions
                                + "/glib-2.0/schemas failed!";
            }
        }

        // 删除应用对应的安装目录
        const QString installPath = kAppInstallPath + it->appId + "/" + it->version;

        // process portal before uninstall
        {
            QVariantMap variantMap;
            if (paramOption.delAppData) {
                if (!noDBusMode) {
                    QDBusReply<uint> dbusReply =
                            q->connection().interface()->serviceUid(q->message().service());
                    QString caller = "";
                    if (dbusReply.isValid()) {
                        caller = getUserName(dbusReply.value());
                    }
                    qDebug() << "Uninstall app call user:" << caller << dbusReply.error();
                    if (!caller.isEmpty()) {
                        QString appDataPath =
                                QString("/home/%1/.linglong/%2").arg(caller).arg(it->appId);
                        variantMap.insert(linglong::util::kKeyDelData, appDataPath);
                    }
                }
            }
            auto packageRootPath = installPath + "/" + arch;
            qDebug() << "call systemHelperInterface.RuinInstallPortal" << packageRootPath
                     << ref.toLocalFullRef() << paramOption.delAppData;
            QDBusReply<void> helperRet = systemHelperInterface.RuinInstallPortal(packageRootPath,
                                                                                 ref.toString(),
                                                                                 variantMap);
            if (!helperRet.isValid()) {
                qWarning() << "process pre uninstall portal failed:" << helperRet.error();
            }
        }

        // 删除release版本时需要判断是否存在debug版本
        if ("devel" != appModule) {
            if (linglong::util::getAppInstalledStatus(appId,
                                                      it->version,
                                                      arch,
                                                      channel,
                                                      "devel",
                                                      "")) {
                // 删除安装目录下除devel外的所有文件
                QDir dir(installPath + "/" + arch);
                dir.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
                QFileInfoList fileList = dir.entryInfoList();
                for (const auto &file : fileList) {
                    if (file.isFile()) {
                        file.dir().remove(file.fileName());
                    } else {
                        if ("devel" != file.fileName()) {
                            linglong::util::removeDir(file.absoluteFilePath());
                        }
                    }
                }
            } else {
                linglong::util::removeDir(installPath);
            }
        } else {
            // 卸载debug版本
            linglong::util::removeDir(installPath + "/" + arch + "/devel");
        }

        QDir archDir(installPath + "/" + arch);
        archDir.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        if (archDir.entryInfoList().size() <= 0) {
            linglong::util::removeDir(installPath);
        }

        QDir dir(kAppInstallPath + it->appId);
        dir.setFilter(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        if (dir.entryInfoList().size() <= 0) {
            linglong::util::removeDir(kAppInstallPath + it->appId);
        }
        qInfo() << "Uninstall del dir:" << installPath;
        reply.message = "uninstall " + appId + ", version:" + it->version + " success";
        delVersionList.append(it->version);
    }
    reply.code = STATUS_CODE(kPkgUninstallSuccess);
    if (paramOption.delAllVersion && pkgList.size() > 1) {
        reply.message = "uninstall " + appId + " " + delVersionList.join(",") + " success";
    }
    return reply;
}

Reply PackageManagerPrivate::Update(const ParamOption &paramOption)
{
    Reply reply;

    QString appId = paramOption.appId.trimmed();
    QString arch = paramOption.arch.trimmed().toLower();
    QString version = paramOption.version.trimmed();
    if (arch.isEmpty()) {
        arch = linglong::util::hostArch();
    }

    QString channel = paramOption.channel.trimmed();
    QString appModule = paramOption.appModule.trimmed();
    if (channel.isEmpty()) {
        channel = "linglong";
    }
    if (appModule.isEmpty()) {
        appModule = "runtime";
    }

    // 异常后重新安装需要清除上次状态
    appState.remove(appId + "/" + version + "/" + arch);

    // 判断是否已安装
    if (!linglong::util::getAppInstalledStatus(appId, version, arch, channel, appModule, "")) {
        reply.message = appId + ", version:" + version + ", arch:" + arch + ", channel:" + channel
                + ", module:" + appModule + " not installed";
        qCritical() << reply.message;
        reply.code = STATUS_CODE(kPkgNotInstalled);

        appState.insert(appId + "/" + version + "/" + arch, reply);
        return reply;
    }

    // 检查是否存在版本更新
    QList<QSharedPointer<linglong::package::AppMetaInfo>> pkgList;
    // 根据已安装文件查询已经安装软件包信息
    linglong::util::getInstalledAppInfo(appId, version, arch, channel, appModule, "", pkgList);
    if (pkgList.size() != 1) {
        reply.message = "query local app:" + appId + " info err";
        qCritical() << reply.message;
        reply.code = STATUS_CODE(kErrorPkgUpdateFailed);

        appState.insert(appId + "/" + version + "/" + arch, reply);
        return reply;
    }

    auto installedApp = pkgList.at(0);
    QString currentVersion = installedApp->version;
    QString appData = QString();
    auto ret = getAppInfofromServer(appId, "", arch, appData, reply.message);
    if (!ret) {
        reply.message = "query server app:" + appId + " info err";
        qCritical() << reply.message;
        reply.code = STATUS_CODE(kErrorPkgUpdateFailed);

        appState.insert(appId + "/" + version + "/" + arch, reply);
        return reply;
    }

    QList<QSharedPointer<linglong::package::AppMetaInfo>> serverPkgList;
    ret = loadAppInfo(appData, serverPkgList, reply.message);
    if (!ret || serverPkgList.size() < 1) {
        reply.message = "load app:" + appId + " info err";
        qCritical() << reply.message;
        reply.code = STATUS_CODE(kErrorPkgUpdateFailed);

        appState.insert(appId + "/" + version + "/" + arch, reply);
        return reply;
    }

    auto serverApp = getLatestApp(appId, serverPkgList);

    if (linglong::util::compareVersion(currentVersion, serverApp->version) >= 0) {
        reply.message =
                "app:" + appId + ", latest version:" + currentVersion + " already installed";
        qCritical() << reply.message;
        // bug 149881
        reply.code = STATUS_CODE(kErrorPkgUpdateSuccess);
        appState.insert(appId + "/" + version + "/" + arch, reply);
        return reply;
    }

    InstallParamOption installParamOption;
    installParamOption.appId = appId;
    installParamOption.version = serverApp->version;
    installParamOption.arch = arch;
    installParamOption.channel = channel;
    installParamOption.appModule = appModule;
    reply = Install({ installParamOption });
    if (reply.code != STATUS_CODE(kPkgInstallSuccess)) {
        reply.message =
                "download app:" + appId + ", version:" + installParamOption.version + " err";
        qCritical() << reply.message;
        reply.code = STATUS_CODE(kErrorPkgUpdateFailed);

        appState.insert(appId + "/" + version + "/" + arch, reply);
        return reply;
    }

    UninstallParamOption uninstallParamOption;
    uninstallParamOption.appId = appId;
    uninstallParamOption.version = currentVersion;
    uninstallParamOption.channel = channel;
    uninstallParamOption.appModule = appModule;
    reply = Uninstall(uninstallParamOption);
    if (reply.code != STATUS_CODE(kPkgUninstallSuccess)) {
        reply.message = "uninstall app:" + appId + ", version:" + currentVersion + " err";
        qCritical() << reply.message;
        reply.code = STATUS_CODE(kErrorPkgUpdateFailed);

        appState.insert(appId + "/" + version + "/" + arch, reply);
        return reply;
    }

    reply.code = STATUS_CODE(kErrorPkgUpdateSuccess);
    reply.message = "update " + appId + " success, version:" + currentVersion + " --> "
            + serverApp->version;
    appState.insert(appId + "/" + version + "/" + arch, reply);
    return reply;
}

QString PackageManagerPrivate::getUserName(uid_t uid)
{
    struct passwd *user;
    user = getpwuid(uid);
    if (user) {
        return QString::fromLocal8Bit(user->pw_name);
    }
    return QString();
}

PackageManager::PackageManager()
    : pool(new QThreadPool)
    , dd_ptr(new PackageManagerPrivate(this))
{
    // 检查安装数据库信息
    linglong::util::checkInstalledAppDb();
    linglong::util::updateInstalledAppInfoDb();
    pool->setMaxThreadCount(POOL_MAX_THREAD);
    // 检查应用缓存信息
    linglong::util::checkAppCache();
}

PackageManager::~PackageManager() { }

Reply PackageManager::ModifyRepo(const QString &name, const QString &url)
{
    Reply reply;
    Q_D(PackageManager);

    bool ret = OSTREE_REPO_HELPER->ensureRepoEnv(d->kLocalRepoPath, reply.message);
    if (!ret) {
        reply.code = STATUS_CODE(kFail);
        return reply;
    }

    QUrl cfgUrl(url);
    if (name.trimmed().isEmpty()
        || (cfgUrl.scheme().toLower() != "http" && cfgUrl.scheme().toLower() != "https")
        || !cfgUrl.isValid()) {
        reply.message = "url format error";
        reply.code = STATUS_CODE(kUserInputParamErr);
        return reply;
    }
    auto ostreeCfg = d->kLocalRepoPath + "/repo/config";
    auto serverCfg = d->kLocalRepoPath + "/config.json";
    if (!linglong::util::fileExists(ostreeCfg)) {
        reply.message = ostreeCfg + " no exist";
        reply.code = STATUS_CODE(kErrorModifyRepoFailed);
        return reply;
    }

    QString dstUrl = "";
    if (url.endsWith("/")) {
        dstUrl = url + "repos/" + name;
    } else {
        dstUrl = url + "/repos/" + name;
    }

    // ostree 没有删除的命令 更换仓库需要先删除之前的老仓库配置,当前使用的仓库名需要从文件中读取
    QString currentRepoName = "repo";
    linglong::util::getLocalConfig("repoName", currentRepoName);
    QString sectionName = QString("remote \"%1\"").arg(currentRepoName);

    QSettings *repoCfg = new QSettings(ostreeCfg, QSettings::IniFormat);
    repoCfg->beginGroup(sectionName);
    QString oldUrl = repoCfg->value("url").toString();
    repoCfg->remove("");
    repoCfg->endGroup();
    delete repoCfg;
    qDebug() << "modify repo delete" << currentRepoName << oldUrl;

    // ostree config --repo=/persistent/linglong/repo set "remote \"repo\".url"
    // https://repo-dev.linglong.space/repo/ ostree
    // config文件中节名有""，QSettings会自动转义，不用QSettings直接修改ostree config文件
    auto keyUrl = QString("remote \"%1\".url").arg(name);
    ret = linglong::runner::Runner(
            "ostree",
            { "config", "--repo=" + d->kLocalRepoPath + "/repo", "set", keyUrl, dstUrl },
            1000 * 60 * 5);
    auto keyGpg = QString("remote \"%1\".gpg-verify").arg(name);
    ret |= linglong::runner::Runner(
            "ostree",
            { "config", "--repo=" + d->kLocalRepoPath + "/repo", "set", keyGpg, "false" },
            1000 * 60 * 5);
    if (!ret) {
        reply.message = "modify repo config failed";
        qWarning() << reply.message;
        reply.code = STATUS_CODE(kErrorModifyRepoFailed);
        return reply;
    }

    d->remoteRepoName = name;
    QJsonObject obj;
    obj["repoName"] = name;
    obj["appDbUrl"] = url;
    QJsonDocument doc(obj);
    QFile jsonFile(serverCfg);
    jsonFile.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream wirteStream(&jsonFile);
    wirteStream.setCodec("UTF-8");
    wirteStream << doc.toJson();
    jsonFile.close();
    qDebug() << QString("modify repo name:%1 url:%2 success").arg(name).arg(url);
    reply.code = STATUS_CODE(kErrorModifyRepoSuccess);
    reply.message = "modify repo url success";
    return reply;
}

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
Reply PackageManager::GetDownloadStatus(const ParamOption &paramOption, int type)
{
    Q_D(PackageManager);
    Reply reply;
    QString appId = paramOption.appId.trimmed();
    if (appId.isEmpty()) {
        reply.code = STATUS_CODE(kUserInputParamErr);
        reply.message = "package name err";
        qCritical() << reply.message;
        return reply;
    }

    QString arch = paramOption.arch.trimmed().toLower();
    if (!arch.isEmpty() && arch != linglong::util::hostArch()) {
        reply.message = "app arch:" + arch + " not support in host";
        reply.code = STATUS_CODE(kUserInputParamErr);
        return reply;
    }
    return d->GetDownloadStatus(paramOption, type);
}

Reply PackageManager::Install(const InstallParamOption &installParamOption)
{
    Q_D(PackageManager);

    Reply reply;
    QString appId = installParamOption.appId.trimmed();
    if (appId.isEmpty()) {
        reply.message = "appId input err";
        reply.code = STATUS_CODE(kUserInputParamErr);
        return reply;
    }

    QFuture<void> future = QtConcurrent::run(pool.data(), [=]() {
        d->Install(installParamOption);
    });
    reply.code = STATUS_CODE(kPkgInstalling);
    reply.message = installParamOption.appId + " is installing";
    return reply;
}

Reply PackageManager::Uninstall(const UninstallParamOption &paramOption)
{
    Q_D(PackageManager);
    Reply reply;
    QString appId = paramOption.appId.trimmed();

    if (appId.isEmpty()) {
        reply.message = "appId input err";
        reply.code = STATUS_CODE(kUserInputParamErr);
        return reply;
    }

    // QFuture<void> future = QtConcurrent::run(pool.data(), [=]() { d->Uninstall(paramOption); });
    return d->Uninstall(paramOption);
}

Reply PackageManager::Update(const ParamOption &paramOption)
{
    Q_D(PackageManager);

    Reply reply;
    QString appId = paramOption.appId.trimmed();
    if (appId.isEmpty()) {
        reply.message = "appId input err";
        reply.code = STATUS_CODE(kUserInputParamErr);
        return reply;
    }

    QFuture<void> future = QtConcurrent::run(pool.data(), [=]() {
        d->Update(paramOption);
    });
    reply.code = STATUS_CODE(kPkgUpdating);
    reply.message = appId + " is updating";
    return reply;
}

QueryReply PackageManager::Query(const QueryParamOption &paramOption)
{
    Q_D(PackageManager);

    QueryReply reply;
    QString appId = paramOption.appId.trimmed();
    if (appId.isEmpty()) {
        reply.code = STATUS_CODE(kUserInputParamErr);
        reply.message = "appId input err";
        qCritical() << reply.message;
        return reply;
    }
    return d->Query(paramOption);
}

void PackageManager::setNoDBusMode(bool enable)
{
    Q_D(PackageManager);
    d->noDBusMode = true;
    qInfo() << "setNoDBusMode enable:" << enable;
}

} // namespace service
} // namespace linglong
