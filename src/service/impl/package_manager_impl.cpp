/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     huqinghong@uniontech.com
 *
 * Maintainer: huqinghong@uniontech.com
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <pwd.h>
#include <sys/types.h>

#include "module/repo/repohelper_factory.h"
#include "module/repo/ostree_repohelper.h"
#include "module/util/httpclient.h"
#include "module/util/package_manager_param.h"
#include "package_manager_impl.h"
#include "dbus_retcode.h"
#include "version.h"

using linglong::util::fileExists;
using linglong::dbus::RetCode;

/*
 * 查询系统架构
 *
 * @return QString: 系统架构字符串
 */
QString PackageManagerImpl::getHostArch()
{
    // other CpuArchitecture ie i386 i486 to do fix
    const QString arch = QSysInfo::currentCpuArchitecture();
    Qt::CaseSensitivity cs = Qt::CaseInsensitive;
    if (arch.startsWith("x86_64", cs)) {
        return "x86_64";
    }
    if (arch.startsWith("arm", cs)) {
        return "arm64";
    }
    if (arch.startsWith("mips", cs)) {
        return "mips64";
    }
    return "unknown";
}

/*
 * 查询当前登陆用户名
 *
 * @return QString: 当前登陆用户名
 */
QString PackageManagerImpl::getUserName()
{
    // QString userPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    // QString userName = userPath.section("/", -1, -1);
    // return userName;
    uid_t uid = geteuid();
    struct passwd *user = getpwuid(uid);
    QString userName = "";
    if (user && user->pw_name) {
        userName = QString(QLatin1String(user->pw_name));
    } else {
        qInfo() << "getUserName err";
    }
    return userName;
}

/*
 * 更新本地AppStream.json文件
 *
 * @param savePath: AppStream.json文件存储路径
 * @param remoteName: 远端仓库名称
 * @param err: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
bool PackageManagerImpl::updateAppStream(const QString &savePath, const QString &remoteName, QString &err)
{
    // FIXME(huqinghong): 获取AppStream.json 地址 to do fix
    const QString xmlPath = "https://repo.linglong.space/xml/AppStream.json";
    /// deepin/linglong
    QString fullPath = savePath + remoteName;
    //创建下载目录
    linglong::util::createDir(fullPath);
    linglong::util::HttpClient *httpClient = linglong::util::HttpClient::getInstance();
    bool ret = httpClient->loadHttpData(xmlPath, fullPath);
    httpClient->release();
    if (!ret) {
        err = "updateAppStream load appstream.json err";
    }
    return ret;
}

/*
 * 比较给定的软件包版本
 *
 * @param curVersion: 当前软件包版本
 * @param dstVersion: 目标软件包版本
 *
 * @return bool: dstVersion 比 curVersion版本新返回true,否则返回false
 */
bool PackageManagerImpl::cmpAppVersion(const QString &curVersion, const QString &dstVersion)
{
    linglong::AppVersion curVer(curVersion);
    linglong::AppVersion dstVer(dstVersion);
    if (!curVer.isValid()) {
        return true;
    }
    if (!dstVer.isValid()) {
        return false;
    }
    qInfo() << "curVersion:" << curVersion << ", dstVersion:" << dstVersion;
    return dstVer.isBigThan(curVer);
}

/*
 * 根据匹配的软件包列表查找最新版本软件包信息
 *
 * @param verMap: 目标软件包版本信息
 *
 * @return QString: 最新版本软件包信息
 */
QString PackageManagerImpl::getLatestAppInfo(const QMap<QString, QString> &verMap)
{
    if (verMap.size() == 1) {
        return verMap.keys().at(0);
    }
    QString latesVer = linglong::APP_MIN_VERSION;
    QString ret = "";
    for (QString key : verMap.keys()) {
        // AppStream.json中的key值格式为 "org.deepin.calculator_2.2.4"
        QString ver = verMap[key];
        if (cmpAppVersion(latesVer, ver)) {
            latesVer = ver;
            ret = key;
        }
        qInfo() << "latest version:" << latesVer;
    }
    return ret;
}

