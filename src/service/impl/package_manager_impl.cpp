/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     huqinghong@uniontech.com
 *
 * Maintainer: huqinghong@uniontech.com
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "module/repo/repohelper_factory.h"
#include "module/repo/ostree_repohelper.h"
#include "module/util/app_status.h"
#include "module/util/appinfo_cache.h"
#include "module/util/httpclient.h"
#include "module/util/package_manager_param.h"
#include "module/util/sysinfo.h"
#include "module/util/runner.h"

#include "package_manager_impl.h"
#include "dbus_retcode.h"

using linglong::dbus::RetCode;
using namespace linglong::Status;
using namespace linglong::util;

const QString kAppInstallPath = "/deepin/linglong/layers/";
const QString kLocalRepoPath = "/deepin/linglong/repo";

/*
 * 将json字符串转化为软件包数据
 *
 * @param jsonString: 软件包对应的json字符串
 * @param appList: 转换结果
 * @param err: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
bool PackageManagerImpl::loadAppInfo(const QString &jsonString, AppMetaInfoList &appList, QString &err)
{
    QJsonParseError parseJsonErr;
    QJsonDocument document = QJsonDocument::fromJson(jsonString.toUtf8(), &parseJsonErr);
    if (QJsonParseError::NoError != parseJsonErr.error) {
        err = "parse json data err";
        return false;
    }

    QJsonObject jsonObject = document.object();
    if (jsonObject.size() == 0) {
        err = "receive data is empty";
        return false;
    }

    if (!jsonObject.contains("code") || !jsonObject.contains("data")) {
        err = "receive data format err";
        return false;
    }

    int code = jsonObject["code"].toInt();
    if (code != 0) {
        err = "app not found in repo";
        qCritical().noquote() << jsonString;
        return false;
    }

    QJsonValue arrayValue = jsonObject.value(QStringLiteral("data"));
    if (!arrayValue.isArray()) {
        err = "jsonString from server data format is not array";
        qCritical().noquote() << jsonString;
        return false;
    }

    // 多个结果以json 数组形式返回
    QJsonArray arr = arrayValue.toArray();
    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject dataObj = arr.at(i).toObject();
        const QString jsonString = QString(QJsonDocument(dataObj).toJson(QJsonDocument::Compact));
        // qInfo().noquote() << jsonString;
        auto appItem = linglong::util::loadJSONString<AppMetaInfo>(jsonString);
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
bool PackageManagerImpl::getAppInfofromServer(const QString &pkgName, const QString &pkgVer, const QString &pkgArch,
                                              QString &appData, QString &err)
{
    linglong::util::HttpClient *httpClient = linglong::util::HttpClient::getInstance();
    bool ret = httpClient->queryRemote(pkgName, pkgVer, pkgArch, appData);
    httpClient->release();
    if (!ret) {
        err = "getAppInfofromServer err";
        qCritical() << err;
        return false;
    }

    qDebug().noquote() << appData;
    return true;
}

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
bool PackageManagerImpl::downloadAppData(const QString &pkgName, const QString &pkgVer, const QString &pkgArch,
                                         const QString &dstPath, QString &err)
{
    bool ret = G_OSTREE_REPOHELPER->ensureRepoEnv(kLocalRepoPath, err);
    if (!ret) {
        qCritical() << err;
        return false;
    }
    QVector<QString> qrepoList;
    ret = G_OSTREE_REPOHELPER->getRemoteRepoList(kLocalRepoPath, qrepoList, err);
    if (!ret) {
        qCritical() << err;
        return false;
    } else {
        for (auto iter = qrepoList.begin(); iter != qrepoList.end(); ++iter) {
            qInfo() << "downloadAppData remote reponame:" << *iter;
        }
    }

    // ref format --> app/org.deepin.calculator/x86_64/1.2.2
    // QString matchRef = QString("app/%1/%2/%3").arg(pkgName).arg(pkgArch).arg(pkgVer);
    // QString pkgName = "us.zoom.Zoom";
    QString matchRef = "";
    ret = G_REPOHELPER->queryMatchRefs(kLocalRepoPath, qrepoList[0], pkgName, pkgVer, pkgArch, matchRef, err);
    if (!ret) {
        qCritical() << err;
        return false;
    } else {
        qInfo() << "downloadAppData ref:" << matchRef;
    }

    // ret = repo.repoPull(repoPath, qrepoList[0], pkgName, err);
    ret = G_REPOHELPER->repoPullbyCmd(kLocalRepoPath, qrepoList[0], matchRef, err);
    if (!ret) {
        qCritical() << err;
        return false;
    }
    // checkout 目录
    // const QString dstPath = repoPath + "/AppData";
    ret = G_OSTREE_REPOHELPER->checkOutAppData(kLocalRepoPath, qrepoList[0], matchRef, dstPath, err);
    if (!ret) {
        qCritical() << err;
        return false;
    }
    qInfo() << "downloadAppData success, path:" << dstPath;

    return ret;
}

/*!
 * 下载软件包
 * @param packageIdList
 */
