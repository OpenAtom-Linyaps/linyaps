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
#include "service/impl/dbus_retcode.h"
#include "util/singleton.h"

using linglong::util::fileExists;
using linglong::util::listDirFolders;
using linglong::dbus::RetCode;

using linglong::service::util::AppInstance;
using linglong::service::util::AppInfo;

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
    : dd_ptr(new PackageManagerPrivate(this)),app_instance_list(linglong::service::util::AppInstance::get())
{
    AppInfo app_info;
    app_info.appid = "org.test.app1";
    app_info.version = "v0.1";
    this->app_instance_list->AppendAppInstance<AppInfo>(app_info);
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
 * 查询软件包安装状态
 *
 * @param pkgName: 软件包包名
 * @param userName: 用户名，默认为当前用户
 *
 * @return bool: 1:已安装 0:未安装 -1查询失败
 */
int PackageManager::getIntallStatus(const QString pkgName, const QString userName)
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
        qCritical() << "getIntallStatus parse json file err";
        return -1;
    }
    QJsonObject jsonObject = document.object();
    QJsonObject usersObject = jsonObject["users"].toObject();
    // 用户名存在
    if (usersObject.contains(dstUserName)) {
        QJsonObject userNameObject = usersObject[dstUserName].toObject();
        // 包名存在
        if (userNameObject.contains(pkgName)) {
            return 1;
        }
    }
    return 0;
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
 * @param savePath: 解压后文件存储路径
 * @param err: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
bool PackageManager::extractOUAP(const QString ouapPath, const QString savePath, QString &err)
{
    Package create_package;
    bool ret = create_package.Extract(ouapPath, savePath);
    if (!ret) {
        err = "extractOUAP error";
        return ret;
    }
    ret = create_package.GetInfo(ouapPath, savePath);
    if (!ret) {
        err = "get ouap info error";
    }
    return ret;
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
    // QString pkgName = "org.deepin.calculator";
    // QString pkgName = "us.zoom.Zoom";
    // QString arch = "x86_64";
    ret = repo.resolveMatchRefs(repoPath, qrepoList[0], pkgName, pkgArch, matchRef, err);
    if (!ret) {
        qInfo() << err;
        return false;
    } else {
        qInfo() << matchRef;
    }

    //ret = repo.repoPull(repoPath, qrepoList[0], pkgName, err);
    ret = repo.repoPullbyCmd(repoPath, qrepoList[0], matchRef, err);
    if (!ret) {
        qInfo() << err;
        return false;
    }
    // checkout 目录
    // const QString dstPath = repoPath + "/AppData";
    ret = repo.checkOutAppData(repoPath, qrepoList[0], matchRef, dstPath, err);
    if (!ret) {
        qInfo() << err;
    }
    qInfo() << "downloadOUAPData success, path:" << dstPath;
    // 方案2 将数据checkout到临时目录，临时目录制作一个离线包，再调用离线包的api安装
    // makeUAPbyOUAP(QString cfgPath, QString dstPath)
    // Package pkg;
    // pkg.InitUapFromFile(uapPath);
    return ret;
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
    // QString savePath = "/deepin/linglong/layers/" + appId + "/" + appVer;
    // linglong::util::createDir(savePath);
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
        err = "updateOUAP load ouap err";
    }
    httpClient->release();
    return ret;
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
    // deepin/linglong
    QString fullPath = savePath + remoteName + "/AppStream.json";
    if (!linglong::util::fileExists(fullPath)) {
        err = fullPath + " is not exist";
        return false;
    }

    QFile jsonFile(fullPath);
    jsonFile.open(QIODevice::ReadOnly | QIODevice::Text);
    // auto qbt = jsonFile.readAll();
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
    err = "app:" + pkgName + " not found";
    return false;
}

/*!
 * 下载软件包
 * @param packageIDList
 */