/*
 * 根据AppStream.json查询目标软件包信息
 *
 * @param savePath: AppStream.json文件存储路径
 * @param remoteName: 远端仓库名称
 * @param pkgName: 软件包包名
 * @param pkgVer: 软件包版本
 * @param pkgArch: 软件包对应的架构
 * @param pkgInfo: 查询结果
 * @param err: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
bool PackageManagerImpl::getAppInfoByAppStream(const QString &savePath, const QString &remoteName,
                                               const QString &pkgName, const QString &pkgVer, const QString &pkgArch,
                                               AppStreamPkgInfo &pkgInfo, QString &err)
{
    //判断文件是否存在
    // deepin/linglong
    QString fullPath = savePath + remoteName + "/AppStream.json";
    if (!linglong::util::fileExists(fullPath)) {
        err = fullPath + " is not exist";
        return false;
    }

    QFile jsonFile(fullPath);
    jsonFile.open(QIODevice::ReadOnly | QIODevice::Text);
    QString qValue = jsonFile.readAll();
    jsonFile.close();
    QJsonParseError parseJsonErr;
    QJsonDocument document = QJsonDocument::fromJson(qValue.toUtf8(), &parseJsonErr);
    if (!(parseJsonErr.error == QJsonParseError::NoError)) {
        qCritical() << "getAppInfoByAppStream parse json file wrong";
        err = fullPath + " json file wrong";
        return false;
    }

    // 自定义软件包信息metadata 继承JsonSerialize 来处理，to do fix
    QJsonObject jsonObject = document.object();
    if (jsonObject.size() == 0) {
        err = fullPath + " is empty";
        return false;
    }

    // 查找指定版本和架构的软件包是否存在
    if (!pkgVer.isEmpty()) {
        QString appKey = pkgName + "_" + pkgVer;
        if (!jsonObject.contains(appKey)) {
            err = pkgName + "-" + pkgVer + " not found";
            return false;
        }

        QJsonObject subObj = jsonObject[appKey].toObject();
        // 判断指定架构是否存在 to do fix optimized code
        QJsonValue arrayValue = subObj.value(QStringLiteral("arch"));
        if (arrayValue.isArray()) {
            QJsonArray arr = arrayValue.toArray();
            bool flag = false;
            for (int i = 0; i < arr.size(); i++) {
                QString item = arr.at(i).toString();
                if (item == pkgArch) {
                    flag = true;
                    break;
                }
            }
            if (!flag) {
                err = pkgName + "-" + pkgArch + " not support";
                return false;
            }
        }
        pkgInfo.appId = subObj["appid"].toString();
        pkgInfo.appName = subObj["name"].toString();
        pkgInfo.appVer = subObj["version"].toString();
        pkgInfo.appUrl = subObj["appUrl"].toString();
        pkgInfo.summary = subObj["summary"].toString();
        pkgInfo.runtime = subObj["runtime"].toString();
        pkgInfo.reponame = subObj["reponame"].toString();
        pkgInfo.appArch = pkgArch;
        return true;
    }

    QStringList pkgsList = jsonObject.keys();
    QString filterString = ".*" + pkgName + ".*";
    QStringList appList = pkgsList.filter(QRegExp(filterString, Qt::CaseInsensitive));
    if (appList.isEmpty()) {
        err = "app:" + pkgName + " not found";
        qInfo() << err;
        return false;
    }
    QMap<QString, QString> verMap;
    for (QString key : appList) {
        QJsonObject tmp = jsonObject[key].toObject();
        QString value = tmp["version"].toString();
        verMap.insert(key, value);
    }
    QString appKey = getLatestAppInfo(verMap);
    qInfo() << "latest appKey:" << appKey;
    if (!jsonObject.contains(appKey)) {
        err = "getLatestAppInfo err";
        return false;
    }
    QJsonObject subObj = jsonObject[appKey].toObject();
    appStreamPkgInfo.appId = subObj["appid"].toString();
    // 精确匹配
    if (appStreamPkgInfo.appId != pkgName) {
        err = "getLatestAppInfo " + pkgName + " err";
        return false;
    }

    // 判断指定架构是否存在  to do fix optimized code
    QJsonValue arrayValue = subObj.value(QStringLiteral("arch"));
    if (arrayValue.isArray()) {
        QJsonArray arr = arrayValue.toArray();
        bool flag = false;
        for (int i = 0; i < arr.size(); i++) {
            QString item = arr.at(i).toString();
            if (item == pkgArch) {
                flag = true;
                break;
            }
        }
        if (!flag) {
            err = pkgName + "-" + pkgArch + " not support";
            return false;
        }
    }
    pkgInfo.appName = subObj["name"].toString();
    pkgInfo.appVer = subObj["version"].toString();
    pkgInfo.appUrl = subObj["appUrl"].toString();
    pkgInfo.summary = subObj["summary"].toString();
    pkgInfo.runtime = subObj["runtime"].toString();
    pkgInfo.reponame = subObj["reponame"].toString();
    pkgInfo.appArch = pkgArch;
    return true;
}

/*
 * 查询软件包安装状态
 *
 * @param pkgName: 软件包包名
 * @param pkgVer: 软件包版本号
 * @param pkgArch: 软件包对应的架构
 * @param userName: 用户名，默认为当前用户
 *
 * @return bool: 1:已安装 0:未安装 -1查询失败
 */