RetMessageList PackageManagerImpl::Download(const QStringList &packageIdList, const QString &savePath)
{
    return {};
}

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
bool PackageManagerImpl::installRuntime(const QString &runtimeId, const QString &runtimeVer, const QString &runtimeArch,
                                        QString &err)
{
    AppMetaInfoList appList;
    QString appData = "";

    bool ret = getAppInfofromServer(runtimeId, runtimeVer, runtimeArch, appData, err);
    if (!ret) {
        return false;
    }
    ret = loadAppInfo(appData, appList, err);
    if (!ret) {
        qCritical() << err;
        return false;
    }
    // app runtime 只能匹配一个
    if (appList.size() != 1) {
        err = "app:" + runtimeId + ", version:" + runtimeVer + " not found in repo";
        return false;
    }

    auto pkgInfo = appList.at(0);
    const QString savePath = kAppInstallPath + runtimeId + "/" + runtimeVer + "/" + runtimeArch;
    // 创建路径
    linglong::util::createDir(savePath);
    ret = downloadAppData(runtimeId, runtimeVer, runtimeArch, savePath, err);
    if (!ret) {
        err = "installRuntime download runtime data err";
        return false;
    }

    // 更新本地数据库文件
    QString userName = getUserName();
    pkgInfo->kind = "runtime";
    insertAppRecord(pkgInfo, "user", userName);
    return true;
}

/*
 * 检查应用runtime安装状态
 *
 * @param runtime: 应用runtime字符串
 * @param err: 错误信息
 *
 * @return bool: true:安装成功或已安装返回true false:安装失败
 */
bool PackageManagerImpl::checkAppRuntime(const QString &runtime, QString &err)
{
    // runtime ref in repo org.deepin.Runtime/20/x86_64
    QStringList runtimeInfo = runtime.split("/");
    if (runtimeInfo.size() != 3) {
        err = "app runtime:" + runtime + " runtime format err";
        return false;
    }
    const QString runtimeId = runtimeInfo.at(0);
    const QString runtimeVer = runtimeInfo.at(1);
    const QString runtimeArch = runtimeInfo.at(2);

    bool ret = true;
    // 判断app依赖的runtime是否安装
    QString userName = getUserName();
    if (!getAppInstalledStatus(runtimeId, runtimeVer, "", userName)) {
        ret = installRuntime(runtimeId, runtimeVer, runtimeArch, err);
    }
    return ret;
}

/*!
 * 在线安装软件包
 * @param packageIdList
 */