RetMessageList PackageManager::Download(const QStringList &packageIDList, const QString savePath)
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
    RetMessageList retMsg;
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

    // 下载OUAP 在线包到指定目录
    QString err = "";
    // 更新AppStream.json
    bool ret = updateAppStream("/deepin/linglong/", "repo", err);
    if (!ret) {
        qInfo() << err;
        info->setcode(RetCode(RetCode::update_appstream_failed));
        info->setmessage(err);
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }
    // 根据AppStream.json 查找目标软件包
    const QString xmlPath = "/deepin/linglong/repo/AppStream.json";
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
    ret = getAppInfoByAppStream("/deepin/linglong/", "repo", pkgName, arch, err);
    if (!ret) {
        qInfo() << err;
        info->setcode(RetCode(RetCode::search_pkg_by_appstream_failed));
        info->setmessage(err);
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }

    // 下载OUAP在线包到指定目录
    ret = updateOUAP(xmlPath, savePath, err);
    if (!ret) {
        qInfo() << err;
        info->setcode(RetCode(RetCode::load_ouap_file_failed));
        info->setmessage(err);
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }

    // 下载UAP离线包to do
    info->setcode(RetCode(RetCode::ouap_download_success));
    info->setmessage("download ouap success");
    info->setstate(true);
    retMsg.push_back(info);
    qInfo() << "download ouap success savePath:" << savePath;
    return retMsg;
}

/*
 * 更新应用安装状态到本地文件
 *
 * @param appStreamPkgInfo: 安装成功的软件包信息
 *
 * @return bool: true:成功 false:失败
 */