int PackageManagerImpl::getInstallStatus(const QString &pkgName, const QString &pkgVer, const QString &pkgArch,
                                         const QString &userName)
{
    if (pkgName.isNull() || pkgName.isEmpty()) {
        return -1;
    }
    // 数据库的文件路径
    QString dbPath = "/deepin/linglong/layers/AppInfoDB.json";
    if (!linglong::util::fileExists(dbPath)) {
        return 0;
    }

    QString dstUserName = userName;
    // 默认为当前用户
    if (userName.isNull() || userName.isEmpty()) {
        dstUserName = getUserName();
    }
    QFile dbFile(dbPath);
    if (!dbFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << "db file open failed!";
        return -1;
    }
    // 读取文件的全部内容
    QString qValue = dbFile.readAll();
    dbFile.close();
    QJsonParseError parseJsonErr;
    QJsonDocument document = QJsonDocument::fromJson(qValue.toUtf8(), &parseJsonErr);
    if (!(parseJsonErr.error == QJsonParseError::NoError)) {
        qCritical() << "getInstallStatus parse json file err";
        return -1;
    }
    QJsonObject jsonObject = document.object();
    QJsonObject usersObject = jsonObject["users"].toObject();
    // 用户名存在
    if (usersObject.contains(dstUserName)) {
        QJsonObject userNameObject = usersObject[dstUserName].toObject();
        // 包名不存在
        if (!userNameObject.contains(pkgName)) {
            return 0;
        }
        // 判断指定版本是否存在
        if (!pkgVer.isEmpty()) {
            QJsonObject userNameObject = usersObject[dstUserName].toObject();
            QJsonObject usersPkgObject = userNameObject[pkgName].toObject();
            QJsonValue arrayValue = usersPkgObject.value(QStringLiteral("version"));
            QJsonArray versionArray = arrayValue.toArray();
            bool flag = false;
            for (int i = 0; i < versionArray.size(); i++) {
                QString item = versionArray.at(i).toString();
                if (item == pkgVer) {
                    flag = true;
                }
            }
            // 指定版本不存在
            if (!flag) {
                return 0;
            }
        }
        // 判断指定arch是否存在
        QJsonObject pkgsObject = jsonObject["pkgs"].toObject();
        if (!pkgArch.isEmpty()) {
            QJsonObject pkgObject = pkgsObject[pkgName].toObject();
            QString appArch = pkgObject["arch"].toString();
            if (pkgArch != appArch) {
                return 0;
            }
        }
        return 1;
    }
    return 0;
}

/*
 * 将OUAP在线包数据部分签出到指定目录
 *
 * @param pkgName: 软件包包名
 * @param pkgVer: 软件包版本号
 * @param pkgArch: 软件包对应的架构
 * @param dstPath: OUAP在线包数据部分存储路径
 * @param err: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
bool PackageManagerImpl::downloadAppData(const QString &pkgName, const QString &pkgVer, const QString &pkgArch,
                                          const QString &dstPath, QString &err)
{
    // linglong::OstreeRepoHelper repo;
    const QString repoPath = "/deepin/linglong/repo";
    bool ret = G_OSTREE_REPOHELPER->ensureRepoEnv(repoPath, err);
    // bool ret = repo->ensureRepoEnv(repoPath, err);
    if (!ret) {
        qInfo() << err;
        return false;
    }
    QVector<QString> qrepoList;
    ret = G_OSTREE_REPOHELPER->getRemoteRepoList(repoPath, qrepoList, err);
    if (!ret) {
        qInfo() << err;
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
    ret = G_REPOHELPER->queryMatchRefs(repoPath, qrepoList[0], pkgName, pkgVer, pkgArch, matchRef, err);
    if (!ret) {
        qInfo() << err;
        return false;
    } else {
        qInfo() << "downloadAppData ref:" << matchRef;
    }

    // ret = repo.repoPull(repoPath, qrepoList[0], pkgName, err);
    ret = G_REPOHELPER->repoPullbyCmd(repoPath, qrepoList[0], matchRef, err);
    if (!ret) {
        qInfo() << err;
        return false;
    }
    // checkout 目录
    // const QString dstPath = repoPath + "/AppData";
    ret = G_OSTREE_REPOHELPER->checkOutAppData(repoPath, qrepoList[0], matchRef, dstPath, err);
    if (!ret) {
        qInfo() << err;
        return false;
    }
    qInfo() << "downloadAppData success, path:" << dstPath;

    return ret;
}

/*
 * 更新应用安装状态到本地文件
 *
 * @param appStreamPkgInfo: 安装成功的软件包信息
 *
 * @return bool: true:成功 false:失败
 */
