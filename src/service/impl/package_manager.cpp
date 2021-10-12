/*
 * Copyright (c) 2020-2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "module/repo/repohelper.h"
#include "package_manager.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDebug>
#include <QThread>
#include <QProcess>
#include <QJsonArray>
#include <QStandardPaths>
#include <QSysInfo>
#include <module/runtime/app.h>
#include <module/util/fs.h>
#include "job_manager.h"
#include "module/util/httpclient.h"

using linglong::util::fileExists;
using linglong::util::listDirFolders;

class PackageManagerPrivate
{
public:
    explicit PackageManagerPrivate(PackageManager *parent)
        : q_ptr(parent)
    {
    }

    QMap<QString, QPointer<App>> apps;

    PackageManager *q_ptr = nullptr;
};

PackageManager::PackageManager()
    : dd_ptr(new PackageManagerPrivate(this))
{
}

PackageManager::~PackageManager() = default;

/*
 * 查询当前登陆用户名
 *
 * @return QString: 当前登陆用户名
 */
const QString PackageManager::getUserName()
{
    QString userPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QString userName = userPath.section("/", -1, -1);
    return userName;
}

/*
 * 查询系统架构
 *
 * @return QString: 系统架构字符串
 */
const QString PackageManager::getHostArch()
{
    //other CpuArchitecture ie i386 i486 to do fix
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
 * 根据OUAP在线包数据生成对应的离线包
 *
 * @param cfgPath: OUAP在线包数据存储路径
 * @param dstPath: 生成离线包存储路径
 *
 * @return bool: true:成功 false:失败
 */
bool PackageManager::makeUAPbyOUAP(QString cfgPath, QString dstPath)
{
    Package create_package;
    if (!create_package.InitUap(cfgPath, dstPath)) {
        return false;
    }
    create_package.MakeUap();
    return true;
}

/*
 * 将OUAP在线包解压到指定目录
 *
 * @param ouapPath: ouap在线包存储路径
 * @param ouapName: ouapName在线包名称
 * @param savePath: 解压后文件存储路径
 * @param err: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
bool PackageManager::extractOUAP(const QString ouapPath, QString ouapName, const QString savePath, QString &err)
{
    Package create_package;
    QString fullPath = ouapPath + "/" + ouapName;
    create_package.Extract(fullPath, savePath);
    create_package.GetInfo(fullPath, savePath);
    return true;
}

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
bool PackageManager::downloadOUAPData(const QString pkgName, const QString pkgArch, const QString dstPath, QString &err)
{
    linglong::RepoHelper repo;
    const QString repoPath = "/deepin/linglong/repo";
    bool ret = repo.ensureRepoEnv(repoPath, err);
    if (!ret) {
        qInfo() << err;
        return false;
    }
    QVector<QString> qrepoList;
    ret = repo.getRemoteRepoList(repoPath, qrepoList, err);
    if (!ret) {
        qInfo() << err;
        return false;
    } else {
        for (auto iter = qrepoList.begin(); iter != qrepoList.end(); ++iter) {
            qInfo() << *iter;
        }
    }
    QMap<QString, QString> outRefs;
    ret = repo.getRemoteRefs(repoPath, qrepoList[0], outRefs, err);
    if (!ret) {
        qInfo() << err;
        return false;
    } else {
        // for (auto iter = outRefs.begin(); iter != outRefs.end(); ++iter) {
        //     std::cout << "ref:" << iter.key().toStdString() << ", commit value:" << iter.value().toStdString() << endl;
        // }
    }
    QString matchRef = "";
    //QString pkgName = "org.deepin.calculator";
    //QString pkgName = "us.zoom.Zoom";
    //QString arch = "x86_64";
    ret = repo.resolveMatchRefs(repoPath, qrepoList[0], pkgName, pkgArch, matchRef, err);
    if (!ret) {
        qInfo() << err;
    } else {
        qInfo() << matchRef;
    }
    // bug 目前只能下载到临时目录 待解决 flatpak是ok的
    ret = repo.repoPull(repoPath, qrepoList[0], pkgName, err);
    if (!ret) {
        qInfo() << err;
        return false;
    }
    // checkout 目录
    //const QString dstPath = repoPath + "/AppData";
    ret = repo.checkOutAppData(repoPath, qrepoList[0], matchRef, dstPath, err);
    if (!ret) {
        qInfo() << err;
    }
    // 方案2 将数据checkout到临时目录，临时目录制作一个离线包，再调用离线包的api安装
    //makeUAPbyOUAP(QString cfgPath, QString dstPath)
    //Package pkg;
    //pkg.InitUapFromFile(uapPath);
    return true;
}

/*
 * 解析OUAP在线包中的info.json配置信息
 *
 * @param infoPath: info.json文件存储路径
 *
 * @return bool: true:成功 false:失败
 */
bool PackageManager::resolveOUAPCfg(const QString infoPath)
{
    QFile jsonFile(infoPath);
    jsonFile.open(QIODevice::ReadOnly | QIODevice::Text);
    QString qValue = jsonFile.readAll();
    jsonFile.close();
    QJsonParseError parseJsonErr;
    QJsonDocument document = QJsonDocument::fromJson(qValue.toUtf8(), &parseJsonErr);
    if (!(parseJsonErr.error == QJsonParseError::NoError)) {
        qCritical() << "resolveOUAPCfg parse json file err";
        return false;
    }
    QJsonObject jsonObject = document.object();
    QString appId = jsonObject["appid"].toString();
    QString appName = jsonObject["name"].toString();
    QString appVer = jsonObject["version"].toString();
    return true;
}

/*
 * 通过AppStream.json更新OUAP在线包
 *
 * @param xmlPath: AppStream.json文件存储路径
 * @param savePath: OUAP在线包存储路径
 * @param err: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
bool PackageManager::updateOUAP(const QString xmlPath, const QString savePath, QString &err)
{
    //创建应用目录
    //QString savePath = "/deepin/linglong/layers/" + appId + "/" + appVer;
    //linglong::util::createDir(savePath);
    QDir temDir(savePath);
    QString absDir = temDir.absolutePath();
    // 路径必须是绝对路径
    linglong::util::createDir(absDir);
    qInfo() << absDir;
    // 下载OUAP包
    // OUAP包的命名规则  name + 版本号 + 架构
    const QString ouapName = appStreamPkgInfo.appId + "-" + appStreamPkgInfo.appVer + "-" + appStreamPkgInfo.appArch + ".ouap";
    linglong::util::HttpClient *httpClient = linglong::util::HttpClient::getInstance();
    bool ret = httpClient->loadHttpData(appStreamPkgInfo.appUrl + ouapName, absDir);
    if (!ret) {
        qInfo() << err;
    }
    httpClient->release();
    return true;
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
bool PackageManager::updateAppStream(const QString savePath, const QString remoteName, QString &err)
{
    // 获取AppStream.json 地址
    const QString xmlPath = "http://10.20.54.2/linglong/xml/AppStream.json";
    ///deepin/linglong
    QString fullPath = savePath + remoteName;
    //创建下载目录
    linglong::util::createDir(fullPath);
    linglong::util::HttpClient *httpClient = linglong::util::HttpClient::getInstance();
    bool ret = httpClient->loadHttpData(xmlPath, fullPath);
    httpClient->release();
    return ret;
}

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
bool PackageManager::getAppInfoByAppStream(const QString savePath, const QString remoteName, const QString pkgName, const QString pkgArch, QString &err)
{
    //判断文件是否存在
    //deepin/linglong
    QString fullPath = savePath + remoteName + "/AppStream.json";
    if (!linglong::util::fileExists(fullPath)) {
        err = fullPath + " is not exist";
        return false;
    }

    QFile jsonFile(fullPath);
    jsonFile.open(QIODevice::ReadOnly | QIODevice::Text);
    //auto qbt = jsonFile.readAll();
    QString qValue = jsonFile.readAll();
    jsonFile.close();
    QJsonParseError parseJsonErr;
    QJsonDocument document = QJsonDocument::fromJson(qValue.toUtf8(), &parseJsonErr);
    if (!(parseJsonErr.error == QJsonParseError::NoError)) {
        qCritical() << "getAppInfoByAppStream parse json file wrong";
        err = fullPath + " json file wrong";
        return false;
    }
    if (document.isArray()) {
        QJsonArray array = document.array();
        int nSize = array.size();
        for (int i = 0; i < nSize; i++) {
            QJsonValue sub = array.at(i);
            QJsonObject subObj = sub.toObject();
            QString appId = subObj["appid"].toString();
            QString appName = subObj["name"].toString();
            QString appVer = subObj["version"].toString();
            QString appUrl = subObj["appUrl"].toString();

            QString summary = subObj["summary"].toString();
            QString runtime = subObj["runtime"].toString();
            QString reponame = subObj["reponame"].toString();
            QString arch = "";
            QJsonValue arrayValue = subObj.value(QStringLiteral("arch"));
            if (arrayValue.isArray()) {
                QJsonArray arr = arrayValue.toArray();
                arch = arr.at(0).toString();
                // int nSize = arr.size();
                // for (int i = 0; i < nSize; i++) {
                //     QJsonValue item = arr.at(i);
                //     if (item.isString()) {
                //         arch = item.toString();
                //     }
                // }
            }
            if (pkgName == appId && arch == pkgArch) {
                appStreamPkgInfo.appId = appId;
                appStreamPkgInfo.appName = appName;
                appStreamPkgInfo.appVer = appVer;
                appStreamPkgInfo.appArch = arch;
                appStreamPkgInfo.appUrl = appUrl;

                appStreamPkgInfo.summary = summary;
                appStreamPkgInfo.runtime = runtime;
                appStreamPkgInfo.reponame = reponame;
                return true;
            }
        }
    }
    err = "getAppInfoByAppStream app:" + pkgName + " is not found";
    return false;
}

/*!
 * 下载软件包
 * @param packageIDList
 */
QString PackageManager::Download(const QStringList &packageIDList, const QString savePath)
{
    Q_D(PackageManager);

    // return JobManager::instance()->CreateJob([](Job *jr) {
    //     在这里写入真正的实现
    //     QProcess p;
    //     p.setProgram("curl");
    //     p.setArguments({"https://www.baidu.com"});
    //     p.start();
    //     p.waitForStarted();
    //     p.waitForFinished(-1);
    //     qDebug() << p.readAllStandardOutput();
    //     qDebug() << "finish" << p.exitStatus() << p.state();
    // });
    qInfo() << "PackageManager::Download called";

    QString pkgName = packageIDList.at(0);
    if (pkgName.isNull() || pkgName.isEmpty()) {
        qInfo() << "package name err";
        return "";
    }

    // 下载OUAP 在线包到指定目录
    QString err = "";
    // 更新AppStream.json
    updateAppStream("/deepin/linglong/", "repo", err);

    // 根据AppStream.json 查找目标软件包
    const QString xmlPath = "/deepin/linglong/repo/AppStream.json";
    //const QString arch = "x86_64";
    const QString arch = getHostArch();
    if (arch == "unknown") {
        qInfo() << "the host arch is not recognized";
        return "";
    }
    bool ret = getAppInfoByAppStream("/deepin/linglong/", "repo", pkgName, arch, err);
    if (!ret) {
        qInfo() << err;
        return "";
    }

    // 下载OUAP在线包到指定目录
    updateOUAP(xmlPath, savePath, err);

    // 下载UAP离线包to do
    return "";
}

/*!
 * 在线安装软件包
 * @param packageIDList
 */
QString PackageManager::Install(const QStringList &packageIDList)
{
    Q_D(PackageManager);

    // return JobManager::instance()->CreateJob([](Job *jr) {
    //     // 在这里写入真正的实现
    //     QProcess p;
    //     p.setProgram("curl");
    //     p.setArguments({"https://www.baidu.com"});
    //     p.start();
    //     p.waitForStarted();
    //     p.waitForFinished(-1);
    //     qDebug() << p.readAllStandardOutput();
    //     qDebug() << "finish" << p.exitStatus() << p.state();
    // });

    QString pkgName = packageIDList.at(0);
    if (pkgName.isNull() || pkgName.isEmpty()) {
        qInfo() << "package name err";
        return "";
    }
    QString retInfo = "Install success";
    // 判断是否为UAP离线包
    QStringList uap_list = packageIDList.filter(QRegExp("^*.uap$", Qt::CaseInsensitive));
    if (uap_list.size() > 0) {
        qInfo() << uap_list;
        for (auto it : uap_list) {
            Package pkg;
            pkg.InitUapFromFile(it);
        }
        return retInfo;
    }

    // 根据OUAP在线包文件安装OUAP在线包 to do

    // 判断是否已安装to do

    // 根据包名安装在线包
    QString err = "";
    // 更新AppStream.json
    bool ret = updateAppStream("/deepin/linglong/", "repo", err);
    if (!ret) {
        qInfo() << err;
        return "";
    }
    // 根据AppStream.json 查找目标软件包
    const QString xmlPath = "/deepin/linglong/repo/AppStream.json";
    // const QString arch = "x86_64";
    const QString arch = getHostArch();
    if (arch == "unknown") {
        qInfo() << "the host arch is not recognized";
        return "";
    }
    ret = getAppInfoByAppStream("/deepin/linglong/", "repo", pkgName, arch, err);
    if (!ret) {
        qInfo() << err;
        return "";
    }

    // 下载OUAP 到指定目录
    //const QString tmpDir = "/tmp/linglong/";
    const QString savePath = "/deepin/linglong/layers/" + appStreamPkgInfo.appId + "/" + appStreamPkgInfo.appVer + "/" + appStreamPkgInfo.appArch;
    updateOUAP(xmlPath, savePath, err);

    // 解压OUAP 到指定目录
    const QString ouapName = appStreamPkgInfo.appId + "-" + appStreamPkgInfo.appVer + "-" + appStreamPkgInfo.appArch + ".ouap";
    extractOUAP(savePath, ouapName, savePath, err);

    // 解析OUAP 在线包中的配置信息
    const QString infoPath = savePath + "/" + ouapName + ".info";
    resolveOUAPCfg(infoPath);

    // 下载OUAP 在线包数据到目标目录 安装完成
    //QString pkgName = "org.deepin.calculator";
    //QString pkgName = "us.zoom.Zoom";
    downloadOUAPData(appStreamPkgInfo.appId, appStreamPkgInfo.appArch, savePath, err);

    // 更新本地数据库文件 to do
    return retInfo;
}

QString PackageManager::Uninstall(const QStringList &packageIDList)
{
    sendErrorReply(QDBusError::NotSupported, message().member());
    return {};
}

QString PackageManager::Update(const QStringList &packageIDList)
{
    sendErrorReply(QDBusError::NotSupported, message().member());
    return {};
}

QString PackageManager::UpdateAll()
{
    sendErrorReply(QDBusError::NotSupported, message().member());
    return {};
}

PackageList PackageManager::Query(const QStringList &packageIDList)
{
    sendErrorReply(QDBusError::NotSupported, message().member());
    return {};
}

/*!
 * 安装本地软件包
 * @param packagePathList
 */
QString PackageManager::Import(const QStringList &packagePathList)
{
    sendErrorReply(QDBusError::NotSupported, message().member());
    return {};
}

/*!
 * 执行软件包
 * @param packageID 软件包的appid
 */
QString PackageManager::Start(const QString &packageID)
{
    Q_D(PackageManager);

    qDebug() << "start package" << packageID;
    return JobManager::instance()->CreateJob([=](Job *jr) {
        QString config = nullptr;
        if (fileExists("~/.linglong/" + packageID + ".yaml")) {
            config = "~/.linglong/" + packageID + ".yaml";
        } else if (fileExists("/deepin/linglong/layers/" + packageID + "/latest/" + packageID + ".yaml")) {
            config = "/deepin/linglong/layers/" + packageID + "/latest/" + packageID + ".yaml";
        } else {
            auto config_dir = listDirFolders("/deepin/linglong/layers/" + packageID);
            if (config_dir.isEmpty()) {
                qInfo() << "loader:: can not found config file!";
                return;
            }
            if (config_dir.size() >= 2) {
                std::sort(config_dir.begin(), config_dir.end(), [](const QString &s1, const QString &s2) {
                    auto v1 = s1.split("/").last();
                    auto v2 = s2.split("/").last();
                    return v1.toDouble() > v2.toDouble();
                });
            }
            config = config_dir.first() + "/" + packageID + ".yaml";
        }

        qDebug() << "load package" << packageID << " config " << config;

        auto app = App::load(config);
        if (nullptr == app) {
            qCritical() << "nullptr" << app;
            return;
        }
        d->apps[app->container->ID] = QPointer<App>(app);
        app->start();
    });
    //    sendErrorReply(QDBusError::NotSupported, message().member());
}

void PackageManager::Stop(const QString &containerID)
{
    sendErrorReply(QDBusError::NotSupported, message().member());
}

ContainerList PackageManager::ListContainer()
{
    Q_D(PackageManager);
    ContainerList list;

    for (const auto &app : d->apps) {
        auto c = QPointer<Container>(new Container);
        c->ID = app->container->ID;
        c->PID = app->container->PID;
        list.push_back(c);
    }
    return list;
}