RetMessageList PackageManagerImpl::Install(const QStringList &packageIdList, const ParamStringMap &paramMap)
{
    RetMessageList retMsg;
    bool ret = false;
    auto info = QPointer<RetMessage>(new RetMessage);
    QString pkgName = packageIdList.at(0);
    if (pkgName.isNull() || pkgName.isEmpty()) {
        qCritical() << "package name err";
        info->setcode(RetCode(RetCode::user_input_param_err));
        info->setmessage("package name err");
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }

    QString err = "";

    // const QString arch = "x86_64";
    const QString arch = hostArch();
    if (arch == "unknown") {
        qCritical() << "the host arch is not recognized";
        info->setcode(RetCode(RetCode::host_arch_not_recognized));
        info->setmessage("the host arch is not recognized");
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }

    // 获取版本信息
    QString version = "";
    if (!paramMap.empty() && paramMap.contains(linglong::util::KEY_VERSION)) {
        version = paramMap[linglong::util::KEY_VERSION];
    }

    QString userName = getUserName();
    QString appData = "";
    // 安装不查缓存
    ret = getAppInfofromServer(pkgName, version, arch, appData, err);
    if (!ret) {
        qCritical() << err;
        info->setcode(RetCode(RetCode::pkg_install_failed));
        info->setmessage(err);
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }
    AppMetaInfoList appList;
    ret = loadAppInfo(appData, appList, err);
    if (!ret) {
        qCritical() << err;
        info->setcode(RetCode(RetCode::pkg_install_failed));
        info->setmessage(err);
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }
    if (appList.size() != 1) {
        err = "app:" + pkgName + ", version:" + version + " not found in repo";
        qCritical() << err;
        info->setcode(RetCode(RetCode::pkg_install_failed));
        info->setmessage(err);
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }

    auto appInfo = appList.at(0);
    // 判断对应版本的应用是否已安装
    if (getAppInstalledStatus(pkgName, appInfo->version, "", userName)) {
        qCritical() << pkgName << ", version: " << appInfo->version << " already installed";
        info->setcode(RetCode(RetCode::pkg_already_installed));
        info->setmessage(pkgName + ", version: " + appInfo->version + " already installed");
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }

    // 检查软件包依赖的runtime安装状态
    ret = checkAppRuntime(appInfo->runtime, err);
    if (!ret) {
        qCritical() << err;
        info->setcode(RetCode(RetCode::install_runtime_failed));
        info->setmessage(err);
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }

    // 下载在线包数据到目标目录 安装完成
    // QString pkgName = "org.deepin.calculator";
    const QString savePath = kAppInstallPath + appInfo->appId + "/" + appInfo->version + "/" + appInfo->arch;
    ret = downloadAppData(appInfo->appId, appInfo->version, appInfo->arch, savePath, err);
    if (!ret) {
        qCritical() << err;
        info->setcode(RetCode(RetCode::load_pkg_data_failed));
        info->setmessage(err);
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }

    // 链接应用配置文件到系统配置目录
    if (linglong::util::dirExists(savePath + "/outputs/share")) {
        const QString appEntriesDirPath = savePath + "/outputs/share";
        linglong::util::linkDirFiles(appEntriesDirPath, sysLinglongInstalltions);
    } else {
        const QString appEntriesDirPath = savePath + "/entries";
        linglong::util::linkDirFiles(appEntriesDirPath, sysLinglongInstalltions);
    }
    // 更新desktop database
    auto retRunner = linglong::runner::Runner("update-desktop-database", {sysLinglongInstalltions + "/applications/"},
                                   1000 * 60 * 1);
    if (!retRunner) {
         qWarning() << "warning: update desktop database of " + sysLinglongInstalltions + "/applications/ failed!";
    }
    // 更新本地数据库文件
    appInfo->kind = "app";
    insertAppRecord(appInfo, "user", userName);

    info->setcode(RetCode(RetCode::pkg_install_success));
    info->setmessage("install " + pkgName + ", version:" + appInfo->version + " success");
    info->setstate(true);
    retMsg.push_back(info);
    return retMsg;
}

/*!
 * 查询软件包
 * @param packageIdList: 软件包的appId
 *
 * @return AppMetaInfoList 查询结果列表
 */
AppMetaInfoList PackageManagerImpl::Query(const QStringList &packageIdList, const ParamStringMap &paramMap)
{
    const QString pkgName = packageIdList.at(0);
    if (pkgName.isNull() || pkgName.isEmpty()) {
        qWarning() << "package name err";
        return {};
    }
    if (pkgName == "installed") {
        return queryAllInstalledApp();
    }
    AppMetaInfoList pkgList;
    // 查找单个软件包 优先从本地数据库文件中查找
    QString arch = hostArch();
    if (arch == "unknown") {
        qCritical() << "the host arch is not recognized";
        return pkgList;
    }

    QString userName = getUserName();
    bool ret = getInstalledAppInfo(pkgName, "", arch, userName, pkgList);

    // 目标软件包 已安装则终止查找
    qInfo() << "PackageManager::Query called, ret:" << ret;
    if (ret) {
        return pkgList;
    }

    QString err = "";
    QString appData = "";
    int status = StatusCode::FAIL;

    if (!paramMap.contains(linglong::util::KEY_NO_CACHE)) {
        status = queryLocalCache(pkgName, appData);
    }

    bool fromServer = false;
    // 缓存查不到从服务器查
    if (status != StatusCode::SUCCESS) {
        ret = getAppInfofromServer(pkgName, "", arch, appData, err);
        if (!ret) {
            qCritical() << err;
            return pkgList;
        }
        fromServer = true;
    }
    ret = loadAppInfo(appData, pkgList, err);
    qInfo().noquote() << appData;
    if (!ret) {
        qCritical() << err;
        return pkgList;
    }
    // 若从服务器查询得到正确的数据则更新缓存
    if (fromServer) {
        updateCache(pkgName, appData);
    }
    return pkgList;
}