bool PackageManagerImpl::updateAppStatus(AppStreamPkgInfo appStreamPkgInfo)
{
    // file lock to do
    // 数据库的文件路径
    QString dbPath = "/deepin/linglong/layers/AppInfoDB.json";
    QFile dbFile(dbPath);
    // 首次安装
    if (!linglong::util::fileExists(dbPath)) {
        dbFile.open(QIODevice::WriteOnly | QIODevice::Text);
        QJsonObject jsonObject;
        // QJsonArray emptyArray;
        QJsonObject emptyObject;
        // jsonObject.insert("pkgs", emptyArray);
        jsonObject.insert("pkgs", emptyObject);
        jsonObject.insert("users", emptyObject);
        QJsonDocument jsonDocTmp;
        jsonDocTmp.setObject(jsonObject);
        dbFile.write(jsonDocTmp.toJson());
        dbFile.close();
    }

    if (!dbFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << "db file open failed!";
        return false;
    }
    // 读取文件的全部内容
    QString qValue = dbFile.readAll();
    dbFile.close();
    QJsonParseError parseJsonErr;
    QJsonDocument document = QJsonDocument::fromJson(qValue.toUtf8(), &parseJsonErr);
    if (!(parseJsonErr.error == QJsonParseError::NoError)) {
        qCritical() << "updateAppStatus parse json file err";
        return false;
    }

    QJsonObject jsonObject = document.object();
    QJsonObject pkgsObject = jsonObject["pkgs"].toObject();
    QJsonObject appItemValue;
    appItemValue.insert("name", appStreamPkgInfo.appName);
    appItemValue.insert("arch", appStreamPkgInfo.appArch);
    appItemValue.insert("summary", appStreamPkgInfo.summary);
    appItemValue.insert("runtime", appStreamPkgInfo.runtime);
    appItemValue.insert("reponame", appStreamPkgInfo.reponame);
    QJsonArray userArray;
    // different user install same app to do fix
    QString userName = getUserName();
    userArray.append(userName);
    appItemValue.insert("users", userArray);
    pkgsObject.insert(appStreamPkgInfo.appId, appItemValue);

    QJsonObject usersObject = jsonObject["users"].toObject();
    QJsonObject usersSubItem;
    usersSubItem.insert("ref", "app:" + appStreamPkgInfo.appId);
    // usersSubItem.insert("commitv", "0123456789");
    QJsonObject userItem;

    // users下用户名已存在,将软件包添加到该用户名对应的软件包列表中
    QJsonObject userNameObject;
    QJsonArray versionArray;
    if (usersObject.contains(userName)) {
        userNameObject = usersObject[userName].toObject();
        // 首次安装该应用
        if (!userNameObject.contains(appStreamPkgInfo.appId)) {
            versionArray.append(appStreamPkgInfo.appVer);
            usersSubItem.insert("version", versionArray);
            userNameObject.insert(appStreamPkgInfo.appId, usersSubItem);
        } else {
            QJsonObject usersPkgObject = userNameObject[appStreamPkgInfo.appId].toObject();
            QJsonValue arrayValue = usersPkgObject.value(QStringLiteral("version"));
            versionArray = arrayValue.toArray();
            versionArray.append(appStreamPkgInfo.appVer);
            usersPkgObject["version"] = versionArray;
            userNameObject[appStreamPkgInfo.appId] = usersPkgObject;
        }
        usersObject[userName] = userNameObject;
    } else {
        versionArray.append(appStreamPkgInfo.appVer);
        usersSubItem.insert("version", versionArray);
        userItem.insert(appStreamPkgInfo.appId, usersSubItem);
        usersObject.insert(userName, userItem);
    }

    jsonObject.insert("pkgs", pkgsObject);
    jsonObject.insert("users", usersObject);

    document.setObject(jsonObject);
    dbFile.open(QIODevice::WriteOnly | QIODevice::Text);
    // 将修改后的内容写入文件
    QTextStream wirteStream(&dbFile);
    // 设置编码UTF8
    wirteStream.setCodec("UTF-8");
    wirteStream << document.toJson();
    dbFile.close();
    return true;
}

/*!
 * 下载软件包
 * @param packageIDList
 */
RetMessageList PackageManagerImpl::Download(const QStringList &packageIDList, const QString &savePath)
{
    return {};
}

/*
 * 查询当前用户已安装的软件包
 *
 * @return PKGInfoList: 查询结果
 */
PKGInfoList PackageManagerImpl::queryAllInstalledApp()
{
    PKGInfoList pkglist;
    // 数据库的文件路径
    QString dbPath = "/deepin/linglong/layers/AppInfoDB.json";
    if (!linglong::util::fileExists(dbPath)) {
        return pkglist;
    }

    // 默认查找当前用户安装的app
    QString dstUserName = getUserName();

    QFile dbFile(dbPath);
    if (!dbFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << "db file open failed!";
        return pkglist;
    }
    // 读取文件的全部内容
    QString qValue = dbFile.readAll();
    dbFile.close();
    QJsonParseError parseJsonErr;
    QJsonDocument document = QJsonDocument::fromJson(qValue.toUtf8(), &parseJsonErr);
    if (!(parseJsonErr.error == QJsonParseError::NoError)) {
        qCritical() << "queryAllInstalledApp parse json file err";
        return pkglist;
    }
    QJsonObject jsonObject = document.object();
    QJsonObject usersObject = jsonObject["users"].toObject();
    // 用户名存在
    if (usersObject.contains(dstUserName)) {
        QJsonObject userNameObject = usersObject[dstUserName].toObject();
        QStringList appList = userNameObject.keys();
        QJsonObject pkgsObject = jsonObject["pkgs"].toObject();
        for (QString pkgName : appList) {
            QJsonObject pkgObject = pkgsObject[pkgName].toObject();
            QJsonObject usersPkgObject = userNameObject[pkgName].toObject();
            QJsonValue arrayValue = usersPkgObject.value(QStringLiteral("version"));
            QJsonArray versionArray = arrayValue.toArray();
            for (int i = 0; i < versionArray.size(); i++) {
                QString ver = versionArray.at(i).toString();
                auto info = QPointer<PKGInfo>(new PKGInfo);
                info->appid = pkgName;
                info->appname = pkgObject.value("name").toString();
                info->description = pkgObject["summary"].toString();
                info->arch = pkgObject["arch"].toString();
                info->version = ver;
                pkglist.push_back(info);
            }
        }
    }
    return pkglist;
}