bool PackageManager::updateAppStatus(AppStreamPkgInfo appStreamPkgInfo)
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
    usersSubItem.insert("version", appStreamPkgInfo.appVer);
    usersSubItem.insert("ref", "app:" + appStreamPkgInfo.appId);
    // usersSubItem.insert("commitv", "0123456789");
    QJsonObject userItem;
    userItem.insert(appStreamPkgInfo.appId, usersSubItem);
    // users下用户名已存在,将软件包添加到该用户名对应的软件包列表中
    QJsonObject userNameObject;
    if (usersObject.contains(userName)) {
        userNameObject = usersObject[userName].toObject();
        userNameObject.insert(appStreamPkgInfo.appId, usersSubItem);
        usersObject[userName] = userNameObject;
    } else {
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

/*
 * 查询已安装软件包信息
 *
 * @param pkgName: 软件包包名
 * @param pkgList: 查询结果
 *
 * @return bool: true:成功 false:失败(软件包未安装)
 */
bool PackageManager::getInstalledAppInfo(const QString pkgName, PKGInfoList &pkgList)
{
    if (!getIntallStatus(pkgName)) {
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

    QJsonObject usersObject = jsonObject["users"].toObject();
    const QString dstUserName = getUserName();
    QJsonObject userNameObject = usersObject[dstUserName].toObject();
    QJsonObject usersPkgObject = userNameObject[pkgName].toObject();
    QString appVer = usersPkgObject["version"].toString();
    qInfo() << appName << " " << appVer << " " << appSummary << " " << appRuntime << " " << appRepo;

    auto info = QPointer<PKGInfo>(new PKGInfo);
    info->appid = pkgName;
    info->appname = appName;
    info->version = appVer;
    info->arch = getHostArch();
    info->description = appSummary;
    pkgList.push_back(info);
    return true;
}

/*
 * 查询未安装软件包信息
 *
 * @param pkgName: 软件包包名
 * @param pkgList: 查询结果
 * @param err: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
bool PackageManager::getUnInstalledAppInfo(const QString pkgName, PKGInfoList &pkgList, QString &err)
{
    const QString arch = getHostArch();
    if (arch == "unknown") {
        qInfo() << "the host arch is not recognized";
        return false;
    }
    bool ret = getAppInfoByAppStream("/deepin/linglong/", "repo", pkgName, arch, err);
    if (ret) {
        qInfo() << appStreamPkgInfo.appName << " " << appStreamPkgInfo.appId << " " << appStreamPkgInfo.summary;
        auto info = QPointer<PKGInfo>(new PKGInfo);
        info->appid = pkgName;
        info->appname = appStreamPkgInfo.appName;
        info->version = appStreamPkgInfo.appVer;
        info->arch = appStreamPkgInfo.appArch;
        info->description = appStreamPkgInfo.summary;
        pkgList.push_back(info);
    }
    return ret;
}

/*
 * 建立box运行应用需要的软链接
 */
void PackageManager::buildRequestedLink()
{
    // TODO: fix it ( will remove this )
    QString yaml_path = "/deepin/linglong/layers/" + appStreamPkgInfo.appId + "/"
                        + appStreamPkgInfo.appVer + "/" + appStreamPkgInfo.appArch + "/" + appStreamPkgInfo.appId + ".yaml";
    QString new_path = "/deepin/linglong/layers/" + appStreamPkgInfo.appId + "/"
                       + appStreamPkgInfo.appVer + "/" + appStreamPkgInfo.appId + ".yaml";
    if (linglong::util::fileExists(yaml_path)) {
        // link to file
        linglong::util::linkFile(yaml_path, new_path);
    }
    // TODO: fix it ( will remove this )
    QString info_path = "/deepin/linglong/layers/" + appStreamPkgInfo.appId + "/"
                        + appStreamPkgInfo.appVer + "/" + appStreamPkgInfo.appArch + "/info.json";
    QString info_new_path = "/deepin/linglong/layers/" + appStreamPkgInfo.appId + "/"
                            + appStreamPkgInfo.appVer + "/" + appStreamPkgInfo.appArch + "/info";

    if (linglong::util::fileExists(info_path)) {
        // link to file
        linglong::util::linkFile(info_path, info_new_path);
    }
}

/*
 * 根据OUAP在线包解压出来的uap文件查询软件包信息
 *
 * @param fileDir: OUAP在线包文件解压后的uap文件存储路径
 * @param err: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
bool PackageManager::getAppInfoByOUAPFile(const QString fileDir, QString &err)
{
    // uap-1 文件路径
    QString uapPath = fileDir + "uap-1";
    if (!linglong::util::fileExists(uapPath)) {
        err = uapPath + " not exist";
        return false;
    }

    QFile uapFile(uapPath);
    if (!uapFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << "uap-1 file open failed!";
        return false;
    }
    // 读取文件的全部内容
    QString qValue = uapFile.readAll();
    uapFile.close();
    QJsonParseError parseJsonErr;
    QJsonDocument document = QJsonDocument::fromJson(qValue.toUtf8(), &parseJsonErr);
    if (!(parseJsonErr.error == QJsonParseError::NoError)) {
        qCritical() << "getAppInfoByOUAPFile parse json file err";
        return false;
    }
    QJsonObject jsonObject = document.object();
    if (jsonObject.contains("appid")) {
        appStreamPkgInfo.appId = jsonObject["appid"].toString();
    }
    if (jsonObject.contains("name")) {
        appStreamPkgInfo.appName = jsonObject["name"].toString();
    }
    if (jsonObject.contains("version")) {
        appStreamPkgInfo.appVer = jsonObject["version"].toString();
    }
    if (jsonObject.contains("runtime")) {
        appStreamPkgInfo.runtime = jsonObject["runtime"].toString();
    }
    appStreamPkgInfo.appArch = getHostArch();
    if (jsonObject.contains("description")) {
        appStreamPkgInfo.summary = jsonObject["description"].toString();
    }
    appStreamPkgInfo.reponame = "localApp";
    return true;
}

/*
 * 根据OUAP在线包文件安装软件包
 *
 * @param filePath: OUAP在线包文件路径
 * @param err: 错误信息
 *
 * @return bool: true:成功 false:失败
 */
bool PackageManager::installOUAPFile(const QString filePath, QString &err)
{
    bool ret = false;
    QDir temDir(filePath);
    QString absPath = temDir.absolutePath();
    qInfo() << absPath;
    if (!linglong::util::fileExists(absPath)) {
        return ret;
    }
    const QString tmpPath = "/tmp/linglong/";
    // 解压OUAP
    ret = extractOUAP(absPath, tmpPath, err);
    if (!ret) {
        qInfo() << err;
        return ret;
    }
    ret = getAppInfoByOUAPFile(tmpPath, err);
    if (!ret) {
        qInfo() << err;
        return ret;
    }

    // 判断软件包是否安装 to do

    const QString savePath = "/deepin/linglong/layers/" + appStreamPkgInfo.appId + "/" + appStreamPkgInfo.appVer + "/" + appStreamPkgInfo.appArch;
    // 创建路径
    linglong::util::createDir(savePath);
    ret = downloadOUAPData(appStreamPkgInfo.appId, appStreamPkgInfo.appArch, savePath, err);
    if (!ret) {
        qInfo() << err;
        return ret;
    }
    // 根据box的安装要求建立软链接
    buildRequestedLink();
    // 更新本地数据库文件 to do
    updateAppStatus(appStreamPkgInfo);
    return ret;
}

/*!
 * 在线安装软件包
 * @param packageIDList
 */
RetMessageList PackageManager::Install(const QStringList &packageIDList)
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
    // 判断是否为UAP离线包
    QStringList uap_list = packageIDList.filter(QRegExp("^*\\.uap$", Qt::CaseInsensitive));
    if (uap_list.size() > 0) {
        qInfo() << uap_list;
        for (auto it : uap_list) {
            Package pkg;
            pkg.InitUapFromFile(it);
        }
        // prompt msg to do
        info->setcode(RetCode(RetCode::uap_install_success));
        info->setmessage("install uap success");
        info->setstate(true);
        retMsg.push_back(info);
        return retMsg;
    }
    QString err = "";
    Qt::CaseSensitivity cs = Qt::CaseInsensitive;
    // 根据OUAP在线包文件安装OUAP在线包 to do
    if (pkgName.endsWith(".ouap", cs)) {
        ret = installOUAPFile(pkgName, err);
        info->setcode(RetCode(RetCode::ouap_install_success));
        info->setmessage("install ouap success");
        if (!ret) {
            info->setcode(RetCode(RetCode::ouap_install_failed));
            info->setmessage(err);
        }
        info->setstate(ret);
        retMsg.push_back(info);
        return retMsg;
    }

    // 判断是否已安装
    if (getIntallStatus(pkgName)) {
        qInfo() << pkgName << " already installed";
        info->setcode(RetCode(RetCode::pkg_already_installed));
        info->setmessage(pkgName + " already installed");
        info->setstate(true);
        retMsg.push_back(info);
        return retMsg;
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
    ret = getAppInfoByAppStream("/deepin/linglong/", "repo", pkgName, arch, err);
    if (!ret) {
        qInfo() << err;
        info->setcode(RetCode(RetCode::search_pkg_by_appstream_failed));
        info->setmessage(err);
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }

    // 下载OUAP 到指定目录
    // const QString tmpDir = "/tmp/linglong/";
    const QString savePath = "/deepin/linglong/layers/" + appStreamPkgInfo.appId + "/" + appStreamPkgInfo.appVer + "/" + appStreamPkgInfo.appArch;
    ret = updateOUAP(xmlPath, savePath, err);
    if (!ret) {
        qInfo() << err;
        info->setcode(RetCode(RetCode::load_ouap_file_failed));
        info->setmessage(err);
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }
    // 解压OUAP 到指定目录
    const QString ouapName = appStreamPkgInfo.appId + "-" + appStreamPkgInfo.appVer + "-" + appStreamPkgInfo.appArch + ".ouap";
    ret = extractOUAP(savePath + "/" + ouapName, savePath, err);
    if (!ret) {
        qInfo() << err;
        info->setcode(RetCode(RetCode::extract_ouap_failed));
        info->setmessage(err);
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }
    // 解析OUAP 在线包中的配置信息
    const QString infoPath = savePath + "/" + ouapName + ".info";
    resolveOUAPCfg(infoPath);

    // 下载OUAP 在线包数据到目标目录 安装完成
    // QString pkgName = "org.deepin.calculator";
    // QString pkgName = "us.zoom.Zoom";
    ret = downloadOUAPData(appStreamPkgInfo.appId, appStreamPkgInfo.appArch, savePath, err);
    if (!ret) {
        qInfo() << err;
        info->setcode(RetCode(RetCode::load_pkg_data_failed));
        info->setmessage(err);
        info->setstate(false);
        retMsg.push_back(info);
        return retMsg;
    }
    // 根据box的安装要求建立软链接
    buildRequestedLink();
    // 更新本地数据库文件 to do
    updateAppStatus(appStreamPkgInfo);
    info->setcode(RetCode(RetCode::pkg_install_success));
    info->setmessage("install " + pkgName + " success");
    info->setstate(true);
    retMsg.push_back(info);
    return retMsg;
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

/*!
 * 查询软件包
 * @param packageIDList: 软件包的appid
 *
 * @return PKGInfoList 查询结果列表
 */
PKGInfoList PackageManager::Query(const QStringList &packageIDList)
{
    QString pkgName = packageIDList.at(0);
    if (pkgName.isNull() || pkgName.isEmpty()) {
        qInfo() << "package name err";
        return {};
    }
    PKGInfoList pkglist;
    // 查找单个软件包 优先从本地数据库文件中查找
    bool ret = getInstalledAppInfo(pkgName, pkglist);
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
    ret = getUnInstalledAppInfo(pkgName, pkglist, err);
    if (!ret) {
        qInfo() << err;
    }
    return pkglist;
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