/*
 * 卸载软件包
 *
 * @param packageIdList: 软件包的appId
 *
 * @return RetMessageList 卸载结果信息
 */
RetMessageList PackageManagerImpl::Uninstall(const QStringList &packageIdList, const ParamStringMap &paramMap)
{
    RetMessageList retMsg;
    auto info = QPointer<RetMessage>(new RetMessage);
    const QString pkgName = packageIdList.at(0);

    // 获取版本信息
    QString version = "";
    if (!paramMap.empty() && paramMap.contains(linglong::util::KEY_VERSION)) {
        version = paramMap[linglong::util::KEY_VERSION];
    }

    // 判断是否已安装
    QString userName = getUserName();
    if (!getAppInstalledStatus(pkgName, version, "", userName)) {
        qCritical() << pkgName << " not installed";
        info->setcode(RetCode(RetCode::pkg_not_installed));
        info->setmessage(pkgName + " not installed");
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }
    QString err = "";
    AppMetaInfoList pkgList;
    // 根据已安装文件查询已经安装软件包信息
    QString arch = hostArch();
    getInstalledAppInfo(pkgName, version, arch, userName, pkgList);

    auto it = pkgList.at(0);
    if (pkgList.size() > 0) {
        const QString installPath = kAppInstallPath + it->appId + "/" + it->version;
        // 删掉安装配置链接文件
        if (linglong::util::dirExists(installPath + "/" + arch + "/outputs/share")) {
            const QString appEntriesDirPath = installPath + "/" + arch + "/outputs/share";
            linglong::util::removeDstDirLinkFiles(appEntriesDirPath, sysLinglongInstalltions);
        } else {
            const QString appEntriesDirPath = installPath + "/" + arch + "/entries";
            linglong::util::removeDstDirLinkFiles(appEntriesDirPath, sysLinglongInstalltions);
        }
        linglong::util::removeDir(installPath);
        qInfo() << "Uninstall del dir:" << installPath;
    }

     // 更新desktop database
    auto retRunner = linglong::runner::Runner("update-desktop-database", {sysLinglongInstalltions + "/applications/"},
                                   1000 * 60 * 1);
    if (!retRunner) {
        qWarning() << "warning: update desktop database of " + sysLinglongInstalltions + "/applications/ failed!";
    }

    // 更新本地repo仓库
    bool ret = G_OSTREE_REPOHELPER->ensureRepoEnv(kLocalRepoPath, err);
    if (!ret) {
        qCritical() << err;
        info->setcode(RetCode(RetCode::pkg_uninstall_failed));
        info->setmessage("uninstall local repo not exist");
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }
    // 应从安装数据库获取应用所属仓库信息 to do fix
    QVector<QString> qrepoList;
    ret = G_OSTREE_REPOHELPER->getRemoteRepoList(kLocalRepoPath, qrepoList, err);
    if (!ret) {
        qCritical() << err;
        info->setcode(RetCode(RetCode::pkg_uninstall_failed));
        info->setmessage("uninstall remote repo not exist");
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }

    // new ref format org.deepin.calculator/1.2.2/x86_64
    // QString matchRef = QString("app/%1/%2/%3").arg(it->appId).arg(arch).arg(it->version);
    QString matchRef = QString("%1/%2/%3").arg(it->appId).arg(it->version).arg(arch);
    qInfo() << "Uninstall app ref:" << matchRef;
    ret = G_OSTREE_REPOHELPER->repoDeleteDatabyRef(kLocalRepoPath, qrepoList[0], matchRef, err);
    if (!ret) {
        qCritical() << err;
        info->setcode(RetCode(RetCode::pkg_uninstall_failed));
        info->setmessage("uninstall " + pkgName + ",version:" + it->version + " failed");
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }

    // 更新安装数据库
    deleteAppRecord(pkgName, it->version, "", userName);
    info->setcode(RetCode(RetCode::pkg_uninstall_success));
    info->setmessage("uninstall " + pkgName + ",version:" + it->version + " success");
    info->setstate(true);
    retMsg.push_back(info);
    return retMsg;
}