/*
 * 查询已安装软件包信息
 *
 * @param pkgName: 软件包包名
 * @param pkgVer: 软件包版本号
 * @param pkgArch: 软件包对应的架构
 * @param pkgList: 查询结果
 *
 * @return bool: true:成功 false:失败(软件包未安装)
 */
bool PackageManagerImpl::getInstalledAppInfo(const QString &pkgName, const QString &pkgVer, const QString &pkgArch,
                                             PKGInfoList &pkgList)
{
    if (!getInstallStatus(pkgName, pkgVer)) {
        return false;
    }
    // 判断查询类型 pkgName == installed to do
    QString dbPath = "/deepin/linglong/layers/AppInfoDB.json";
    QFile dbFile(dbPath);
    // 读取文件的全部内容
    dbFile.open(QIODevice::ReadOnly | QIODevice::Text);
    QString qValue = dbFile.readAll();
    dbFile.close();
    QJsonDocument document = QJsonDocument::fromJson(qValue.toUtf8());
    QJsonObject jsonObject = document.object();
    QJsonObject pkgsObject = jsonObject["pkgs"].toObject();
    QJsonObject pkgObject = pkgsObject[pkgName].toObject();

    QString appName = pkgObject.value("name").toString();
    QString appSummary = pkgObject["summary"].toString();
    QString appRuntime = pkgObject["runtime"].toString();
    QString appRepo = pkgObject["reponame"].toString();
    QString appArch = pkgObject["arch"].toString();
    // 判断指定架构app是否存在
    if (!pkgArch.isEmpty() && pkgArch != appArch) {
        return false;
    }
    QJsonObject usersObject = jsonObject["users"].toObject();
    const QString dstUserName = getUserName();
    QJsonObject userNameObject = usersObject[dstUserName].toObject();
    QJsonObject usersPkgObject = userNameObject[pkgName].toObject();

    QString dstVer = pkgVer;
    // 版本号为空，返回最高版本信息
    if (pkgVer.isEmpty()) {
        QJsonValue arrayValue = usersPkgObject.value(QStringLiteral("version"));
        QJsonArray versionArray = arrayValue.toArray();
        QString latesVer = linglong::APP_MIN_VERSION;
        for (int i = 0; i < versionArray.size(); i++) {
            QString ver = versionArray.at(i).toString();
            if (cmpAppVersion(latesVer, ver)) {
                latesVer = ver;
            }
        }
        dstVer = latesVer;
    }

    auto info = QPointer<PKGInfo>(new PKGInfo);
    info->appid = pkgName;
    info->appname = appName;
    info->version = dstVer;
    info->arch = appArch;
    info->description = appSummary;
    pkgList.push_back(info);
    return true;
}

/*
 * 根据AppStream.json对软件包进行模糊查找
 *
 * @param savePath: AppStream.json文件存储路径
 * @param remoteName: 远端仓库名称
 * @param pkgName: 软件包包名
 * @param pkgList: 查询结果
 * @param err: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
bool PackageManagerImpl::getSimilarAppInfoByAppStream(const QString &savePath, const QString &remoteName,
                                                      const QString &pkgName, PKGInfoList &pkgList, QString &err)
{
    //判断文件是否存在
    // deepin/linglong
    QString fullPath = savePath + remoteName + "/AppStream.json";
    if (!linglong::util::fileExists(fullPath)) {
        err = fullPath + " is not exist";
        return false;
    }

    QFile jsonFile(fullPath);
    jsonFile.open(QIODevice::ReadOnly | QIODevice::Text);
    QString qValue = jsonFile.readAll();
    jsonFile.close();
    QJsonParseError parseJsonErr;
    QJsonDocument document = QJsonDocument::fromJson(qValue.toUtf8(), &parseJsonErr);
    if (!(parseJsonErr.error == QJsonParseError::NoError)) {
        qCritical() << "getSimilarAppInfoByAppStream parse json file wrong";
        err = fullPath + " json file wrong";
        return false;
    }
    // 自定义软件包信息metadata 继承JsonSerialize 来处理，to do fix
    QJsonObject jsonObject = document.object();
    if (jsonObject.size() > 0) {
        QStringList pkgsList = jsonObject.keys();
        QString filterString = "(" + pkgName + ")+";
        QStringList appList = pkgsList.filter(QRegExp(filterString, Qt::CaseInsensitive));
        if (appList.isEmpty()) {
            err = "app:" + pkgName + " not found";
            qInfo() << err;
            return false;
        } else {
            for (QString appKey : appList) {
                auto info = QPointer<PKGInfo>(new PKGInfo);
                QJsonObject subObj = jsonObject[appKey].toObject();
                info->appid = subObj["appid"].toString();
                info->appname = subObj["name"].toString();
                info->version = subObj["version"].toString();
                info->description = subObj["summary"].toString();
                QString arch;
                QJsonValue arrayValue = subObj.value(QStringLiteral("arch"));
                if (arrayValue.isArray()) {
                    QJsonArray arr = arrayValue.toArray();
                    if (arr.size() > 0) {
                        arch = arr.at(0).toString();
                    }
                }
                info->arch = arch;
                pkgList.push_back(info);
            }
            return true;
        }
    }
    err = fullPath + " is empty";
    return false;
}

/*
 * 查询未安装软件包信息
 *
 * @param pkgName: 软件包包名
 * @param pkgVer: 软件包版本号
 * @param pkgArch: 软件包对应的架构
 * @param pkgList: 查询结果
 * @param err: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
bool PackageManagerImpl::getUnInstalledAppInfo(const QString &pkgName, const QString &pkgVer, const QString &pkgArch,
                                               PKGInfoList &pkgList, QString &err)
{
    bool ret = getAppInfoByAppStream("/deepin/linglong/", "repo", pkgName, pkgVer, pkgArch, appStreamPkgInfo, err);
    if (ret) {
        qInfo() << appStreamPkgInfo.appName << " " << appStreamPkgInfo.appId << " " << appStreamPkgInfo.summary;
        auto info = QPointer<PKGInfo>(new PKGInfo);
        info->appid = appStreamPkgInfo.appId;
        info->appname = appStreamPkgInfo.appName;
        info->version = appStreamPkgInfo.appVer;
        info->arch = appStreamPkgInfo.appArch;
        info->description = appStreamPkgInfo.summary;
        pkgList.push_back(info);
    } else {
        qInfo() << "getUnInstalledAppInfo fuzzy search app:" << pkgName;
        // 模糊匹配
        ret = getSimilarAppInfoByAppStream("/deepin/linglong/", "repo", pkgName, pkgList, err);
    }
    return ret;
}

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
bool PackageManagerImpl::installRuntime(const QString &runtimeID, const QString &runtimeVer, const QString &runtimeArch,
                                        QString &err)
{
    AppStreamPkgInfo pkgInfo;
    bool ret = getAppInfoByAppStream("/deepin/linglong/", "repo", runtimeID, runtimeVer, runtimeArch, pkgInfo, err);
    if (!ret) {
        err = "installRuntime get runtime info err";
        return false;
    }
    const QString savePath = "/deepin/linglong/layers/" + runtimeID + "/" + runtimeVer + "/" + runtimeArch;
    // 创建路径
    linglong::util::createDir(savePath);
    ret = downloadAppData(runtimeID, runtimeVer, runtimeArch, savePath, err);
    if (!ret) {
        err = "installRuntime download runtime data err";
        return false;
    }

    // 更新本地数据库文件
    updateAppStatus(pkgInfo);
    return true;
}

/*
 * 检查应用runtime安装状态
 *
 * @param err: 错误信息
 *
 * @return bool: true:安装成功或已安装返回true false:安装失败
 */
bool PackageManagerImpl::checkAppRuntime(QString &err)
{
    // runtime ref in AppStream com.deepin.Runtime/20/x86_64
    QStringList runtimeInfo = appStreamPkgInfo.runtime.split("/");
    if (runtimeInfo.size() != 3) {
        err = "AppStream.json " + appStreamPkgInfo.appId + " runtime info err";
        return false;
    }
    const QString runtimeID = runtimeInfo.at(0);
    const QString runtimeVer = runtimeInfo.at(1);
    const QString runtimeArch = runtimeInfo.at(2);
    bool ret = true;

    // 判断app依赖的runtime是否安装
    if (!getInstallStatus(runtimeID, runtimeVer)) {
        ret = installRuntime(runtimeID, runtimeVer, runtimeArch, err);
    }
    return ret;
}

/*!
 * 在线安装软件包
 * @param packageIDList
 */
RetMessageList PackageManagerImpl::Install(const QStringList &packageIDList, const ParamStringMap &paramMap)
{
    RetMessageList retMsg;
    bool ret = false;
    auto info = QPointer<RetMessage>(new RetMessage);
    QString pkgName = packageIDList.at(0);
    if (pkgName.isNull() || pkgName.isEmpty()) {
        qInfo() << "package name err";
        info->setcode(RetCode(RetCode::user_input_param_err));
        info->setmessage("package name err");
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }

    QString err = "";

    // const QString arch = "x86_64";
    const QString arch = getHostArch();
    if (arch == "unknown") {
        qInfo() << "the host arch is not recognized";
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

    // 根据包名安装在线包
    // 更新AppStream.json
    ret = updateAppStream("/deepin/linglong/", "repo", err);
    if (!ret) {
        qInfo() << err;
        info->setcode(RetCode(RetCode::update_appstream_failed));
        info->setmessage(err);
        info->setstate(ret);
        retMsg.push_back(info);
        return retMsg;
    }
    // 根据AppStream.json 查找目标软件包
    const QString xmlPath = "/deepin/linglong/repo/AppStream.json";
    ret = getAppInfoByAppStream("/deepin/linglong/", "repo", pkgName, version, arch, appStreamPkgInfo, err);
    if (!ret) {
        qInfo() << err;
        info->setcode(RetCode(RetCode::search_pkg_by_appstream_failed));
        info->setmessage(err);
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }

    // 判断对应版本的应用是否已安装
    if (getInstallStatus(pkgName, appStreamPkgInfo.appVer)) {
        qCritical() << pkgName << ", version: " << appStreamPkgInfo.appVer << " already installed";
        info->setcode(RetCode(RetCode::pkg_already_installed));
        info->setmessage(pkgName + ", version: " + appStreamPkgInfo.appVer + " already installed");
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }

    // 检查软件包依赖的runtime安装状态
    ret = checkAppRuntime(err);
    if (!ret) {
        qCritical() << err;
        info->setcode(RetCode(RetCode::install_runtime_failed));
        info->setmessage(err);
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }

    // 下载OUAP 在线包数据到目标目录 安装完成
    // QString pkgName = "org.deepin.calculator";
    const QString savePath = "/deepin/linglong/layers/" + appStreamPkgInfo.appId + "/" + appStreamPkgInfo.appVer + "/" + appStreamPkgInfo.appArch;
    ret = downloadAppData(appStreamPkgInfo.appId, appStreamPkgInfo.appVer, appStreamPkgInfo.appArch, savePath, err);
    if (!ret) {
        qInfo() << err;
        info->setcode(RetCode(RetCode::load_pkg_data_failed));
        info->setmessage(err);
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }

    // 更新本地数据库文件 to do
    updateAppStatus(appStreamPkgInfo);
    info->setcode(RetCode(RetCode::pkg_install_success));
    info->setmessage("install " + pkgName + ",version:" + appStreamPkgInfo.appVer + " success");
    info->setstate(true);
    retMsg.push_back(info);
    return retMsg;
}

/*!
 * 查询软件包
 * @param packageIDList: 软件包的appid
 *
 * @return PKGInfoList 查询结果列表
 */
PKGInfoList PackageManagerImpl::Query(const QStringList &packageIDList, const ParamStringMap &paramMap)
{
    const QString pkgName = packageIDList.at(0);
    if (pkgName.isNull() || pkgName.isEmpty()) {
        qInfo() << "package name err";
        return {};
    }
    if (pkgName == "installed") {
        return queryAllInstalledApp();
    }
    PKGInfoList pkglist;
    // 查找单个软件包 优先从本地数据库文件中查找
    QString arch = getHostArch();
    if (arch == "unknown") {
        qInfo() << "the host arch is not recognized";
        return pkglist;
    }
    bool ret = getInstalledAppInfo(pkgName, "", arch, pkglist);
    // 目标软件包 已安装则终止查找
    qInfo() << "PackageManager::Query called, ret:" << ret;
    if (ret) {
        return pkglist;
    }
    // 更新AppStream.json后，查找AppStream.json 文件
    QString err = "";
    // 更新AppStream.json
    ret = updateAppStream("/deepin/linglong/", "repo", err);
    if (!ret) {
        qInfo() << err;
        return pkglist;
    }
    // 查询未安装App信息
    ret = getUnInstalledAppInfo(pkgName, "", arch, pkglist, err);
    if (!ret) {
        qInfo() << err;
    }
    return pkglist;
}

/*
 * 卸载操作下更新软件包状态信息
 *
 * @param pkgName: 卸载软件包包名
 * @param pkgVer: 卸载软件包对应的版本
 *
 * @return bool: true:更新成功 false:失败
 */
bool PackageManagerImpl::updateUninstallAppStatus(const QString &pkgName, const QString &pkgVer)
{
    // file lock to do
    // 数据库的文件路径
    QString dbPath = "/deepin/linglong/layers/AppInfoDB.json";
    if (!linglong::util::fileExists(dbPath)) {
        qCritical() << "db file not exist!";
        return false;
    }
    QFile dbFile(dbPath);
    if (!dbFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << "db file open failed!";
        return false;
    }
    // 读取文件的全部内容
    QString qValue = dbFile.readAll();
    dbFile.close();
    QJsonParseError parseJsonErr;
    QJsonDocument document = QJsonDocument::fromJson(qValue.toUtf8(), &parseJsonErr);
    if (!(parseJsonErr.error == QJsonParseError::NoError)) {
        qCritical() << "updateUninstallAppStatus parse json file err";
        return false;
    }
    // 自定义软件包信息metadata 继承JsonSerialize 来处理，to do fix
    QJsonObject jsonObject = document.object();
    QJsonObject pkgsObject = jsonObject["pkgs"].toObject();
    QJsonObject usersObject = jsonObject["users"].toObject();
    // 删除users 字段下 软件包对应的目录
    QString userName = getUserName();
    QJsonObject userNameObject;
    bool isMulVer = false;
    if (usersObject.contains(userName)) {
        userNameObject = usersObject[userName].toObject();
        // 判断是否有多个版本
        QJsonObject usersPkgObject = userNameObject[pkgName].toObject();
        QJsonValue arrayValue = usersPkgObject.value(QStringLiteral("version"));
        QJsonArray versionArray = arrayValue.toArray();
        isMulVer = versionArray.size() > 1 ? true : false;
        if (isMulVer) {
            for (int i = 0; i < versionArray.size(); i++) {
                QString item = versionArray.at(i).toString();
                if (item == pkgVer) {
                    versionArray.removeAt(i);
                    usersPkgObject["version"] = versionArray;
                    userNameObject[pkgName] = usersPkgObject;
                    break;
                }
            }
        } else {
            userNameObject.remove(pkgName);
        }
        usersObject[userName] = userNameObject;
    }

    // 更新pkgs字段中的用户列表users字段
    QJsonObject pkgObject = pkgsObject[pkgName].toObject();
    QJsonArray pkgUsersObject = pkgObject["users"].toArray();
    if (!isMulVer) {
        for (int i = 0; i < pkgUsersObject.size(); i++) {
            QString item = pkgUsersObject.at(i).toString();
            if (item == userName) {
                pkgUsersObject.removeAt(i);
                pkgObject["users"] = pkgUsersObject;
                pkgsObject[pkgName] = pkgObject;
                break;
            }
        }
    }

    // 软件包无用户安装 删除对应的记录
    if (pkgUsersObject.isEmpty()) {
        pkgsObject.remove(pkgName);
    }

    // users 下面的用户未安装软件，删除对应的用户记录
    if (userNameObject.isEmpty()) {
        usersObject.remove(userName);
    }

    jsonObject.insert("pkgs", pkgsObject);
    jsonObject.insert("users", usersObject);

    document.setObject(jsonObject);
    dbFile.open(QIODevice::WriteOnly | QIODevice::Text);
    // 将修改后的内容写入文件
    QTextStream wirteStream(&dbFile);
    // 设置编码UTF8
    wirteStream.setCodec("UTF-8");
    wirteStream << document.toJson();
    dbFile.close();
    return true;
}

/*
 * 卸载软件包
 *
 * @param packageIDList: 软件包的appid
 *
 * @return RetMessageList 卸载结果信息
 */
RetMessageList PackageManagerImpl::Uninstall(const QStringList &packageIDList, const ParamStringMap &paramMap)
{
    RetMessageList retMsg;
    auto info = QPointer<RetMessage>(new RetMessage);
    const QString pkgName = packageIDList.at(0);

    // 获取版本信息
    QString version = "";
    if (!paramMap.empty() && paramMap.contains(linglong::util::KEY_VERSION)) {
        version = paramMap[linglong::util::KEY_VERSION];
    }

    // 判断是否已安装
    if (!getInstallStatus(pkgName, version)) {
        qCritical() << pkgName << " not installed";
        info->setcode(RetCode(RetCode::pkg_not_installed));
        info->setmessage(pkgName + " not installed");
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }
    QString err = "";
    PKGInfoList pkglist;
    // 卸载 删除文件（软链接？）判断是否有用户使用软件包 to do fix
    // 根据已安装文件查询已经安装软件包信息
    QString arch = getHostArch();
    getInstalledAppInfo(pkgName, version, arch, pkglist);
    auto it = pkglist.at(0);
    if (pkglist.size() > 0) {
        const QString installPath = "/deepin/linglong/layers/" + it->appid + "/" + it->version;
        linglong::util::removeDir(installPath);
        qInfo() << "Uninstall del dir:" << installPath;
    }
    // 更新本地repo仓库
    const QString repoPath = "/deepin/linglong/repo";
    bool ret = G_OSTREE_REPOHELPER->ensureRepoEnv(repoPath, err);
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
    ret = G_OSTREE_REPOHELPER->getRemoteRepoList(repoPath, qrepoList, err);
    if (!ret) {
        qCritical() << err;
        info->setcode(RetCode(RetCode::pkg_uninstall_failed));
        info->setmessage("uninstall remote repo not exist");
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }
    // ref format --> app/org.deepin.calculator/x86_64/1.2.2
    QString matchRef = QString("app/%1/%2/%3").arg(it->appid).arg(arch).arg(it->version);
    qInfo() << "Uninstall app ref:" << matchRef;
    ret = G_OSTREE_REPOHELPER->repoDeleteDatabyRef(repoPath, qrepoList[0], matchRef, err);
    if (!ret) {
        qCritical() << err;
        info->setcode(RetCode(RetCode::pkg_uninstall_failed));
        info->setmessage("uninstall " + pkgName + ",version:" + it->version + " failed");
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }

    // 更新安装数据库
    updateUninstallAppStatus(pkgName, it->version);
    info->setcode(RetCode(RetCode::pkg_uninstall_success));
    info->setmessage("uninstall " + pkgName + ",version:" + it->version + " success");
    info->setstate(true);
    retMsg.push_back(info);
    return retMsg;
}